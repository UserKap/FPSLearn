"""
生成 /Game/CustomEditorBP/PostProcess/M_PP_BodycamLens：执法记录仪镜头的桶形畸变、色边、边缘模糊、圆角柔边和暗角。
已存在时清空节点重建，参数恢复默认值。材质实例里改过的参数保留。

在编辑器里运行（需要启用 Python Editor Script Plugin）：
    py "E:/Project/UE_LearnFPS/Tools/Python/create_bodycam_lens.py"
不开编辑器运行：
    UnrealEditor-Cmd.exe UE_LearnFPS.uproject -run=pythonscript -script=<本文件> -EnablePlugins=PythonScriptPlugin -unattended
"""

import os
import traceback

import unreal

PACKAGE_PATH = "/Game/CustomEditorBP/PostProcess"
ASSET_NAME = "M_PP_BodycamLens"
RESULT_FILE = os.path.join(unreal.Paths.project_saved_dir(), "BodycamLens_Result.txt")

MEL = unreal.MaterialEditingLibrary

# 屏幕坐标按画面比例换成圆，角落的半径平方归一到 1。
# 三个通道用略有差别的畸变量，形成色边。越靠边，在一个小圆盘里多采几次求平均，形成边缘模糊。
# 画面边界按圆角矩形柔和渐隐，不会出现硬切的黑边。
# Scene 只用来让材质登记 PostProcessInput0，本身不参与颜色。
LENS_CODE = """
float Aspect = ViewSize.x / max(ViewSize.y, 1.0);
float2 AspectScale = float2(Aspect, 1.0);
float2 P = (UV - 0.5) * AspectScale;
float R2 = dot(P, P) / (0.25 * (Aspect * Aspect + 1.0));

float3 ChannelK = K1 * float3(1.0 + Fringe, 1.0, 1.0 - Fringe);
float3 Scale = lerp(float3(1.0, 1.0, 1.0), (1.0 + ChannelK * R2 + K2 * R2 * R2) / max(Zoom, 0.01), Strength);
float BlurRadius = EdgeBlur * Strength * R2 * R2;

const int Taps = 10;
float3 Color = 0.0;
for (int i = 0; i < Taps; i++)
{
    float Angle = i * 2.39996323;
    float2 Offset = float2(cos(Angle), sin(Angle)) * BlurRadius * sqrt((i + 0.5) / Taps) / AspectScale;
    float2 UVR = P * Scale.r / AspectScale + 0.5 + Offset;
    float2 UVG = P * Scale.g / AspectScale + 0.5 + Offset;
    float2 UVB = P * Scale.b / AspectScale + 0.5 + Offset;
    Color.r += SceneTextureLookup(ClampSceneTextureUV(ViewportUVToSceneTextureUV(UVR, PPI_PostProcessInput0), PPI_PostProcessInput0), PPI_PostProcessInput0, true).r;
    Color.g += SceneTextureLookup(ClampSceneTextureUV(ViewportUVToSceneTextureUV(UVG, PPI_PostProcessInput0), PPI_PostProcessInput0), PPI_PostProcessInput0, true).g;
    Color.b += SceneTextureLookup(ClampSceneTextureUV(ViewportUVToSceneTextureUV(UVB, PPI_PostProcessInput0), PPI_PostProcessInput0), PPI_PostProcessInput0, true).b;
}
Color /= Taps;

float Corner = max(CornerRadius, 0.001);
float2 Q = abs(P * Scale.g) - (float2(Aspect * 0.5, 0.5) - Corner);
float Dist = length(max(Q, 0.0)) + min(max(Q.x, Q.y), 0.0) - Corner;
float Inside = lerp(1.0, 1.0 - smoothstep(-max(EdgeSoftness, 0.001), 0.0, Dist), Strength);
float Vig = saturate(1.0 - Vignette * Strength * R2 * R2);
return Color * Vig * Inside + Scene.rgb * 0.0;
"""

