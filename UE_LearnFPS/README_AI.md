# README_AI

新对话先读本文件，再改代码。给人看的进度说明是 [README.md](README.md)。

行为、目录、资源路径、构建方式变了，两份一起改。本文件写约束、路径和当前实现；`README.md` 只保留结论，不抄类成员。

最近整理：2026-10-13。弹壳和子弹伤害同一天接上。

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

`Move(Right, Forward)` 用视角组件的 `GetBodyYaw()`（开自由瞄准时是镜头偏航，关掉时是控制器偏航）：`Forward` 加在角色右方向，`Right` 加在角色前方向。注释写明这是为了对齐当前 `IA_Move` 的轴。没核对输入资源之前，不要把这两轴对调回去。

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
- 枪惯性（`bEnableWeaponLag`，默认开）：控制器旋转是鼠标给的瞄准，`WeaponAimRotation` 用二阶弹簧追它（腰射 `HipWeaponLagFrequency` 5 赫兹、开镜 `AimWeaponLagFrequency` 9 赫兹，阻尼 `WeaponLagDamping` 0.65），落后超过 `WeaponLagMaxAngle` 椭圆时硬夹。`UpdateDrivenCamera` 里的 `LookRotation` 就是它（滚转取控制器上的侧身），手臂锚点、枪身冲击、调试红点都用它。以后打子弹用 `GetWeaponAimRotation()`，不要用控制器旋转，否则子弹和画面里的枪口不一致。两个弹簧共用 `StepAngleSpring`
- 自由瞄准（`bEnableFreeAim`，默认开）：镜头追的是 `WeaponAimRotation`（关掉枪惯性时它等于控制器旋转）。鼠标、后坐力上抬、压枪检测、`AimPitch` 还是读写控制器旋转。视角组件另存 `FreeAimViewRotation` = 镜头和身体朝向，`UpdateFreeAimView` 在 `UpdateDrivenCamera` 开头算：枪口偏离（控制器减镜头）在死区椭圆里时镜头不追；偏出时目标是把枪口拉回死区边，用二阶弹簧（`FreeAimFollowFrequency` 赫兹、`FreeAimFollowDamping`，帧长时拆小步）追，速度上限 `FreeAimMaxTurnSpeed`；最后用 `MaxOffset` 椭圆硬夹，枪不会出画面。死区和硬边都随 `AimSwayAlpha` 在腰射、开镜两套之间插值。`Look` 记录最后一次鼠标输入时间，停手 `FreeAimRecenterDelay` 秒后目标偏离每帧按 `FreeAimRecenterSpeed` 缩小，镜头回中。开着时每帧把 `bUseControllerRotationYaw` 设成 false，并在有控制器时 `SetActorRotation(镜头偏航)`，所以身体、`YawRotation`、第三人称 `AimYaw`（枪口相对身体）都跟镜头。移动组件 `Move` 用 `GetBodyYaw()`。相机旋转、骨骼参考位置 `MeshWorld` 用镜头朝向（滚转仍取控制器上的侧身）；`ArmsAnchor` 旋转用枪口朝向，位置绕 `FreeAimPivotOffset`（镜头局部，默认后 12 下 10）转过「枪口 × 镜头⁻¹」。`bDrawFreeAimDebug` 从锚点沿枪口朝向打线，画红点。只按单机写：服务器没有鼠标输入时间，联机时身体朝向的回中会和客户端不一致
- Tick 在 `TG_PrePhysics`，并尽量排在 PlayerCameraManager 取景之前

枪：

