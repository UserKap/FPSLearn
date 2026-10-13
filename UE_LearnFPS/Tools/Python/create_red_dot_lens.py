"""
生成 /Game/CustomEditorBP/Weapon/M_RedDotLens：红点镜镜片。

镜片是受光的薄玻璃（Thin Translucent，工程开了 Substrate，会转成彩色透射）：反射按菲涅尔，不会因为玻璃透明而变淡；
GlassColor 是透过玻璃的颜色。红点是自发光，直接叠加，不受玻璃反射影响。
Opacity 在这个着色模型里表示「不透明顶层」的比例，顶层底色是黑的，所以边缘黑边就是把 Opacity 拉到 1。

准星：ReticleDirection（瞄准线方向，已含高低、风偏修正）和 ReticleUp（枪的上方）是世界方向，读瞄具部件的自定义图元数据
（0~3、4~7），由武器每帧写入，不要在材质里改。武器不替换材质，镜片材质槽要自己指定成这个材质或它的实例。
贴图坐标按视线偏离瞄准线的角度算，和眼睛在镜片后面的位置无关，所以没有视差。ReticleSize 是整张贴图对应的角分。
准星形状取贴图透明通道，颜色由 DotColor 决定。

镜片边缘：LensUVCenter 是镜片中心的 UV，LensUVScale 是 1 个 UV 等于多少网格单位，半宽半高、上下圆角用网格单位，
按 Attachment_sig-sauer-romeo-3 量出（Tools/Python/inspect_scope_lens_geometry.py）。换瞄具时要重新量。
TubeDepth 模拟镜框前沿到镜片的深度：视线穿过镜片后走到这个深度，落在镜框外就是黑的，所以斜着看会出现月牙形黑边。
MaxShadow 限制黑边最多往里盖多少，按镜片半宽半高的比例算，几乎平视镜片时也不会整片盖黑。
DistortWidth 宽的一圈里法线往外弯，配合折射，边缘的画面会被拉扯。

已存在时清空节点重建，参数恢复默认值。材质实例里改过的参数保留。

不开编辑器运行：
    UnrealEditor-Cmd.exe UE_LearnFPS.uproject -run=pythonscript -script=<本文件> -EnablePlugins=PythonScriptPlugin -unattended
"""

import os
import traceback

import unreal

PACKAGE_PATH = "/Game/CustomEditorBP/Weapon"
ASSET_NAME = "M_RedDotLens"
RESULT_FILE = os.path.join(unreal.Paths.project_saved_dir(), "RedDotLens_Result.txt")
RETICLE_TEXTURE = "/Game/XL_FPSPack/Weapons/attachment/Reticles/Textures/T_Holographic_17"

MEL = unreal.MaterialEditingLibrary

# 角度换成角分：弧度 × 180 / π × 60。右方 = 上方 × 前方，和代码里一致。
# 输出 xy 是贴图坐标，z 为 1 表示在贴图范围内且视线朝前。
UV_CODE = """
float3 V = -CamVec;
float3 D = Dir.xyz / max(length(Dir.xyz), 0.0001);
float3 U = Up.xyz - D * dot(Up.xyz, D);
U = U / max(length(U), 0.0001);
float3 R = cross(U, D);
float Forward = dot(V, D);
float X = atan2(dot(V, R), Forward) * 3437.747;
float Y = atan2(dot(V, U), Forward) * 3437.747;
float2 UV = 0.5 + float2(X, -Y) / max(ReticleSize, 0.01);
float Inside = (Forward > 0.0 && all(abs(UV - 0.5) < 0.5)) ? 1.0 : 0.0;
return float3(UV, Inside);
"""

