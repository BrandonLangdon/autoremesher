"""Minimal, self-contained OBJ read/write for the AutoRemesher bridge.

We deliberately do NOT use Blender's import/export operators: writing/parsing a
tiny OBJ ourselves is faster, immune to Blender's version-to-version operator
churn, and lets us keep exactly the data the remesher needs (positions and
faces, in the object's local space) and nothing else.
"""


def object_to_obj(obj, depsgraph, filepath, apply_modifiers=True):
    """Write ``obj``'s mesh to ``filepath`` as a triangulated OBJ in local space.

    When ``apply_modifiers`` is True the evaluated mesh (modifier stack baked) is
    written; otherwise the base mesh is written as-is. Coordinates are in the
    object's local space, so the result round-trips onto the same object via its
    unchanged world transform.
    """
    if apply_modifiers:
        owner = obj.evaluated_get(depsgraph)
        mesh = owner.to_mesh()
    else:
        owner = obj
        mesh = obj.to_mesh()

    try:
        mesh.calc_loop_triangles()
        with open(filepath, "w", encoding="utf-8") as f:
            f.write("# Exported by AutoRemesher Bridge\n")
            for v in mesh.vertices:
                co = v.co
                f.write("v %.6f %.6f %.6f\n" % (co[0], co[1], co[2]))
            for tri in mesh.loop_triangles:
                a, b, c = tri.vertices
                f.write("f %d %d %d\n" % (a + 1, b + 1, c + 1))
    finally:
        # Free the temporary mesh created by to_mesh().
        owner.to_mesh_clear()


def read_obj(filepath):
    """Parse an OBJ file into (vertices, faces).

    Returns a list of (x, y, z) tuples and a list of vertex-index tuples. Faces
    keep their original arity (the remesher emits quads and the occasional
    non-quad), which ``mesh.from_pydata`` accepts directly.
    """
    vertices = []
    faces = []
    with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            if line.startswith("v "):
                parts = line.split()
                vertices.append((float(parts[1]), float(parts[2]), float(parts[3])))
            elif line.startswith("f "):
                idx = []
                for token in line.split()[1:]:
                    # Handle "v", "v/vt", "v/vt/vn", "v//vn".
                    idx.append(int(token.split("/")[0]) - 1)
                if len(idx) >= 3:
                    faces.append(tuple(idx))
    return vertices, faces