- 装备插槽默认 `HandGrip_R`，瞄准插槽默认 `AimSocket`
- `GetAimSocketLocation()` 给动画蓝图，用来把枪对到头
- `BeginPlay` 把弹夹填满，手动模式枪膛就绪
- `CanFire`：有弹、手动模式枪膛就绪、距上一发超过 `60 / FireRate` 秒
- `FireRound` 扣一发、播动画、调用 `OnRoundFired`，然后抛弹壳。手动模式打完把 `bChamberReady` 置假
- 全自动用定时器连射。半自动只在按下时打一发。手动要再调 `CycleBolt()`
- 换弹：普通模式补满弹夹；手动模式每次从备用弹搬 1 发
- 开火动画：瞄准用瞄准蒙太奇；否则移动用走路蒙太奇，没填则退回待机。跑步不播。角色蒙太奇经 `PlayFireAnimation` 播在 `FirstPersonMesh` 上，枪的蒙太奇播在武器网格上
- 命中用下面的弹道射线，没有抛体。肉伤、穿甲伤害、穿甲后肉伤保留、精度倍率记在 `ACustomFPSBullet` 上，枪用 `BulletClass` 的 CDO 读取，不在枪上再存一份伤害。结算规则：没护甲造成肉伤；有护甲先用穿甲伤害扣护甲，打穿后再造成肉伤乘以保留比例。还没有目标，所以这些数还不会扣血
- 弹道（射线，CQB 不做下坠）：`FireRound` 第一步调 `FireShot`，在动画和后坐之前，用这一帧的枪口。`ResolveBore` 取枪管部件（`FireSoundPart` 的 `FireSoundSocket`，即 `DMR-X_Barrel` 的 `Muzzle`）的世界变换，X 轴是枪管方向；没有部件时退回武器网格的 `Muzzle` 插槽位置加网格前方（网格自己的 `Muzzle` 插槽 X 轴朝后，不能用它的轴）。`GetShotAim`：枪口位置加枪管方向，子弹沿枪管直线飞。`GetSightLine`（`FPS|瞄具`）：从 `AimSocket` 位置出发（`AimSocket` 的 X 轴朝下，只用位置），先指向子弹在 `ZeroDistance`（默认 50 米）处的位置，再按 `SightElevationMOA`（正数弹着点上移，即瞄准线往下转）、`SightWindageMOA`（正数弹着点右移）转一个角度。枪的上方 = `AimSocket` 减枪口在垂直枪管平面上的投影，不用插槽轴向。近处子弹比红点低，约等于瞄具高出枪管的距离。`UpdateReticle` 由视角组件在 `UpdateDrivenCamera` 末尾每帧调用：不建动态材质、不换材质，只往 `ReticlePart`（默认 `Scope`）部件组件的自定义图元数据写世界方向，0~3 是瞄准线 `ReticleDirection`，4~7 是枪的上方 `ReticleUp`；镜片材质里这两个向量参数勾了「使用自定义图元数据」，下标和代码一致。镜片材质由网格资产指定（Romeo 3 的 `Holographc` 槽现在是 `M_RedDotLens_Inst`）。材质按视线偏离瞄准线的角度（角分）采样准星贴图，准星在无限远，没有视差，歪枪时跟着转。不要在构造函数里用 `ConstructorHelpers` 硬加载这个材质：启动时加载的资源会被锁定，命令行脚本 `delete_all_material_expressions` 会断言 `!IsRooted()` 崩溃。`FireShot` 先从 `GetPawnViewLocation()`（胶囊加眼高）连到枪口，挡住就算命中挡住处（防枪口插墙穿墙），否则从枪口打 `MaxRange`。忽略武器和持枪者，通道 `ShotTraceChannel` 默认 `ECC_Visibility`，打开了 `bReturnPhysicalMaterial`。命中调蓝图事件 `OnShotHit`。`bDrawShotDebug` 默认开：画线、命中点，屏幕打命中 Actor、组件、骨骼、距离。武器骨骼带 10 倍缩放，插槽数值要合成骨骼变换后再看，`Tools/Python/inspect_weapon_sockets.py` 只读打印这些。视角组件的调试红点也改成 `GetShotAim`，在 `UpdateDrivenCamera` 末尾、枪姿势算完后画，不带散布。专用射击通道、扣血还没做
- 散布（`FPS|散布`）：`FireShot` 在 `GetShotAim` 之后用 `GetCurrentSpread()` 的半角（度）扰动方向，再记这一发的连射散布，所以第一发不受连射影响。半角 = `WeaponMOA` × 子弹 `SpreadMultiplier` / 60 / 2（角分是直径）+ 连射散布 + 腰射 `HipSpreadAngle`（持枪者接口 `IsHolderAiming` 为假时）+ `MoveSpreadAngle` × 水平速度 / `MoveSpreadFullSpeed`（夹到 1）+ `AirSpreadAngle`（`GetMovementComponent()->IsFalling()`）。偏移角是二维正态，标准差为半角的一半，夹到半角，方位角均匀。连射散布每发加 `BloomPerShot`，上限 `BloomMax`，按 `BloomRecoveryRate` 度每秒懒衰减（存值和时间，读时扣掉）。子弹的精度倍率只乘枪固有精度。数值故意小：大部分不准来自看得见的枪惯性、自由瞄准和后坐，全填 0 就是纯归零射线
- 抛壳从 `WeaponMesh` 的 `EjectionSocket`（默认 `Ejection`）取世界变换，不从枪管部件取。没有该插槽时用骨骼 `Bolt`，再没有就用武器原点，并只警告一次。生成 `BulletClass`，调用 `EjectCasing`：换成发射后网格、第一人称可见、物理碰撞世界静态和动态、忽略角色。线速度是持枪者 `GetVelocity()` 乘 `EjectionInheritVelocity`（默认 1），再加插槽局部 `EjectionDirection` 乘 `EjectionSpeed`。不继承持枪者速度时，向右侧移开枪会让弹壳相对枪往左飞，看起来从枪左边穿出。`BulletClass` 为空或发射后网格为空则不留弹壳。弹壳到 `CasingLifetime` 销毁。`EjectCasing` 打开 `SetNotifyRigidBodyCollision` 并绑定 `OnComponentHit`；`HandleCasingHit` 用碰撞冲量除以质量估算撞击速度（冲量为零时退回刚体速度），低于 `CasingHitMinSpeed` 不响，两次间隔不足 `CasingHitInterval` 不响，一颗最多响 `CasingHitMaxCount` 次。音量 0.3 到 1 按速度映射，音高随机 ±5%。`CasingHitSound` 默认 `/Game/ShotSystem/Shot_SFX/SC_IG_WEP_Casing_Random`
- `FireSound` 默认是 `/Game/MetaSound/Weapon/00_Mcx/MS_00_MCX`。它实现了 `UE.Source.OneShot`，播放时引擎自己触发 `On Play`，代码不要再发 Trigger。`FireEffect` 默认是 `/Game/XL_FPSPack/Weapons/Common/VFX/Muzzle_Smoke/Muzzle_Smoke`。资源包里没有枪口火焰 Niagara，`M_Muzzle_Flash_Mat` 只是材质；这份默认特效是枪口烟雾。`PlayFireSound` 和 `PlayFireEffect` 都在 `FireRound` 里调用，共用 `ResolveMuzzle`：挂在部件静态网格上，不挂在 `WeaponMesh` 上。`FireSoundPart` 默认 `Barrel`，`FireSoundSocket` 默认 `Muzzle`。`DMR-X_Barrel` 的 `Muzzle` 插槽就在这根枪管模型上。`RebuildParts` 给每个部件组件打上部件名标签，`FindWeaponPart` 靠这个标签查找。插槽不存在就挂在部件原点。部件没生成时改在武器原点播放。特效在激活前设成 `EFirstPersonPrimitiveType::FirstPerson` 和 `SetOnlyOwnerSee(true)`，和武器网格同一条第一人称通道。留在世界通道时，持枪相机看不到，F8 的场景相机可以看见。`Equip` 必须先 `bEquipped = true` 再 `RebuildParts`，最后 `ApplyFirstPersonView`，否则新建的枪管部件不在第一人称通道里。池化不用，避免复用到还在世界通道的组件。没打出子弹不播。每发各生成一次，连射可以重叠