# 全在瞄具网格局部空间的尺度里算。镜片外形是圆角矩形，上下两对角的圆角不同（Romeo 3 上圆下平）。
# 镜片位置取 UV：镜片 UV 是正面平铺投影，往右、往上 UV 都变小。
# 不能用 LocalPosition：第一人称网格的像素世界位置被往相机缩放过，反推出的局部位置是错的。
# 前沿开口 = 镜片沿视线往前推 TubeDepth：落在开口外的部分被镜框挡住。
# 输出 xyz 是边缘法线要加的局部偏移，w 是黑边（1 为全黑）。
MASK_CODE = """
float3 Right = LensRight.xyz;
float3 Upward = LensUp.xyz;
float3 Front = normalize(cross(Right, Upward));
float3 D = -CamLocal / max(length(CamLocal), 0.0001);
float2 Half = max(LensHalfSize.xy, 0.001);
float MaxRadius = min(Half.x, Half.y);

float2 Rel = (LensUVCenter.xy - UV) * LensUVScale;
float Along = abs(dot(D, Front));
float2 Shift = float2(dot(D, Right), dot(D, Upward)) / max(Along, 0.05) * TubeDepth;
float ShiftRatio = length(Shift / Half);
float Limit = max(MaxShadow, 0.0);
Shift *= ShiftRatio > Limit ? Limit / ShiftRatio : 1.0;
float2 RelFront = Rel + Shift;

float Radius = min(Rel.y > 0.0 ? TopRadius : BottomRadius, MaxRadius);
float2 Q = abs(Rel) - Half + Radius;
float LensDist = length(max(Q, 0.0)) + min(max(Q.x, Q.y), 0.0) - Radius;

Radius = min(RelFront.y > 0.0 ? TopRadius : BottomRadius, MaxRadius);
Q = abs(RelFront) - Half + Radius;
float FrontDist = length(max(Q, 0.0)) + min(max(Q.x, Q.y), 0.0) - Radius;

float Soft = max(EdgeSoftness, 0.001);
float Visible = (1.0 - smoothstep(-Soft, 0.0, LensDist)) * (1.0 - smoothstep(-Soft, 0.0, FrontDist));

float Edge = smoothstep(-max(DistortWidth, 0.001), 0.0, LensDist);
float2 Outward = Rel / Half;
Outward = Outward / max(length(Outward), 0.0001);
float3 Bend = (Right * Outward.x + Upward * Outward.y) * Edge * EdgeBend;
return float4(Bend, 1.0 - Visible);
"""

NORMAL_CODE = """
return normalize(VertexNormal + BendWorld);
"""

# 红点在镜框挡住的地方也看不见。ReticleOcclusion 让红点挡掉一部分背景，亮背景下不至于发白。
SURFACE_CODE = """
float Visible = 1.0 - Dark;
float Mask = TexAlpha * Inside * Visible;
float Opacity = saturate(Dark + Mask * ReticleOcclusion);
return float4(DotColor.rgb * Mask * DotIntensity, Opacity);
"""

UV_SCALARS = [
    ("ReticleSize", 150.0),
]

UV_VECTORS = [
    ("ReticleDirection", "Dir", unreal.LinearColor(1.0, 0.0, 0.0, 0.0)),
    ("ReticleUp", "Up", unreal.LinearColor(0.0, 0.0, 1.0, 0.0)),
]

# 这两个由武器写到瞄具部件的自定义图元数据上，下标要和 ACustomFPSWeapon::UpdateReticle 一致。
PRIMITIVE_DATA = {
    "ReticleDirection": 0,
    "ReticleUp": 4,
}

MASK_SCALARS = [
    ("LensUVScale", 26.44),
    ("TopRadius", 11.2),
    ("BottomRadius", 5.0),
    ("EdgeSoftness", 0.6),
    ("TubeDepth", 2.0),
    ("MaxShadow", 0.3),
    ("DistortWidth", 3.0),
    ("EdgeBend", 0.6),
]

MASK_VECTORS = [
    ("LensUVCenter", unreal.LinearColor(0.5, 0.4827, 0.0, 0.0)),
    ("LensHalfSize", unreal.LinearColor(13.22, 11.23, 0.0, 0.0)),
    ("LensRight", unreal.LinearColor(0.0, 1.0, 0.0, 0.0)),
    ("LensUp", unreal.LinearColor(0.0, 0.0, 1.0, 0.0)),
]

SURFACE_SCALARS = [
    ("DotIntensity", 20.0),
    ("ReticleOcclusion", 0.5),
]

SURFACE_VECTORS = [
    ("DotColor", unreal.LinearColor(1.0, 0.03, 0.02, 1.0)),
]


