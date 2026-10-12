# README_AI

新对话先读本文件，再改代码。给人看的进度说明是 [README.md](README.md)。

行为、目录、资源路径、构建方式变了，两份一起改。本文件写约束、路径和当前实现；`README.md` 只保留结论，不抄类成员。

最近整理：2026-10-12。开火音效同一天接上。

## 工程事实

- 引擎：虚幻 5.8，安装在 `D:\UE\UE_5.8`。工作区里的 `D:\Unreal\UE_5.8` 不是这台机器上的引擎目录。
- 工程：`D:\UnityProject\FPS\UE_LearnFPS\UE_LearnFPS.uproject`
- Git 仓库根目录是 `D:\UnityProject\FPS`，工程在子目录 `UE_LearnFPS`。
- 唯一游戏模块：`UE_LearnFPS`。日志类别：`LogUE_LearnFPS`。
- `Source/UE_LearnFPS/UE_LearnFPS.Build.cs` 把 `CustomFPS` 各子目录加进了 `PublicIncludePaths`。短文件名和 `CustomFPS/...` 两种 include 都能编过。新增子目录时要补这条路径。
- 开火音效依赖插件 `Metasound` 和模块 `MetasoundEngine`。`UMetaSoundSource` 的头文件是 `MetasoundSource.h`。
- 开火特效依赖插件 `Niagara` 和模块 `Niagara`。生成用 `UNiagaraFunctionLibrary::SpawnSystemAttached`。
- 非 Unity 构建按 cpp 文件名区分编译单元。`CustomFPS` 根目录不能再放一份和子目录同名的 `.cpp`。2026-10-12 曾因此退出码 6。
- 编辑器目标编译：

```bat
D:\UE\UE_5.8\Engine\Build\BatchFiles\Build.bat UE_LearnFPSEditor Win64 Development -Project="D:\UnityProject\FPS\UE_LearnFPS\UE_LearnFPS.uproject" -WaitMutex -FromMsBuild -architecture=x64
```

- 打开工程会跑 `Tools/Clangd/Watch-Source.ps1`。保存 C++ 后它调用 `Tools/Clangd/Refresh-Code.ps1`，用 `Intermediate/Build/.../UnrealEditor/Development/<模块>/*.Shared.rsp` 重写项目根的 `compile_commands.json`。没有这份 rsp 时才退回生成 `.vscode/compileCommands_*.json`。`clangd.onConfigChanged` 必须是 `restart`，并且 `clangd.onConfigChangedForceEnable` 为 true，否则 clangd 23 不会在数据库变化时重启。改了 `Build.cs` 包含路径后，要先成功编译一次编辑器目标，rsp 才会变。
- 默认地图 `/Game/FirstPerson/Lvl_FirstPerson`，默认 GameMode `/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode`。自定义模式是 `/Game/CustomEditorBP/BP_CustomFPSGameMode`。不要假设一进 PIE 就是自定义角色。
- 注释、蓝图显示名、文档用简体中文。

## 什么该动

正在做的系统全在 `Source/UE_LearnFPS/CustomFPS/`。

模板文件先留着，没有明确要求不要改、不要删：

- `UE_LearnFPSCharacter`、`UE_LearnFPSGameMode`、`UE_LearnFPSPlayerController`、`UE_LearnFPSCameraManager`
- `Variant_Shooter/`、`Variant_Horror/`
- `Content/FirstPerson/`、`Content/Weapons/`、`Content/Variant_*`

`Content/XL_FPSPack/` 是外部资源包，只当网格和动画来源，不在里面改逻辑。

## 架构约束

角色是 `ACustomFPSCharacter`：`ACharacter` + `ICustomFPSWeaponHolder`。能力在四个组件上，角色负责创建、转发动画查询、开火和死亡。

组件挂载：

- `FPSInput`：`UCustomFPSInputComponent`
- `FPSLocomotion`：`UCustomFPSLocomotionComponent`
- `FPSView`：`UCustomFPSViewComponent`
- `FPSWeapon`：`UCustomFPSWeaponComponent`
- `FirstPersonCamera` 挂在胶囊体上，初始高度 `BaseEyeHeight`（64）
- `ArmsAnchor` 和相机同级，也挂在胶囊体上
- `FirstPersonMesh` 挂在 `ArmsAnchor` 下，`SetOnlyOwnerSee(true)`，`EFirstPersonPrimitiveType::FirstPerson`
- `GetMesh()` 是第三人称身体，`SetOwnerNoSee(true)`，`WorldSpaceRepresentation`。自己看不见，但镜头要读它的骨骼，所以 `VisibilityBasedAnimTickOption` 必须是 `AlwaysTickPoseAndRefreshBones`

相机晃动只加在相机上。手臂和枪跟着 `ArmsAnchor`，不跟着相机。不要把 `FirstPersonMesh` 或武器改成相机的子物体。

