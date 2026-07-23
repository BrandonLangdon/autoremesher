"""AutoRemesher Bridge — quad-remesh the selected object via the AutoRemesher CLI.

The add-on is a thin driver: it exports the active mesh to a temp OBJ, runs the
standalone AutoRemesher executable as a subprocess, and imports the result back.
The AutoRemesher core is never linked into Blender, so it stays fully standalone.
"""

bl_info = {
    "name": "AutoRemesher Bridge",
    "author": "AutoRemesher contributors",
    "version": (1, 0, 0),
    "blender": (3, 0, 0),
    "location": "View3D > Sidebar (N) > AutoRemesher",
    "description": "Quad-remesh the selected object using the AutoRemesher CLI",
    "category": "Mesh",
}

import bpy
from bpy.props import (
    BoolProperty,
    EnumProperty,
    FloatProperty,
    IntProperty,
    PointerProperty,
    StringProperty,
)
from bpy.types import AddonPreferences, Panel, PropertyGroup

from . import operator as ar_operator


class AutoRemesherPreferences(AddonPreferences):
    bl_idname = __package__

    autoremesher_path: StringProperty(
        name="AutoRemesher Binary",
        description="Path to the AutoRemesher executable (the CLI). On macOS this is "
        "inside the app bundle: AutoRemesher.app/Contents/MacOS/autoremesher",
        subtype="FILE_PATH",
    )
    ftetwild_path: StringProperty(
        name="fTetWild Binary",
        description="Optional path to FloatTetwild_bin, used when 'Use fTetWild' is enabled",
        subtype="FILE_PATH",
    )

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "autoremesher_path")
        layout.prop(self, "ftetwild_path")
        layout.label(
            text="Tip: point AutoRemesher Binary at the CLI executable, not the .app folder.",
            icon="INFO",
        )


class AutoRemesherSettings(PropertyGroup):
    target_quads: IntProperty(
        name="Target Quads",
        description="Approximate number of quads in the output — the main density control",
        default=50000, min=1000, max=1000000,
    )
    edge_scaling: FloatProperty(
        name="Edge Scaling",
        description="Multiplies the target edge length; raise for larger quads / lower-poly output",
        default=1.0, min=1.0, max=4.0,
    )
    adaptivity: FloatProperty(
        name="Adaptivity",
        description="How strongly quad density follows curvature (0 = uniform, 1 = curvature-adaptive)",
        default=1.0, min=0.0, max=1.0,
    )
    sharp_edge: FloatProperty(
        name="Sharp Edge",
        description="Dihedral-angle threshold (degrees); edges sharper than this are kept as features. "
        "Lower keeps more hard edges (hard-surface); higher keeps fewer (organic)",
        default=90.0, min=30.0, max=180.0,
    )
    smooth_normal: FloatProperty(
        name="Smooth Normal",
        description="Surface smoothing during remeshing (0 = faceted; larger blends normals up to this angle)",
        default=0.0, min=0.0, max=180.0,
    )
    use_ftetwild: BoolProperty(
        name="Use fTetWild",
        description="Pre-clean the surface with fTetWild before quad remeshing (needs the fTetWild "
        "binary set in preferences). Robust on large or messy meshes",
        default=False,
    )
    apply_modifiers: BoolProperty(
        name="Apply Modifiers",
        description="Remesh the evaluated mesh (modifier stack baked). When replacing in place, the "
        "object's modifiers are cleared since they are now part of the geometry",
        default=True,
    )
    output_mode: EnumProperty(
        name="Output",
        description="What to do with the remeshed result",
        items=[
            ("REPLACE", "Replace In Place", "Swap the object's mesh, keeping its transform, name and (baked) look"),
            ("NEW", "New Object", "Add a new object beside the original with the same transform"),
        ],
        default="REPLACE",
    )


class VIEW3D_PT_autoremesher(Panel):
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "AutoRemesher"
    bl_label = "AutoRemesher"

    def draw(self, context):
        layout = self.layout
        settings = context.scene.autoremesher

        col = layout.column(align=True)
        col.prop(settings, "target_quads")
        col.prop(settings, "edge_scaling")
        col.prop(settings, "adaptivity")
        col.prop(settings, "sharp_edge")
        col.prop(settings, "smooth_normal")

        col = layout.column(align=True)
        col.prop(settings, "use_ftetwild")
        col.prop(settings, "apply_modifiers")

        layout.prop(settings, "output_mode")

        row = layout.row()
        row.scale_y = 1.4
        row.operator("mesh.autoremesher_remesh", icon="MOD_REMESH")


classes = (
    AutoRemesherPreferences,
    AutoRemesherSettings,
    ar_operator.MESH_OT_autoremesher,
    VIEW3D_PT_autoremesher,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.Scene.autoremesher = PointerProperty(type=AutoRemesherSettings)


def unregister():
    del bpy.types.Scene.autoremesher
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
