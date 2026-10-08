# 47B：R07 人界犬／妖狼与灵犬动作重制

实施：2026-10-06～2026-10-08。本批对应改进清单 R07B，只处理普通犬类与 SpiritHound。**R07B 的素材接入、动作状态、脚线／比例与本批回归已完成；R07 整项、Boss 美术和原生透明桌面验收不在本批完成声明内。**

## 范围与保护

- 原创五套左向四足犬／妖狼动作，各 8 帧；Idle/Move 8 FPS、Attack/Hurt 12 FPS、Death 10 FPS。源图、区域、脚锚、统一处理及 SHA 见 `ArtSource/MortalRealm/DesktopPixelV2/Dog/README.md`。
- 旧 `BP_DogMonster` 同时被普通犬和各 Boss 回退路径使用；因此不重绑定旧包。专用 commandlet 受保护地复制为 `BP_DesktopDogMonster`，只替换新包五动作和表现参数；普通犬默认路径指向新包，Boss 本体仍保留旧包。Boss／无尽副本召唤物也使用 ordinary alternate，所以犬召唤物同步换新动作／0.25 秒接触点；不宣称 Boss 战所有内容零变化。
- 生命 45、移动速度 100、攻击力 6、射程 320、间隔约 1.7 秒等旧犬玩法值保持；只修改接触点、受击播放时长、视觉倍率、脚线策略与血条视觉基线。正式攻击距离不重新平衡，不宣称完整近战触及表现。
- SpiritHound 接入新五动作、1.25 视觉倍率和真实玩家脚线／景深策略，保留近战伤害方式、成长数值、解锁费用及未拥有的初始状态，不改存档结构。
- Fox 与 Dog 共用参数化动作夹具，保留 Fox 入口、旗标及既有断言。Dog 使用真实近战追击而非 Fox 的远程判断；正式正常玩法不启用夹具。
- 导入器在完整预检后仅保存新犬包，临时禁用 Blueprint 编译自动保存，保护复制／编译中途不落盘；支持 `-VerifyOnly` 和幂等复验。保护旧包、数值、碰撞、组件变换及无关属性。
- 测试仅使用隔离 `-UserDir`；正式存档与旧 Boss 资源逐次核对哈希。法宝在制素材／脚本不加入本批提交。

## 验证记录

| 项目 | 最终证据 |
| --- | --- |
| 原图与图集只读重建 | 独立重建及 10/8 最终复验通过；五套 40 帧，0 重复／裁切／半透明，脚线跨度均为 0 像素；`Saved/Automation/R07Dog/Build/DogRebuildVerify20261008.log` |
| 整理工具单元测试 | 24 项通过，0 失败／取消／跳过；`Saved/Automation/R07Dog/Build/ArtToolTests20261008.log` |
| Editor 初次构建 | Attempt1 失败：两处反射只读导出的 parent const 类型不符、共享夹具外层漏捕获 Shot；已修正，不将失败当成功 |
| Editor 修正后构建 | Attempt2 编译链接成功，8 actions，74.01 秒；这不是最终 Spawner 接入后的构建验收 |
| UE 素材导入 | 退出 0，textures=5、sprites created=40、flipbooks created=5、packages=50，0 错误／警告；`Saved/Automation/R07Dog/Import20261007/Import.log` |
| 新独立 BP 绑定 | 退出 0，仅保存新 BP；完整玩法／组件快照及旧包保护通过；`Saved/Automation/R07Dog/Binding20261007/Binding.log` |
| 最终 Development Editor | Spawner 切换后实际编译、链接成功，4 actions，32.49 秒；`Saved/Automation/R07Dog/Build/EditorAttempt3_20261007.log` |
| 最终 Development Game | 实际编译、链接成功，11 actions，121.66 秒；`Saved/Automation/R07Dog/Build/GameAttempt1_20261007.log` |
| 完整 `ImmortalPath.` 自动化 | 80 成功、0 测试警告、0 失败、0 未运行；`Saved/TestReports/R07DogFinal20261007_01/index.json`，不是上批 76 项结果 |
| Dog 受控实测 | 216 检查、0 失败、唯一 PASS、退出 0、17/17 源截图；`Saved/Automation/DogArt/R07B_20261007_Run01/R07Dog.log` |
| Dog 正常地图观察 | 14 检查、0 失败、唯一 PASS、退出 0、3/3 源截图；`Saved/Automation/DogArt/Walking_20261007_Run01/R07Dog.log` |
| 最终脚本 Fox 受控回归 | 213 检查、0 失败、唯一 PASS、退出 0、17/17 源截图；`Saved/Automation/FoxArt/Regression_20261008_Run01/R07Fox.log` |
| 最终脚本 Fox 正常地图回归 | 13 检查、0 失败、唯一 PASS、退出 0、3/3 源截图；`Saved/Automation/FoxArt/WalkingRegression_20261008_Run02/R07Fox.log` |
| 绑定只读／幂等复验 | 两次退出 0，already up to date、no package saved，新旧 BP 哈希均不变；`Saved/Automation/R07Dog/VerifyOnly20261007`、`Idempotence20261007` |
| 错误目录／混合模式拒绝 | 各有唯一初始化前拒绝日志，目录内 0 存档，正式主档不变；`Saved/Automation/R07Dog/RejectedUserDir20261008`、`Saved/Automation/DogArt/RejectedMixed_20261008` |

