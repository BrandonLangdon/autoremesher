"""The remesh operator.

Runs the standalone AutoRemesher CLI as a subprocess and stays responsive: a
modal timer polls the process so Blender's UI never freezes (remeshing a large
mesh can take minutes) and the run can be cancelled with Esc.

The subprocess writes its console output to a log file rather than a pipe. The
CLI is chatty (per-percent progress plus hole-filling messages), and an
undrained pipe would fill the OS buffer and deadlock the child mid-run.
"""

import os
import re
import shutil
import subprocess
import tempfile
import time

import bpy
from bpy.types import Operator

from . import io_obj

# Matches the CLI's headless progress lines, e.g. "90% done. Island 1: solve".
_PROGRESS_RE = re.compile(r"(\d+)% done\.(.*)")


def resolve_binary(path):
    """Resolve a user-provided path to the actual AutoRemesher executable.

    Blender's file browser can select a macOS ``.app`` bundle but cannot descend
    into it, so accept the bundle and dig out ``Contents/MacOS/<exe>``. A direct
    path to the executable is returned unchanged. Returns "" for an empty path.
    """
    if not path:
        return ""
    path = os.path.normpath(bpy.path.abspath(path))

    # Already the executable.
    if os.path.isfile(path):
        return path

    # A macOS .app bundle (a directory, usually ending in .app).
    macos_dir = os.path.join(path, "Contents", "MacOS")
    if os.path.isdir(macos_dir):
        # Prefer the executable named after the bundle (autoremesher.app -> autoremesher).
        candidate = os.path.join(macos_dir, os.path.splitext(os.path.basename(path))[0])
        if os.path.isfile(candidate):
            return candidate
        # Otherwise take the first regular file in Contents/MacOS.
        for name in sorted(os.listdir(macos_dir)):
            full = os.path.join(macos_dir, name)
            if os.path.isfile(full):
                return full

    # Return as-is; the caller's isfile() check will produce a clear error.
    return path