def find_enum(enum_cls, *keywords):
    for name in dir(enum_cls):
        value = getattr(enum_cls, name)
        if isinstance(value, enum_cls) and all(k in name.upper() for k in keywords):
            return value
    raise RuntimeError("找不到枚举 {} {}".format(enum_cls.__name__, keywords))


def find_enum_by_member(*keywords):
    for class_name in dir(unreal):
        enum_cls = getattr(unreal, class_name)
        if isinstance(enum_cls, type) and issubclass(enum_cls, unreal.EnumBase):
            for name in dir(enum_cls):
                if all(k in name.upper() for k in keywords):
                    value = getattr(enum_cls, name)
                    if isinstance(value, enum_cls):
                        return value
    raise RuntimeError("找不到枚举成员 {}".format(keywords))


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
    node.set_editor_property("group", "RedDot")
    return node


def make_vector(material, x, y, name, value):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    node.set_editor_property("group", "RedDot")
    if name in PRIMITIVE_DATA:
        node.set_editor_property("use_custom_primitive_data", True)
        node.set_editor_property("primitive_data_index", PRIMITIVE_DATA[name])
    return node


def make_mask(material, x, y, channels):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionComponentMask, x, y)
    for channel in "rgba":
        node.set_editor_property(channel, channel in channels)
    return node


def make_transform(material, x, y, source, destination):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionTransform, x, y)
    node.set_editor_property("transform_source_type", find_enum_suffix(unreal.MaterialVectorCoordTransformSource, source))
    node.set_editor_property("transform_type", find_enum_suffix(unreal.MaterialVectorCoordTransform, destination))
    return node


def find_enum_suffix(enum_cls, suffix):
    for name in dir(enum_cls):
        value = getattr(enum_cls, name)
        if isinstance(value, enum_cls) and name.upper().endswith("_" + suffix):
            return value
    raise RuntimeError("找不到枚举 {} {}".format(enum_cls.__name__, suffix))


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


def connect_any(from_node, to_node, input_names, output_name=""):
    for input_name in input_names:
        if MEL.connect_material_expressions(from_node, output_name, to_node, input_name):
            return
    raise RuntimeError("连线失败：{} -> {}".format(from_node.get_name(), input_names))


def connect_property(node, prop, output_name=""):
    if not MEL.connect_material_property(node, output_name, find_enum(unreal.MaterialProperty, prop)):
        raise RuntimeError("连不上材质输出 {}".format(prop))


def connect_params(material, x, y, node, scalars, vectors):
    row = 0
    for entry in vectors:
        name, value = entry[0], entry[-1]
        input_name = entry[1] if len(entry) == 3 else name
        connect(make_vector(material, x, y + row * 150, name, value), node, input_name)
        row += 1
    for name, value in scalars:
        connect(make_scalar(material, x, y + row * 150, name, value), node, name)
        row += 1