- Dog 咬击观测间隔 0.267 秒，第 3 帧、一次真实伤害；灵犬近／远近战接触为 0.272／0.267 秒，符合 0.25 秒配置及采样容差。500 UE 远目标先追近：38 个移动样本，X 从 93.571 到 396.156，攻击／接触距离 197.415，追近前 0 次伤害；不是 Fox 的远程伤害替代。只有受控测试实例射程设为 100 隔离移动阶段，正式新 BP 射程 320 保持。
- Dog 正常地图自然观察到 4 个新 BP 演员，全部采样接地；Idle 出现 3 个不同姿势。灵犬 130 次样本全部接地、脚线／景深误差 0，69 个 Idle/Move 循环样本、60 次播放推进。静止主人下没有自然左右跟随样本，不作该声明；左右跟随由受控段覆盖。
- 最终 Fox 回归保持真实近／远伤害；接触观测 0.268／0.290 秒。正常地图看到 5 个真实妖狐，130 次 owner/pet 样本均接地，脚线／景深误差 0。没有自然 pet Move 样本，完整左右跟随仍来自受控段。
- Dog 与最终 Fox 两模式各 20 张源截图，实际 1707×320；逐张复核本批脚线、比例、朝向、死亡保持与完整边界。角色间遮挡不等于视口裁切。洋红色键背景是合成前证据，不是粉屏复现，也不证明原生桌面前景合成。
- 两组负向预检实际应用退出码均为 0，日志明确拒绝且 0 存档；不能把退出码 0 当接受运行。正常夹具外层会把日志 FAIL／缺截图判失败。

## Boss 资源隔离烟测

使用真实地图 BP 与既有开发入口，在隔离 NullRHI 游戏运行中重复只读输出实际演员标签、类、AttackFlipbook、AttackWindup 和真实 Spawner 覆盖值。不是仅检查原生默认对象，也不是 Boss 美术或击杀通关验收。

| 场景 | 实际结果与证据 |
| --- | --- |
| 四个世界 Boss | AzureScaleDragon、SevenStarDemonLord、CloudAbyssLeviathan、ChaosHeavenBeast 均实际进入 phase 3；本体为旧 `BP_DogMonster_C`、旧 dogattack、0.35 秒。犬召唤物为新 `BP_DesktopDogMonster_C`、新 Attack、0.25 秒。日志均到达正常退出，stderr 0；`Saved/Automation/R07Dog/BossSmoke20261007` 下各成功 Run.log。跨日续跑前四场 OS 退出码没有单独落盘，结论基于退出请求和正常关停日志，不夸大为已重新获取该进程退出码。 |
| 无尽第 10 层 phase 3 | 实际 Boss／犬召唤类与动作分别保持上述旧／新路线，实际进程退出 0；`Saved/Automation/R07Dog/BossSmoke20261008/EndlessFloor10/Run.log` |
| 青云山第 10／999 关 | 实际关卡 Boss 使用旧 BP、旧 Attack、0.35 秒；CSV 第 300 帧一次性 Quit 后实际进程均退出 0；`BossSmoke20261008/Stage10`、`Stage999` |