`UCustomFPSViewComponent` 在热重载和重新实例化后，角色上的 `FPSView` 指针会脏。`RepairViewComponent` / `RepairAllViewReferences`，以及 `ReloadCompleteDelegate`、`OnObjectsReinstanced` 上的静态委托，是用来把指针指回去的。不要当成无用代码删掉。

武器是抽象 Actor `ACustomFPSWeapon`。`UCustomFPSWeaponComponent` 只保存 `WeaponClass`，在游戏世界里 `SpawnActor` 后调用 `Equip(ArmsMesh)`。编辑器里 `World->IsGameWorld()` 为假，不会生成。CDO 上也不生成。部件网格在武器的 `RebuildParts()` 里建，不建在角色上。

武器通过 `ICustomFPSWeaponHolder` 和持有者说话，不 include 角色类。输入开火走接口，不直接调武器。请求者参数是发出开火的 Actor，玩家把自己传进去。

动画蓝图读的是角色上的 `BlueprintPure` + `BlueprintThreadSafe` 函数（`GetMoveX`、`GetAimPitch`、`GetAimSwayAlpha`、`IsFiring` 等）。这些函数转发到组件。给动画蓝图加数据时，加在角色的这层接口上，并保持线程安全。

## 数据流

输入在 `PawnClientRestart` 里 `AddMappingContext`，优先级 10，高于模板 `IMC_MouseLook`（0）。`UnPossessed` 时移除。默认资源用 `ConstructorHelpers` 指到 `/Game/CustomEditorBP/Input/`：

| 资源 | 行为 |
| --- | --- |
| `IMC_CustomFPSInput` | 映射上下文 |
| `IA_Move` | Triggered，交给移动 |
| `IA_Look` | Triggered 和 Ongoing，交给视角 |
| `IA_Aim` | 按住瞄准，松开结束 |
| `IA_Run` | 按住跑步。Started 会打断下蹲；按住期间的 Triggered 用 `StartSprint(false)`，不再清下蹲 |
| `IA_Squat` | 按住下蹲 |
| `IA_Sideways` | 轴值侧身，松开归零 |
| `IA_Fire` | 按住开火。代码里还有一次 `LoadObject` 兜底 |

死亡时 `IsOwnerDead()` 吃掉移动、视角、瞄准、跑步、下蹲、侧身。开火本身由角色 `StartFire` 拒绝。

`Move(Right, Forward)` 用控制器偏航：`Forward` 加在角色右方向，`Right` 加在角色前方向。注释写明这是为了对齐当前 `IA_Move` 的轴。没核对输入资源之前，不要把这两轴对调回去。

移动速度由状态决定，单位厘米/秒。下蹲优先于跑步：

- 蹲：瞄准 20，否则 60
- 跑：500。瞄准时 `StartSprint` 直接返回
- 瞄准走：100
- 普通走：250

下蹲不是 `ACharacter::Crouch()`。`UpdateCrouch` 自己插值胶囊半高（站 96，蹲 56）、眼睛高度和身体网格 Z。`bIsCrouching` 表示想蹲，不表示插值已经结束。站起来前会做胶囊重叠检测。

视角：

- `Look` 写控制器旋转。灵敏度默认各 0.5
- 开镜进度 `AimSwayAlpha` 在 `AimDuration`（默认 0.18 秒）内从 0 走到 1。FOV 从 105 插到 60
- `GetAimExitAlpha()` 是 `1 - AimSwayAlpha`
- 腰射相机跟随 `CameraAttachBone`（默认 `Head`）。找不到骨骼就固定在眼睛高度。胸前 bodycam 用朝前的插槽，不要直接用 head 的骨骼轴向
- 骨骼晃动的旋转和位移有腰射、瞄准两套倍数和上限，随 `AimSwayAlpha` 混合。瞄准位移倍数默认 0，镜头贴住旋转支点
- 开镜抖动用 `AimKickCurve`，横轴 0 到 1，叠在相机上
- 侧身是相机滚转，最大 20 度，向右为正
- Tick 在 `TG_PrePhysics`，并尽量排在 PlayerCameraManager 取景之前

枪：

