"""
只读：把准星贴图导出成 PNG 到 Saved/ReticlePreview，方便查看形状和透明通道。
"""

import os
import traceback

import unreal

TEXTURES = [
    "/Game/XL_FPSPack/Weapons/attachment/Reticles/Textures/T_Holographic_17",
    "/Game/XL_FPSPack/Weapons/attachment/Reticles/Textures/T_Holographic_15",
]
OUTPUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "ReticlePreview")
RESULT_FILE = os.path.join(unreal.Paths.project_saved_dir(), "ReticlePreview_Result.txt")


def main():
    lines = []
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    for path in TEXTURES:
        try:
            texture = unreal.EditorAssetLibrary.load_asset(path)
            task = unreal.AssetExportTask()
            task.set_editor_property("object", texture)
            task.set_editor_property("filename", os.path.join(OUTPUT_DIR, path.split("/")[-1] + ".png"))
            task.set_editor_property("automated", True)
            task.set_editor_property("prompt", False)
            task.set_editor_property("replace_identical", True)
            lines.append("{} {}".format(path, unreal.Exporter.run_asset_export_task(task)))
        except Exception:
            lines.append("FAILED {} {}".format(path, traceback.format_exc()))
    with open(RESULT_FILE, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


main()
