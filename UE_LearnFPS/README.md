# UE_LearnFPS

用虚幻 5.8 第一人称模板练一套自己的 FPS 角色。模板关卡和射击、恐怖变体还在，正在做的是 `CustomFPS`。

给人看的是这份。给后续对话里的 AI 看的是 [README_AI.md](README_AI.md)。目录、玩法或资源路径变了，两份一起改。

最近整理：2026-10-13。

## 现在能玩什么

自定义角色 `ACustomFPSCharacter`（蓝图 `BP_MainPlayer`）已经拆成输入、移动、视角、枪械四块：

- 走、跑、蹲、瞄准、侧身、转视角
- 生成一把枪，装到手骨上。现有枪是 `BP_MCX300`
- 全自动、半自动、手动三种射击节奏，弹夹、备用弹、换弹和拉栓
- 枪指定一种子弹。子弹有口径、弹头、肉伤、穿甲伤害，以及发射前、发射后两套网格
- 开火时按待机、走路、瞄准挑手臂和枪的蒙太奇
- 后坐力：每把枪引用一份「后坐力」数据资产。连射按固定弹道曲线往上、往左右走，每发再带一点随机。开镜、下蹲更稳，移动更飘。松手后准星回落约 70%，已经压下去的量先扣掉。镜头每发还会顿一下，只动画面不动准星。手臂和枪每发一起往后退、枪口上翘再回位，开镜时以上翘为主
- 每发实际射出后，从武器骨骼网格的抛壳插槽抛出实体弹壳。插槽默认名是 `Ejection`，没有这个插槽时用 `Bolt` 骨骼。弹壳撞到地面或墙会出声，默认音效是 `Content/ShotSystem/Shot_SFX/SC_IG_WEP_Casing_Random`，撞得越重越响，弹跳几下后安静
- 每发实际射出时，在枪管模型的 `Muzzle` 插槽上播放 MetaSound 和开火特效。默认音效是 `Content/MetaSound/Weapon/00_Mcx/MS_00_MCX`。默认特效是资源包里的枪口烟雾 `Content/XL_FPSPack/Weapons/Common/VFX/Muzzle_Smoke/Muzzle_Smoke`。两项都能在武器蓝图里换成别的
- 腰射镜头跟身体骨骼晃，开镜时视野收窄，镜头抖动不带动枪
- 枪有惯性：鼠标动了枪带一点延迟跟过去，停手时会甩过一点再回来，开镜时更跟手。参数在视角组件的「FPS|枪惯性」里
- 自由瞄准（模仿执法记录仪手感）：鼠标先转枪，枪口在死区里转时镜头不动，偏出死区后镜头和身体才带着惯性跟过去；猛甩时枪先到、镜头后到，但枪不会出画面。鼠标停下一会儿，镜头慢慢回中对准枪口。开镜时死区很小。走路方向跟身体，不跟枪口。参数在视角组件的「FPS|自由瞄准」里，可以整套关掉
- 死亡开关：死后移动、视角和开火都停

还没接上的：

- 子弹是从枪口沿枪管打出的射线。红点镜的红点投在无限远，出厂对准 50 米：这个距离上红点就是弹着点，近处子弹比红点低几厘米。武器「瞄具」分类里有高低、风偏两个旋钮（角分），和真红点镜一样拧它来改归零。贴墙时枪口插进墙里不会穿墙。每发有一点随机散布：枪本身的精度（角分，乘子弹的精度倍率），加上腰射、移动、空中和连射的额外散布，停火后连射散布慢慢恢复。现在只画弹道、在屏幕上显示打中了什么和这一发的散布，还不扣血。肉伤和穿甲伤害记在子弹上
- 换弹和拉栓没有按键
- 没有跳跃、生命、受击
- 工程默认地图仍是模板的 `Lvl_FirstPerson`，默认模式仍是 `BP_FirstPersonGameMode`。自定义模式是 `BP_CustomFPSGameMode`，进游戏前要在关卡或世界设置里换成它

## 怎么打开

引擎：`D:\UE\UE_5.8`  
工程：`D:\UnityProject\FPS\UE_LearnFPS\UE_LearnFPS.uproject`

