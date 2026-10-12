# UE_LearnFPS

用虚幻 5.8 第一人称模板练一套自己的 FPS 角色。模板关卡和射击、恐怖变体还在，正在做的是 `CustomFPS`。

给人看的是这份。给后续对话里的 AI 看的是 [README_AI.md](README_AI.md)。目录、玩法或资源路径变了，两份一起改。

最近整理：2026-10-12。

## 现在能玩什么

自定义角色 `ACustomFPSCharacter`（蓝图 `BP_MainPlayer`）已经拆成输入、移动、视角、枪械四块：

- 走、跑、蹲、瞄准、侧身、转视角
- 生成一把枪，装到手骨上。现有枪是 `BP_MCX300`
- 全自动、半自动、手动三种射击节奏，弹夹、备用弹、换弹和拉栓
- 开火时按待机、走路、瞄准挑手臂和枪的蒙太奇
- 每发实际射出时，在枪管模型的 `Muzzle` 插槽上播放 MetaSound 和开火特效。默认音效是 `Content/MetaSound/Weapon/00_Mcx/MS_00_MCX`。默认特效是资源包里的枪口烟雾 `Content/XL_FPSPack/Weapons/Common/VFX/Muzzle_Smoke/Muzzle_Smoke`。两项都能在武器蓝图里换成别的
- 腰射镜头跟身体骨骼晃，开镜时视野收窄，镜头抖动不带动枪
- 死亡开关：死后移动、视角和开火都停

还没接上的：

- 子弹没有弹道，开火目前只扣弹药、播动画，并在屏幕上打一行调试字
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