class MESH_OT_autoremesher(Operator):
    bl_idname = "mesh.autoremesher_remesh"
    bl_label = "Remesh with AutoRemesher"
    bl_description = "Quad-remesh the active object using the AutoRemesher CLI"
    bl_options = {"REGISTER", "UNDO"}

    _timer = None
    _proc = None
    _log_file = None
    _tmpdir = None
    _in_path = None
    _out_path = None
    _log_path = None
    _obj_name = ""
    _start = 0.0
    _log_pos = 0
    _percent = 0
    _stage = ""

    @classmethod
    def poll(cls, context):
        settings = getattr(context.scene, "autoremesher", None)
        obj = (settings.target if settings else None) or context.active_object
        return obj is not None and obj.type == "MESH"

    def invoke(self, context, event):
        prefs = context.preferences.addons[__package__].preferences
        binary = resolve_binary(prefs.autoremesher_path)
        if not binary or not os.path.isfile(binary):
            self.report({"ERROR"}, "Set a valid AutoRemesher binary or .app in Add-on Preferences")
            return {"CANCELLED"}

        settings = context.scene.autoremesher
        obj = settings.target or context.active_object
        if obj is None or obj.type != "MESH":
            self.report({"ERROR"}, "Select a mesh object (or set one in the Object field)")
            return {"CANCELLED"}
        self._obj_name = obj.name
        self._log_pos = 0
        self._percent = 0
        self._stage = ""

        self._tmpdir = tempfile.mkdtemp(prefix="autoremesher_")
        self._in_path = os.path.join(self._tmpdir, "input.obj")
        self._out_path = os.path.join(self._tmpdir, "output.obj")
        self._log_path = os.path.join(self._tmpdir, "log.txt")

        # Export the selection to OBJ (local space; modifiers optional).
        try:
            depsgraph = context.evaluated_depsgraph_get()
            io_obj.object_to_obj(obj, depsgraph, self._in_path, settings.apply_modifiers)
        except Exception as exc:  # noqa: BLE001 - surface any export failure to the user
            self.report({"ERROR"}, "Export failed: %s" % exc)
            self._cleanup()
            return {"CANCELLED"}

        cmd = [
            binary,
            "--input", self._in_path,
            "--output", self._out_path,
            "--target-quads", str(settings.target_quads),
            "--edge-scaling", "%.4f" % settings.edge_scaling,
            "--sharp-edge", "%.2f" % settings.sharp_edge,
            "--smooth-normal", "%.2f" % settings.smooth_normal,
            "--adaptivity", "%.4f" % settings.adaptivity,
        ]
        env = os.environ.copy()
        if settings.use_ftetwild:
            ftw = bpy.path.abspath(prefs.ftetwild_path) if prefs.ftetwild_path else ""
            if ftw and os.path.isfile(ftw):
                cmd.append("--use-ftetwild")
                env["AUTOREMESHER_FTETWILD"] = ftw
            else:
                self.report({"WARNING"}, "fTetWild path not set/invalid; using the built-in remesher")

        try:
            self._log_file = open(self._log_path, "wb")
            self._proc = subprocess.Popen(cmd, stdout=self._log_file, stderr=subprocess.STDOUT, env=env)
        except Exception as exc:  # noqa: BLE001
            self.report({"ERROR"}, "Failed to launch AutoRemesher: %s" % exc)
            self._cleanup()
            return {"CANCELLED"}

        self._start = time.time()
        wm = context.window_manager
        self._timer = wm.event_timer_add(0.3, window=context.window)
        wm.modal_handler_add(self)
        context.workspace.status_text_set("AutoRemesher: running…  (Esc to cancel)")
        return {"RUNNING_MODAL"}

    def modal(self, context, event):
        if event.type == "ESC":
            return self._finish(context, cancelled=True)
        if event.type == "TIMER":
            returncode = self._proc.poll()
            if returncode is None:
                self._poll_progress()
                context.workspace.status_text_set(self._status_line())
                return {"RUNNING_MODAL"}
            return self._finish(context, cancelled=False, returncode=returncode)
        return {"PASS_THROUGH"}

    def _poll_progress(self):
        """Read new log output and update the last-seen percent and stage text."""
        try:
            with open(self._log_path, "r", encoding="utf-8", errors="ignore") as f:
                f.seek(self._log_pos)
                chunk = f.read()
                self._log_pos = f.tell()
        except Exception:  # noqa: BLE001
            return
        for match in _PROGRESS_RE.finditer(chunk):
            self._percent = int(match.group(1))
            stage = match.group(2).strip()
            if stage:
                self._stage = stage

    def _status_line(self):
        elapsed = int(time.time() - self._start)
        if self._percent:
            head = "AutoRemesher: %d%%" % self._percent
            if self._stage:
                head += " — %s" % self._stage
        else:
            head = "AutoRemesher: running…"
        return "%s   %ds  (Esc to cancel)" % (head, elapsed)

    def _finish(self, context, cancelled, returncode=None):
        wm = context.window_manager
        if self._timer is not None:
            wm.event_timer_remove(self._timer)
            self._timer = None
        context.workspace.status_text_set(None)

        if cancelled:
            if self._proc is not None and self._proc.poll() is None:
                self._proc.terminate()
                try:
                    self._proc.wait(timeout=5)
                except Exception:  # noqa: BLE001
                    self._proc.kill()
            self._cleanup()
            self.report({"INFO"}, "AutoRemesher cancelled")
            return {"CANCELLED"}

        if self._log_file is not None:
            self._log_file.close()
            self._log_file = None

        if returncode != 0 or not os.path.isfile(self._out_path):
            tail = self._log_tail()  # read before cleanup removes the log
            self._cleanup()
            self.report({"ERROR"}, "AutoRemesher failed (exit %s)%s" % (returncode, tail))
            return {"CANCELLED"}

        try:
            vertices, faces = io_obj.read_obj(self._out_path)
        except Exception as exc:  # noqa: BLE001
            self._cleanup()
            self.report({"ERROR"}, "Failed to read result: %s" % exc)
            return {"CANCELLED"}

        if not vertices or not faces:
            self._cleanup()
            self.report({"ERROR"}, "AutoRemesher produced an empty mesh")
            return {"CANCELLED"}

        elapsed = int(time.time() - self._start)
        self._apply_result(context, vertices, faces)
        self._cleanup()
        self.report({"INFO"}, "AutoRemesher: %d faces, %d verts in %ds" % (len(faces), len(vertices), elapsed))
        return {"FINISHED"}

    def _apply_result(self, context, vertices, faces):
        settings = context.scene.autoremesher
        obj = bpy.data.objects.get(self._obj_name)

        new_mesh = bpy.data.meshes.new((self._obj_name or "AutoRemesh") + "_remesh")
        new_mesh.from_pydata(vertices, [], faces)
        new_mesh.update()
        new_mesh.validate()

        if obj is None or settings.output_mode == "NEW":
            new_obj = bpy.data.objects.new((self._obj_name or "AutoRemesh") + "_remesh", new_mesh)
            if obj is not None:
                new_obj.matrix_world = obj.matrix_world.copy()
                for coll in obj.users_collection:
                    coll.objects.link(new_obj)
            else:
                context.scene.collection.objects.link(new_obj)
            for selected in context.selected_objects:
                selected.select_set(False)
            new_obj.select_set(True)
            context.view_layer.objects.active = new_obj
        else:
            # Replace the mesh data in place, preserving the object's transform.
            old_mesh = obj.data
            desired_name = old_mesh.name
            obj.data = new_mesh
            if settings.apply_modifiers:
                # The modifier stack is now baked into the new geometry.
                obj.modifiers.clear()
            if old_mesh.users == 0:
                bpy.data.meshes.remove(old_mesh)
            new_mesh.name = desired_name

    def _cleanup(self):
        if self._log_file is not None:
            try:
                self._log_file.close()
            except Exception:  # noqa: BLE001
                pass
            self._log_file = None
        if self._tmpdir and os.path.isdir(self._tmpdir):
            shutil.rmtree(self._tmpdir, ignore_errors=True)
        self._tmpdir = None
        self._proc = None

    def _log_tail(self, limit=200):
        try:
            with open(self._log_path, "r", encoding="utf-8", errors="ignore") as f:
                text = f.read().strip()
        except Exception:  # noqa: BLE001
            return ""
        if not text:
            return ""
        return ": " + text[-limit:].replace("\n", " ")
