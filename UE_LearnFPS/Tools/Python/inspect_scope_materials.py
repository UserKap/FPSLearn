"""
只读：列出瞄具静态网格的材质槽和材质的混合模式，结果写到 Saved/ScopeMaterials_Result.txt。
"""

import os
import traceback

import unreal

MESH_PATH = "/Game/XL_FPSPack/Weapons/attachment/Attachment_sig-sauer-romeo-3"
RESULT_FILE = os.path.join(unreal.Paths.project_saved_dir(), "ScopeMaterials_Result.txt")


def describe_material(material):
    lines = []
    base = material
    while isinstance(base, unreal.MaterialInstance):
        lines.append("    instance {} parent {}".format(base.get_path_name(), base.get_editor_property("parent").get_path_name()))
        base = base.get_editor_property("parent")
    if isinstance(base, unreal.Material):
        lines.append("    base {} blend {} shading {} two_sided {}".format(
            base.get_path_name(),
            base.get_editor_property("blend_mode"),
            base.get_editor_property("shading_model"),
            base.get_editor_property("two_sided")))
        for name in unreal.MaterialEditingLibrary.get_scalar_parameter_names(base):
            lines.append("    scalar {}".format(name))
        for name in unreal.MaterialEditingLibrary.get_vector_parameter_names(base):
            lines.append("    vector {}".format(name))
        for name in unreal.MaterialEditingLibrary.get_texture_parameter_names(base):
            lines.append("    texture {}".format(name))
    return lines


def main():
    lines = []
    try:
        mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
        lines.append("mesh {}".format(mesh.get_path_name()))
        bounds = mesh.get_bounding_box()
        lines.append("bounds min={} max={}".format(bounds.min, bounds.max))
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            material = slot.get_editor_property("material_interface")
            lines.append("slot {} name {} material {}".format(
                index, slot.get_editor_property("material_slot_name"), material.get_path_name() if material else None))
            if material:
                lines.extend(describe_material(material))
        for name in ["Lens", "Glass", "Reticle", "Dot"]:
            socket = mesh.find_socket(name)
            if socket:
                lines.append("socket {} loc {} rot {}".format(name, socket.relative_location, socket.relative_rotation))
    except Exception:
        lines.append("FAILED " + traceback.format_exc())
    with open(RESULT_FILE, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


main()