def build():
    asset_path = "{}/{}".format(PACKAGE_PATH, ASSET_NAME)
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        material = unreal.EditorAssetLibrary.load_asset(asset_path)
        clear_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            ASSET_NAME, PACKAGE_PATH, unreal.Material, unreal.MaterialFactoryNew())

    material.set_editor_property("blend_mode", find_enum(unreal.BlendMode, "TRANSLUCENT"))
    material.set_editor_property("shading_model", find_enum(unreal.MaterialShadingModel, "THIN_TRANSLUCENT"))
    material.set_editor_property("translucency_lighting_mode", find_enum(unreal.TranslucencyLightingMode, "SURFACE", "PER_PIXEL"))
    material.set_editor_property("two_sided", False)
    material.set_editor_property("tangent_space_normal", False)
    material.set_editor_property("refraction_method", find_enum_by_member("PIXEL_NORMAL_OFFSET"))

    camera_vector = MEL.create_material_expression(material, unreal.MaterialExpressionCameraVectorWS, -2200, -600)

    # 准星贴图坐标
    uv_inputs = ["CamVec"] + [entry[1] for entry in UV_VECTORS] + [name for name, _ in UV_SCALARS]
    reticle_uv = make_custom(material, -1500, -700, "Reticle UV", UV_CODE,
                             find_enum(unreal.CustomMaterialOutputType, "FLOAT3"), uv_inputs)
    connect(camera_vector, reticle_uv, "CamVec")
    connect_params(material, -1900, -900, reticle_uv, UV_SCALARS, UV_VECTORS)

    uv = make_mask(material, -1200, -750, "rg")
    inside = make_mask(material, -1200, -600, "b")
    connect(reticle_uv, uv, "")
    connect(reticle_uv, inside, "")

    texture = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1000, -800)
    texture.set_editor_property("parameter_name", "ReticleTexture")
    texture.set_editor_property("group", "RedDot")
    texture.set_editor_property("texture", unreal.EditorAssetLibrary.load_asset(RETICLE_TEXTURE))
    texture.set_editor_property("sampler_type", find_enum(unreal.MaterialSamplerType, "COLOR"))
    connect(uv, texture, "UVs")

    # 镜片边缘
    lens_uv = MEL.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -2200, 0)
    lens_uv.set_editor_property("coordinate_index", 0)
    camera_local = make_transform(material, -1900, -300, "WORLD", "LOCAL")
    connect_any(camera_vector, camera_local, ["", "Input"])
    mask_inputs = ["UV", "CamLocal"] + [name for name, _ in MASK_VECTORS] + [name for name, _ in MASK_SCALARS]
    lens_mask = make_custom(material, -1500, 0, "Lens Mask", MASK_CODE,
                            find_enum(unreal.CustomMaterialOutputType, "FLOAT4"), mask_inputs)
    connect(lens_uv, lens_mask, "UV")
    connect(camera_local, lens_mask, "CamLocal")
    connect_params(material, -1900, 100, lens_mask, MASK_SCALARS, MASK_VECTORS)

    bend_local = make_mask(material, -1200, 0, "rgb")
    dark = make_mask(material, -1200, 150, "a")
    connect(lens_mask, bend_local, "")
    connect(lens_mask, dark, "")

    bend_world = make_transform(material, -1000, 0, "LOCAL", "WORLD")
    connect_any(bend_local, bend_world, ["", "Input"])
    vertex_normal = MEL.create_material_expression(material, unreal.MaterialExpressionVertexNormalWS, -1000, 150)
    lens_normal = make_custom(material, -700, 0, "Lens Normal", NORMAL_CODE,
                              find_enum(unreal.CustomMaterialOutputType, "FLOAT3"), ["VertexNormal", "BendWorld"])
    connect(vertex_normal, lens_normal, "VertexNormal")
    connect(bend_world, lens_normal, "BendWorld")

    # 红点和黑边
    surface_inputs = ["TexAlpha", "Inside", "Dark"] + [name for name, _ in SURFACE_VECTORS] + [name for name, _ in SURFACE_SCALARS]
    surface = make_custom(material, -500, -500, "Lens Surface", SURFACE_CODE,
                          find_enum(unreal.CustomMaterialOutputType, "FLOAT4"), surface_inputs)
    connect(texture, surface, "TexAlpha", "A")
    connect(inside, surface, "Inside")
    connect(dark, surface, "Dark")
    connect_params(material, -900, -450, surface, SURFACE_SCALARS, SURFACE_VECTORS)

    emissive = make_mask(material, -200, -550, "rgb")
    opacity = make_mask(material, -200, -400, "a")
    connect(surface, emissive, "")
    connect(surface, opacity, "")

    # 玻璃本身
    base_color = MEL.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, -250)
    base_color.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
    specular = make_scalar(material, -400, -150, "LensSpecular", 0.5)
    roughness = make_scalar(material, -400, -50, "LensRoughness", 0.05)
    refraction = make_scalar(material, -400, 250, "EdgeRefraction", 1.3)
    glass_color = make_vector(material, -400, 400, "GlassColor", unreal.LinearColor(0.92, 0.97, 1.0, 1.0))
    thin_output = MEL.create_material_expression(material, unreal.MaterialExpressionThinTranslucentMaterialOutput, -100, 400)
    connect_any(glass_color, thin_output, ["TransmittanceColor", "Transmittance Color", "Transmittance"])

    connect_property(base_color, "BASE_COLOR")
    connect_property(specular, "SPECULAR")
    connect_property(roughness, "ROUGHNESS")
    connect_property(emissive, "EMISSIVE_COLOR")
    connect_property(opacity, "OPACITY")
    connect_property(lens_normal, "NORMAL")
    connect_property(refraction, "REFRACTION")

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
