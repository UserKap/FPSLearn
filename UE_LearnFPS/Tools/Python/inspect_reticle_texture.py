"""
只读：列出原全息镜片材质实例的参数值、准星贴图的格式，以及原材质里的节点，结果写到 Saved/ReticleTexture_Result.txt。
"""

import os
import traceback

import unreal

INSTANCE_PATH = "/Game/XL_FPSPack/Weapons/attachment/Reticles/MI_Holographc_Inst6"
RESULT_FILE = os.path.join(unreal.Paths.project_saved_dir(), "ReticleTexture_Result.txt")
MEL = unreal.MaterialEditingLibrary


def describe_texture(texture, lines):
    lines.append("texture {}".format(texture.get_path_name()))
    for prop in ["compression_settings", "srgb", "lod_group", "address_x", "address_y", "mip_gen_settings"]:
        try:
            lines.append("  {} {}".format(prop, texture.get_editor_property(prop)))
        except Exception as error:
            lines.append("  {} ? {}".format(prop, error))
    try:
        lines.append("  size {} x {}".format(texture.blueprint_get_size_x(), texture.blueprint_get_size_y()))
    except Exception as error:
        lines.append("  size ? {}".format(error))
    try:
        source = texture.get_editor_property("source")
        lines.append("  source {}".format(source))
    except Exception:
        pass


def main():
    lines = []
    try:
        instance = unreal.EditorAssetLibrary.load_asset(INSTANCE_PATH)
        lines.append("instance {}".format(instance.get_path_name()))
        for name in MEL.get_scalar_parameter_names(instance):
            lines.append("scalar {} = {}".format(name, MEL.get_material_instance_scalar_parameter_value(instance, name)))
        for name in MEL.get_vector_parameter_names(instance):
            lines.append("vector {} = {}".format(name, MEL.get_material_instance_vector_parameter_value(instance, name)))
        for name in MEL.get_texture_parameter_names(instance):
            texture = MEL.get_material_instance_texture_parameter_value(instance, name)
            lines.append("texparam {} = {}".format(name, texture.get_path_name() if texture else None))
            if texture:
                describe_texture(texture, lines)

        base = instance.get_editor_property("parent")
        lines.append("base {}".format(base.get_path_name()))
        for prop in ["expressions", "expression_collection"]:
            try:
                value = base.get_editor_property(prop)
                lines.append("{} {}".format(prop, value))
            except Exception as error:
                lines.append("{} ? {}".format(prop, error))
        for texture_path in unreal.EditorAssetLibrary.list_assets("/Game/XL_FPSPack/Weapons/attachment/Reticles/Textures", recursive=False):
            texture = unreal.EditorAssetLibrary.load_asset(texture_path)
            if isinstance(texture, unreal.Texture2D):
                describe_texture(texture, lines)
    except Exception:
        lines.append("FAILED " + traceback.format_exc())
    with open(RESULT_FILE, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


main()