后三场的进程退出码、查询参数和证据行号保存在忽略目录 `Saved/Automation/R07Dog/BossSmoke20261008/Results.json`。四世界 Boss 对应 realm 0／2／4／6；使用 `ImmortalTestWorldBoss`、`ImmortalTestWorldBossPhaseThree`、`ImmortalTestLogWorldBoss`、`ImmortalTestExitAfterWorldBoss`。无尽使用 `ImmortalTestEndlessFloor=10`、Prepare/Start/PhaseThree/Log/ExitAfterEndless 既有入口。

共同只读查询在引擎初始化之后启动 CSV：`-ExecCmds="t.MaxFPS 30,CsvProfile FRAMES=1200"`，`-csvExecCmds` 中 `r60:GetAll` 每 60 捕获帧查演员 Tags／AttackFlipbook／AttackWindup 及 Spawner 两覆盖值／两 fallback。关卡加一次性 `300:Quit`，不是重复 `r300:Quit`；已核对本机引擎源码及实际执行日志。不使用会覆盖项目根旧截图的 ScreenshotMaps 入口。

## 失败迭代与保护

- Editor Attempt1 的三处项目编译错误已实修并重建，不归因硬件／工具链，也不跳过测试。复制 API 的 PostDuplicate 会同步编译蓝图，因而额外临时设置 `SaveOnCompile=Never`，防止用户偏好开启自动保存时提前写入半绑定包；未永久改用户设置。
- 10/8 Fox Walking Run01 为 13 检查／5 个覆盖失败：随机序列几乎只生成犬，唯一妖狐被击杀前仅 4 次样本、1 个 Move 样本，没有足够覆盖。没有脚线／景深错误；外层正确判失败，未用应用退出 0 掩盖。Fox 启动脚本与 Dog 一样加入 `-FixedSeed`，最终 Run02 保持同一地面、属性、正常战斗及全部原断言，通过 13 项。未延长或降低标准，也没有强制生成替代狐。
- 世界 Boss 首次 Azure 运行退出 3、没有有效运行日志；下一次在过早 `-csvCaptureFrames` 入口发生 `GRHIGlobals.IsRHIInitialized` 断言，未进入战斗。保留失败目录；改为初始化后 `ExecCmds` 开启同一只读 CSV 查询，四场正常运行。没有修改引擎、角色属性或战斗逻辑规避。
- 正式主档始终 SHA-256 `98DE75CD856463348A4547C4D7F5D202613A0EA984CB41CA675FB6F1FEF61579`；旧 Boss BP 始终 `481256276284FAF245C6CFDC4FAD6DF63AA45882DB7F44EA8DEDBCA9ED863A76`。10/8 所有实测后再次核对，均保持。
- 新 BP 最终 SHA-256 `852E63DA676FAB272FF1CF64419F91AE3DD3EBC8429A13D1FDC103C7A314CA5B`；只读／幂等绑定不重写资产。测试日志、截图、隔离存档和构建产物均留本机忽略目录，不上传；法宝在制文件原样保留。

## 验收门槛与边界

Dog 受控段检查完整待机、左右移动、真实咬击伤害时序、密集受击、致死取消伤害和末帧保持，以及实际 owned pet 的近／远近战追击、主人死亡／复活。受控段采用 Flying 和测试实例 WorldStatic 隔离，仅验证动作与业务状态，不把它当正常地面验收。

正常地图观察不改移动模式、地面、演员属性或战斗；必须看到自然生成的新犬 BP、真实 Walking、动画姿势推进、owned pet、玩家脚线／景深及视口边界。使用固定随机种子保证复现，不指定替代怪物。截图为源码视口合成前证据，不代表 Windows 原生透明桌面验收。

两模式外层脚本要求唯一 PASS、0 失败、应用正常退出及完整 17／3 张源截图；负向预检以拒绝日志和 0 存档证明保护，不能仅凭退出码判断。最终重新跑 Fox 两模式，避免共用夹具重构破坏上一部分。运行入口为 `Build/Tests/run_r07_dog_fixture.ps1` 与 `run_r07_fox_fixture.ps1`，指定 `Controlled`／`Walking` 和新的 RunName，拒绝覆盖已有证据。

R07 后续仍需逐个处理剩余实际演员、Boss 与旧战斗特效。多显示器／DPI、原生深浅桌面前景、长期挂机及 Shipping 仍属于 R11／R12；本批不能覆盖这些缺口。战斗不提供修为，人界美术完成前不扩展灵界／仙界。