后坐力（已做：真实上抬、固定弹道、镜头冲击、枪身冲击；子弹散布见上面弹道部分）：

- 数据在 `UCustomFPSRecoilData`（数据资产，显示名「后坐力」），枪上 `RecoilData` 引用。空则没有后坐力
- 武器 `ApplyRecoil` 在 `FireRound` 里每发调用：两发间隔超过 `PatternResetTime` 时弹道从第 1 发重数。`EvaluatePattern(第几发)` = 每发上抬 × 上抬曲线（空曲线按 1），每发横移 × 横移曲线（空曲线按 0），再加随机，乘状态倍率（开镜、下蹲、移动相乘）。镜头冲击开镜时再乘 `AimCameraKickScale`
- 武器通过接口 `ApplyWeaponRecoil(数据, 准星上抬, 镜头冲击)` 交给持枪者，不引用角色类。接口还新增了 `IsHolderCrouching`
- 角色上的 `FPSRecoil`（`UCustomFPSRecoilComponent`，在 `View` 目录）：准星上抬在 `RiseDuration` 内平滑写进控制器旋转；每帧把控制器旋转的变化里与上抬方向相反的部分当成玩家压枪，从 `AccumulatedRise` 扣掉；最后一发后 `RecoveryDelay` 秒开始回落到 `累计 × (1 - RecoveryFraction)`，玩家压过目标的轴不再回拉。俯仰夹在 ±89，被夹掉的部分不记账
- 镜头冲击只在 `UpdateDrivenCamera` 里叠到 `ViewRotation`，不改控制器，不影响准星。手臂锚点用控制器旋转，所以枪会跟着准星上抬
- 枪身冲击：接口 `ApplyWeaponRecoil` 多带枪身位移（瞄准方向局部，X 向前，后退为负）和枪身旋转（Pitch 正为枪口上翘）。后坐力组件用和镜头冲击相同的两段插值：当前值追目标（`WeaponKickSnapSpeed`），目标回零（`WeaponKickReturnSpeed`）。`UpdateDrivenCamera` 设置 `ArmsAnchor` 时叠加：位置加 `LookRotation.RotateVector(位移)`，旋转为 `LookRotation * 枪身旋转`（四元数，局部空间）。不要另起一处设置 `ArmsAnchor`，会被每帧覆盖。骨架是 Biped，没有 `ik_hand_gun`，不要改单根手骨做后坐，左手会脱离护木。开镜时后退乘 `AimWeaponKickBackScale`（默认 0.15），因为开镜镜头贴着 `AimSocket`，后退会带着镜头一起退；旋转乘 `AimWeaponKickRotationScale`。开火蒙太奇的骨骼叠加照常叠在这之上
- 视角组件 `BeginPlay` 给自己加了后坐力组件的 tick 前置，保证同一帧镜头用的是加过上抬的控制器旋转
- 死亡时 `ResetRecoil`，死后不再接受后坐力