- 装备插槽默认 `HandGrip_R`，瞄准插槽默认 `AimSocket`
- `GetAimSocketLocation()` 给动画蓝图，用来把枪对到头
- `BeginPlay` 把弹夹填满，手动模式枪膛就绪
- `CanFire`：有弹、手动模式枪膛就绪、距上一发超过 `60 / FireRate` 秒
- `FireRound` 扣一发、播动画、调用 `OnRoundFired`。手动模式打完把 `bChamberReady` 置假
- 全自动用定时器连射。半自动只在按下时打一发。手动要再调 `CycleBolt()`
- 换弹：普通模式补满弹夹；手动模式每次从备用弹搬 1 发
- 开火动画：瞄准用瞄准蒙太奇；否则移动用走路蒙太奇，没填则退回待机。跑步不播。角色蒙太奇经 `PlayFireAnimation` 播在 `FirstPersonMesh` 上，枪的蒙太奇播在武器网格上
- 现在没有射线、抛体或伤害。`FireRound` 里有 `UE_LOG` 和 `AddOnScreenDebugMessage` 打 `"True"`，这是临时调试
- `FireSound` 默认是 `/Game/MetaSound/Weapon/00_Mcx/MS_00_MCX`。它实现了 `UE.Source.OneShot`，播放时引擎自己触发 `On Play`，代码不要再发 Trigger。`FireEffect` 默认是 `/Game/XL_FPSPack/Weapons/Common/VFX/Muzzle_Smoke/Muzzle_Smoke`。资源包里没有枪口火焰 Niagara，`M_Muzzle_Flash_Mat` 只是材质；这份默认特效是枪口烟雾。`PlayFireSound` 和 `PlayFireEffect` 都在 `FireRound` 里调用，共用 `ResolveMuzzle`：挂在部件静态网格上，不挂在 `WeaponMesh` 上。`FireSoundPart` 默认 `Barrel`，`FireSoundSocket` 默认 `Muzzle`。`DMR-X_Barrel` 的 `Muzzle` 插槽就在这根枪管模型上。`RebuildParts` 给每个部件组件打上部件名标签，`FindWeaponPart` 靠这个标签查找。插槽不存在就挂在部件原点。部件没生成时改在武器原点播放。特效在激活前设成 `EFirstPersonPrimitiveType::FirstPerson` 和 `SetOnlyOwnerSee(true)`，和武器网格同一条第一人称通道。留在世界通道时，持枪相机看不到，F8 的场景相机可以看见。`Equip` 必须先 `bEquipped = true` 再 `RebuildParts`，最后 `ApplyFirstPersonView`，否则新建的枪管部件不在第一人称通道里。池化不用，避免复用到还在世界通道的组件。没打出子弹不播。每发各生成一次，连射可以重叠

死亡 `SetDead(true)`：停火、收镜、停跑、停蹲、侧身归零、`StopMovementImmediately`。没有调用它的生命系统。

## 源文件

| 文件 | 职责 |
| --- | --- |
| `CustomFPS/Character/CustomFPSCharacter.*` | 组装组件、网格父子关系、动画查询、开火转发、死亡、热重载修好 `FPSView` |
| `CustomFPS/Input/CustomFPSInputComponent.*` | Enhanced Input 绑定和转发 |
| `CustomFPS/Locomotion/CustomFPSLocomotionComponent.*` | 速度、走跑蹲瞄准互斥、胶囊下蹲、动画用速度 |
| `CustomFPS/View/CustomFPSViewComponent.*` | 鼠标、FOV、骨骼跟随、晃动、开镜抖动、侧身 |
| `CustomFPS/Weapon/CustomFPSWeaponComponent.*` | 运行时生成和销毁一把武器 |
| `CustomFPS/Weapon/CustomFPSWeapon.*` | 网格、部件、弹药、射击方式、开火动画 |
| `CustomFPS/Weapon/CustomFPSWeaponHolder.*` | 持枪者接口 |
| `CustomFPS/Animation/CustomFPSAnimLibrary.*` | `GetAnimPlayLength`，给序列评估器的显式时间 |

## 内容资源

- `/Game/CustomEditorBP/BP_MainPlayer`：自定义角色蓝图
- `/Game/CustomEditorBP/BP_CustomFPSGameMode`：自定义模式
- `/Game/CustomEditorBP/Input/`：上面的 IMC 和 IA
- `/Game/CustomEditorBP/Weapon/BP_MCX300`：当前武器，`ACustomFPSWeapon` 的蓝图子类
- `/Game/Player/Anims/Player/ABP_MainPlayer`、`BS_MainPlayer`：手臂动画
- `/Game/Player/Anims/Weapon/ABP_MCX300`、`BS_MCX300`：枪动画
- `/Game/XL_FPSPack/Player`、`/Game/XL_FPSPack/Weapons/MCX-VIRTUS`：网格来源
- `/Game/MetaSound/Weapon/00_Mcx/MS_00_MCX`：MCX 开火 MetaSound。图里随机播放 `XL_FPSPack` 的 `SW_MCX-VIRTUS_Fire_01` 到 `_09`

## 还没做

- 命中检测、伤害、受击
- 换弹和拉栓的输入
- 跳跃
- 把默认地图和 GameMode 换成自定义资源
- 去掉 `FireRound` 的屏幕调试输出

做完一项，把对应句子从这里和 `README.md` 的「还没接上」里移到已完成。
