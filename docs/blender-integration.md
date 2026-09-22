# Blender Integration

A Blender add-on that quad-remeshes the selected object with AutoRemesher and
returns the result to Blender — either replacing the object's mesh in place or
adding a new object beside it.

The add-on lives in [`blender/autoremesher_bridge/`](../blender/autoremesher_bridge).

## Design: a subprocess bridge, not a linked library

The add-on is a **thin driver around the existing AutoRemesher CLI**. It never
links the AutoRemesher core into Blender; it shells out to the standalone
executable. This is deliberate:

- **The core stays standalone.** The CLI (`--input/--output` + parameter flags)
  is the integration contract. Nothing about the core changes for Blender.
- **No ABI/build coupling.** The alternative — compiling the core as a Python
  extension for Blender's bundled interpreter — would mean building geogram +
  TBB into a per-platform, per-Blender-version wheel. The only thing that buys
  is skipping a temp-file round-trip (megabytes, milliseconds). Not worth it.
- **Isolation.** A crash or hang in the remesher cannot take Blender down with
  it, and the run is cancelable.

### Data flow

```
Blender (add-on)                          AutoRemesher (separate process)
────────────────                          ───────────────────────────────
active mesh ──► write input.obj  ──────►  --input input.obj --output output.obj
 (local space, tri)                          --target-quads … --edge-scaling …
                                                     │
new/replaced object ◄── read output.obj ◄────────────┘  (quads, same space)
```

## Key decisions

- **Direct OBJ read/write in Python**, not `bpy.ops` import/export — faster,
  version-proof, and it keeps exactly what the remesher needs (positions +
  faces). See [`io_obj.py`](../blender/autoremesher_bridge/io_obj.py).
- **Local-space round-trip.** The mesh is exported in the object's local space
  and the result is imported back into local space; the object's world
  transform is never touched, so position/rotation/scale are preserved for free.
  (The remesher already works in the input's coordinate space — the only
  normalization in the core is display-only.)
- **Async modal operator.** A timer polls the subprocess so Blender's UI stays
  responsive during multi-minute runs, with a live elapsed-time status and Esc
  to cancel (which terminates the child). See
  [`operator.py`](../blender/autoremesher_bridge/operator.py).
- **Console output goes to a log file, not a pipe.** The CLI is chatty; an
  undrained pipe would fill the OS buffer and deadlock the child. The log's tail
  is surfaced on failure.
- **Replace vs. new object.** *Replace In Place* swaps `object.data`, keeping the
  transform and name (and, with Apply Modifiers, clears the now-baked modifier
  stack). *New Object* adds `<name>_remesh` with the same transform.
- **Failure by exit code.** The CLI now exits non-zero when it produces no
  geometry (`2`; load failure remains `1`), so the add-on detects failure
  directly instead of guessing from the output file. This is the *only*
  core-side change for Blender.

## Install

1. Build AutoRemesher (see the main [README](../README.md)) so you have the CLI
   executable. On macOS that is `AutoRemesher.app/Contents/MacOS/autoremesher`.
2. Zip the add-on **package folder** (the folder must be the top entry in the zip):
   ```bash
   cd blender
   zip -r autoremesher_bridge.zip autoremesher_bridge
   ```
3. In Blender: **Edit ▸ Preferences ▸ Add-ons ▸ Install…**, pick the zip, and
   enable **Mesh: AutoRemesher Bridge**.
4. Expand the add-on's preferences and set **AutoRemesher Binary**.
   - **macOS:** just select `AutoRemesher.app` — Blender's file browser can't
     descend into a `.app`, so the add-on resolves
     `…/Contents/MacOS/autoremesher` inside it for you. A direct path to the
     inner executable works too.

> During development you can instead symlink `blender/autoremesher_bridge` into
> your Blender `scripts/addons/` directory and use **Reload Scripts**.

## Use

1. Select a mesh object.
2. Open the **N** sidebar in the 3D viewport → **AutoRemesher** tab.
3. Set the parameters (see the [parameter reference](../README.md#parameters)),
   choose **Replace In Place** or **New Object**, and click **Remesh with
   AutoRemesher**.
4. The status bar shows elapsed time while it runs; press **Esc** to cancel.

## Testing checklist

- Default cube → visible quad grid, replaces in place, transform preserved.
- A transformed/rotated/scaled object → result stays put (local-space round-trip).
- **New Object** mode → original untouched, `<name>_remesh` created alongside.
- **Apply Modifiers** on an object with a Subdivision modifier → remesh reflects
  the subdivided surface and the modifier is cleared after replace.
- A large STL import → completes; UI stays responsive.
- Cancel mid-run with **Esc** → subprocess terminates, scene unchanged.
- Bad binary path → clear error, no partial changes.

## Limitations / follow-ups

- **Geometry only.** UVs, materials, vertex groups, and custom normals are not
  transferred to the remeshed mesh (retopology changes the topology, so most
  would need re-projection anyway). Re-projecting UVs is a possible follow-up.
- **One object at a time** (the active object). Batch/multi-object could iterate
  the selection.
- **No incremental progress.** The bar shows elapsed time, not percent; the CLI
  reports percent on stdout, which a future version could parse from the log.
- The CLI still initializes Qt at startup, so a headless machine may need a
  display or `QT_QPA_PLATFORM=offscreen`; not an issue on a normal desktop.