PARAMETERS = [
    ("Strength", 1.0),
    ("K1", 0.22),
    ("K2", 0.08),
    ("Zoom", 1.12),
    ("Fringe", 0.035),
    ("EdgeBlur", 0.012),
    ("EdgeSoftness", 0.08),
    ("CornerRadius", 0.12),
    ("Vignette", 0.55),
]


def find_enum(enum_cls, *keywords):
    for name in dir(enum_cls):
        value = getattr(enum_cls, name)
        if isinstance(value, enum_cls) and all(k in name.upper() for k in keywords):
            return value
    raise RuntimeError("找不到枚举 {} {}".format(enum_cls.__name__, keywords))


def make_custom(material, x, y, description, code, output_type, input_names):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("description", description)
    node.set_editor_property("code", code)
    node.set_editor_property("output_type", output_type)
    inputs = []
    for name in input_names:
        # CustomInput 不是蓝图结构体，不能用关键字参数构造。
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property("input_name", name)
        inputs.append(custom_input)
    node.set_editor_property("inputs", inputs)
    return node


def make_scalar(material, x, y, name, value):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    node.set_editor_property("group", "Bodycam")
    return node


def clear_expressions(material):
    # 引擎的 DeleteAllMaterialExpressions 边遍历边删除，一次只删掉约一半，要删到没有为止。
    for _ in range(64):
        if MEL.get_num_material_expressions(material) == 0:
            return
        MEL.delete_all_material_expressions(material)
    raise RuntimeError("清不干净材质节点")


def connect(from_node, to_node, input_name, output_name=""):
    if not MEL.connect_material_expressions(from_node, output_name, to_node, input_name):
        raise RuntimeError("连线失败：{} -> {}".format(from_node.get_name(), input_name))


def build():
    asset_path = "{}/{}".format(PACKAGE_PATH, ASSET_NAME)
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        material = unreal.EditorAssetLibrary.load_asset(asset_path)
        clear_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            ASSET_NAME, PACKAGE_PATH, unreal.Material, unreal.MaterialFactoryNew())

    material.set_editor_property("material_domain", find_enum(unreal.MaterialDomain, "POST_PROCESS"))
    material.set_editor_property("blendable_location", find_enum(unreal.BlendableLocation, "SCENE_COLOR_AFTER_TONEMAPPING"))

    screen_uv = MEL.create_material_expression(material, unreal.MaterialExpressionScreenPosition, -900, -200)
    view_size = MEL.create_material_expression(material, unreal.MaterialExpressionViewSize, -900, -100)
    scene = MEL.create_material_expression(material, unreal.MaterialExpressionSceneTexture, -900, -350)
    scene.set_editor_property("scene_texture_id", find_enum(unreal.SceneTextureId, "POST_PROCESS_INPUT0"))

    input_names = ["Scene", "UV", "ViewSize"] + [name for name, _ in PARAMETERS]
    lens = make_custom(material, -400, 0, "Bodycam Lens", LENS_CODE,
                       find_enum(unreal.CustomMaterialOutputType, "FLOAT3"), input_names)
    connect(scene, lens, "Scene")
    connect(screen_uv, lens, "UV")
    connect(view_size, lens, "ViewSize")
    for index, (name, value) in enumerate(PARAMETERS):
        connect(make_scalar(material, -900, index * 100, name, value), lens, name)

    if not MEL.connect_material_property(lens, "", find_enum(unreal.MaterialProperty, "EMISSIVE_COLOR")):
        raise RuntimeError("连不上自发光输出")

    errors = MEL.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material, False):
        raise RuntimeError("保存失败，可能是编辑器正占用这个材质")
    return asset_path, errors


def main():
    lines = []
    try:
        asset_path, errors = build()
        lines.append("OK " + asset_path)
        lines.extend("COMPILE_ERROR " + e for e in errors)
    except Exception:
        lines.append("FAILED " + traceback.format_exc())
    with open(RESULT_FILE, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    for line in lines:
        unreal.log(line)


main()