死亡 `SetDead(true)`：停火、收镜、停跑、停蹲、侧身归零、`StopMovementImmediately`。没有调用它的生命系统。

## 源文件

| 文件 | 职责 |
| --- | --- |
| `CustomFPS/Character/CustomFPSCharacter.*` | 组装组件、网格父子关系、动画查询、开火转发、死亡、热重载修好 `FPSView` |
| `CustomFPS/Input/CustomFPSInputComponent.*` | Enhanced Input 绑定和转发 |
| `CustomFPS/Locomotion/CustomFPSLocomotionComponent.*` | 速度、走跑蹲瞄准互斥、胶囊下蹲、动画用速度 |
| `CustomFPS/View/CustomFPSViewComponent.*` | 鼠标、FOV、骨骼跟随、晃动、开镜抖动、侧身、自由瞄准（镜头追枪口）、叠加开枪镜头冲击 |
| `CustomFPS/View/CustomFPSRecoilComponent.*` | 准星上抬、玩家压枪抵扣、松手回落、镜头冲击弹簧 |
| `CustomFPS/Weapon/CustomFPSRecoilData.*` | 每把枪的弹道曲线、随机、状态倍率、回落和镜头冲击参数 |
| `CustomFPS/Weapon/CustomFPSWeaponComponent.*` | 运行时生成和销毁一把武器 |
| `CustomFPS/Weapon/CustomFPSWeapon.*` | 网格、部件、弹药、射击方式、开火动画、按子弹抛壳 |
| `CustomFPS/Weapon/CustomFPSBullet.*` | 口径、弹头、肉伤和穿甲伤害、精度倍率、发射前/后网格、弹壳物理 |
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
- `/Game/CustomEditorBP/Weapon/M_RedDotLens`：红点镜镜片，受光的 Thin Translucent（工程开了 Substrate，引擎自动转成彩色透射混合），透明光照 Surface ForwardShading，世界空间法线，折射 Pixel Normal Offset。Substrate 下 Thin Translucent 的 Opacity 是「不透明顶层」比例，顶层底色黑，反射按菲涅尔不随透明度变淡，自发光直接叠加；透射颜色接在 `ThinTranslucentMaterialOutput` 上（`GlassColor`）。不要改回 Default Lit 加普通半透明：那样反射和红点都乘了透明度，玻璃越透越看不清。自定义节点 `Reticle UV`：`CameraVectorWS` 取反得视线，在 `ReticleDirection`、`ReticleUp`、右方（上方叉乘前方）组成的坐标系里取水平、竖直偏角，换成角分除以 `ReticleSize`（整张贴图的角分，默认 150）得贴图坐标，超出贴图或视线朝后时遮掉。贴图参数 `ReticleTexture`（默认 `T_Holographic_17`，星芒红点；`T_Holographic_15` 是十字）只取透明通道当形状，素材 RGB 是预乘过的。自定义节点 `Lens Mask`（瞄具网格局部空间的尺度，镜片位置取 UV0，相机向量转到局部）：不能用 `LocalPosition`，第一人称网格的像素世界位置被往相机缩放过，反推的局部位置会偏，整片镜子变黑。镜片 UV 是正面平铺投影，`LensUVCenter`（0.5, 0.4827）、`LensUVScale`（1 UV = 26.44 网格单位，往右、往上 UV 变小）。镜片外形是圆角矩形，`LensHalfSize`、`LensRight`、`LensUp`、上圆角 `TopRadius`、下圆角 `BottomRadius` 按 Romeo 3 网格第 2 段量出（`Tools/Python/inspect_scope_lens_geometry.py`，换瞄具要重量）；视线穿过镜片再往前走 `TubeDepth`（默认 2）落到镜框开口外就算被挡，斜看出现月牙黑边，偏移量按镜片半宽半高的比例夹到 `MaxShadow`（默认 0.3），平视镜片也不会整片盖黑，`EdgeSoftness` 是边缘柔化宽度；离边 `DistortWidth` 内法线往外弯 `EdgeBend`，配合 `EdgeRefraction` 折射拉扯边缘画面。输出局部弯折量和黑边。`Lens Normal`：顶点法线加转到世界的弯折量。`Lens Surface`：准星遮罩乘可见度，自发光 `DotColor` × `DotIntensity`，Opacity = 黑边 + 准星 × `ReticleOcclusion`。玻璃 `LensSpecular`、`LensRoughness`。参数组 `RedDot`。由 `Tools/Python/create_red_dot_lens.py` 生成，规则同下面的镜头材质。原素材的 `M_Holographc` 用 UV 偏移摆准星，对不上瞄准线，不再使用
- `/Game/CustomEditorBP/PostProcess/M_PP_BodycamLens`：后期处理材质，色调映射之后执行。一个自定义节点做完全部：按画面比例归一半径（角落 R2 = 1），三通道不同 K1 做色边，按 R2² 加 10 次螺旋采样做边缘模糊，圆角矩形距离场柔和渐隐边界，暗角。参数组 `Bodycam`：Strength、K1、K2、Zoom、Fringe、EdgeBlur、EdgeSoftness、CornerRadius、Vignette。实例 `M_PP_BodycamLens_Inst`。不要手改节点，改 `Tools/Python/create_bodycam_lens.py` 再生成。工程没开 Python 插件，用 `UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<脚本> -EnablePlugins=PythonScriptPlugin -unattended` 跑，结果写到 `Saved/BodycamLens_Result.txt`。这个进程是空渲染，不编译着色器，HLSL 错误要在编辑器里打开材质才看得到。编辑器开着时，它内存里的旧材质一保存就会覆盖新文件，要让用户先关材质窗口、跑完右键「重新加载」。引擎实际装在 `D:\Unreal\UE_5.8`
- `/Game/MetaSound/Weapon/00_Mcx/MS_00_MCX`：MCX 开火 MetaSound。图里随机播放 `XL_FPSPack` 的 `SW_MCX-VIRTUS_Fire_01` 到 `_09`

## 还没做

- 命中检测和受击。肉伤、穿甲伤害已经在子弹上，还没有结算到目标
- 换弹和拉栓的输入
- 跳跃
- 把默认地图和 GameMode 换成自定义资源
做完一项，把对应句子从这里和 `README.md` 的「还没接上」里移到已完成。