用引擎打开工程即可。改了 C++ 之后，在工程目录执行：

```bat
D:\UE\UE_5.8\Engine\Build\BatchFiles\Build.bat UE_LearnFPSEditor Win64 Development -Project="D:\UnityProject\FPS\UE_LearnFPS\UE_LearnFPS.uproject" -WaitMutex -FromMsBuild -architecture=x64
```

Cursor 打开工程后会监视 `Source` 和 `Plugins`。保存 C++ 时，`Tools\Clangd\Refresh-Code.ps1` 用最近一次编辑器编译的 rsp 重写 `compile_commands.json`，clangd 接着重启。改了 `Build.cs` 的包含路径要先编过一次编辑器目标，新的包含路径才会进索引。

## 代码怎么分

只有一个运行时模块 `UE_LearnFPS`。

| 位置 | 是什么 |
| --- | --- |
| `Source/UE_LearnFPS/CustomFPS/` | 自己的角色。按 Character、Input、Locomotion、View、Weapon、Animation 分子目录 |
| `Source/UE_LearnFPS/UE_LearnFPSCharacter.*` 等 | 模板第一人称角色，先留着 |
| `Variant_Shooter`、`Variant_Horror` | 模板自带的射击和恐怖变体 |
| `Content/CustomEditorBP/` | 自定义角色、模式、输入、枪 |
| `Content/Player/Anims/` | 手臂和 MCX 的动画蓝图、混合空间 |
| `Content/XL_FPSPack/` | 外部角色和枪械资源包 |
| `Content/FirstPerson/`、`Weapons/`、`Variant_*` | 模板内容 |

角色身体挂在胶囊体上。第一人称手臂挂在 `ArmsAnchor` 上，和相机同级，这样镜头晃不会把枪晃走。相机运行时再去跟身体骨骼。

枪是单独的 Actor。角色上的枪械组件只记住要生成哪一把，进游戏后生成并装到手臂插槽。武器不直接认识角色类，只通过「持枪者」接口问瞄准、跑步、移动，并让角色播手臂开火动画。

## 主要资源

- 角色蓝图：`Content/CustomEditorBP/BP_MainPlayer`
- 模式蓝图：`Content/CustomEditorBP/BP_CustomFPSGameMode`
- 输入：`Content/CustomEditorBP/Input/IMC_CustomFPSInput`，以及移动、视角、瞄准、跑步、下蹲、侧身、开火
- 枪：`Content/CustomEditorBP/Weapon/BP_MCX300`
- 开火音效：`Content/MetaSound/Weapon/00_Mcx/MS_00_MCX`
- 开火特效：`Content/XL_FPSPack/Weapons/Common/VFX/Muzzle_Smoke/Muzzle_Smoke`
- 手臂动画：`Content/Player/Anims/Player/ABP_MainPlayer`
- 枪动画：`Content/Player/Anims/Weapon/ABP_MCX300`
- 红点镜镜片：`Content/CustomEditorBP/Weapon/M_RedDotLens`，Romeo 3 网格的 `Holographc` 槽指定的是它的实例 `M_RedDotLens_Inst`。武器不换材质，只把瞄准线方向写到 `Scope` 部件上给材质读。准星是一张贴图（默认 `T_Holographic_17`），投在无限远，没有视差。镜片是受光玻璃，有反射；斜着看会被镜框挡出黑边，边缘一圈有折射扭曲。贴图、准星角度大小（`ReticleSize`，角分）、亮度、颜色、玻璃颜色和反射、黑边深度、扭曲强度在材质参数里调，方向由代码写入。材质由 `Tools/Python/create_red_dot_lens.py` 生成，注意事项同下
- 执法记录仪镜头：`Content/CustomEditorBP/PostProcess/M_PP_BodycamLens`（桶形畸变、色边、边缘模糊、圆角柔边、暗角），实例 `M_PP_BodycamLens_Inst` 挂在角色相机的后期处理材质上。材质由 `Tools/Python/create_bodycam_lens.py` 生成，重跑会重建节点；跑之前先关掉编辑器里的材质窗口，跑完右键材质「重新加载」
