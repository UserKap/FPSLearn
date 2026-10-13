"""
只读：列出瞄具静态网格每个分段的材质槽、顶点范围、平均法线和 UV 范围，结果写到 Saved/ScopeLensGeometry_Result.txt。
用来确定镜片的中心、朝向和尺寸，供 create_red_dot_lens.py 的镜片参数使用。
"""

import os
import traceback

import unreal

MESH_PATH = "/Game/XL_FPSPack/Weapons/attachment/Attachment_sig-sauer-romeo-3"
RESULT_FILE = os.path.join(unreal.Paths.project_saved_dir(), "ScopeLensGeometry_Result.txt")


def fmt(v):
    return "({:.3f}, {:.3f}, {:.3f})".format(v.x, v.y, v.z)


def main():
    lines = []
    try:
        mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
        subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        sections = mesh.get_num_sections(0)
        lines.append("sections {}".format(sections))
        for section in range(sections):
            try:
                slot = subsystem.get_lod_material_slot(mesh, 0, section)
            except Exception as error:
                slot = "? {}".format(error)
            vertices, triangles, normals, uvs, tangents = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, section)
            lines.append("section {} slot {} verts {} tris {}".format(section, slot, len(vertices), len(triangles) // 3))
            if not vertices:
                continue
            lo = unreal.Vector(min(v.x for v in vertices), min(v.y for v in vertices), min(v.z for v in vertices))
            hi = unreal.Vector(max(v.x for v in vertices), max(v.y for v in vertices), max(v.z for v in vertices))
            center = unreal.Vector((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, (lo.z + hi.z) / 2)
            avg = unreal.Vector(sum(n.x for n in normals), sum(n.y for n in normals), sum(n.z for n in normals))
            lines.append("  min {} max {} center {}".format(fmt(lo), fmt(hi), fmt(center)))
            lines.append("  normal sum {} first {}".format(fmt(avg), fmt(normals[0])))
            if uvs:
                lines.append("  uv min ({:.3f}, {:.3f}) max ({:.3f}, {:.3f})".format(
                    min(u.x for u in uvs), min(u.y for u in uvs), max(u.x for u in uvs), max(u.y for u in uvs)))
            if len(vertices) <= 400:
                for index, v in enumerate(vertices):
                    lines.append("  v{} {} n {} uv ({:.3f}, {:.3f})".format(index, fmt(v), fmt(normals[index]), uvs[index].x, uvs[index].y))
    except Exception:
        lines.append("FAILED " + traceback.format_exc())
    with open(RESULT_FILE, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


main()
