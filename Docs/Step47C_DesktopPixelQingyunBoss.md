# 47C：R07 青云妖王九尾狐五动作接入

实施：2026-10-13～2026-10-14。**本批完成青云山守关妖王五动作素材、独立资产绑定与受控状态/脚线/触点验证；R07 整项、其他 Boss、旧战斗特效与原生桌面验收未完成。** 不把素材数量、生成预览或应用退出 0 当成完整验收。

## 范围与实现

- 原创左向白/冰蓝九尾狐，Idle/Move/Attack/Hurt/Death 各 8 个独立姿势。40 帧全局唯一、0 半透明/裁切/重复警告，人工脚底轴 (256,336)、512×384、PPUU 2.56；统一整套缩放 0.7。完整来源、失败版本、哈希与整理提示见 `ArtSource/MortalRealm/DesktopPixelV2/QingyunBoss/README.md`。
- 使用内置 imagegen，程序只做受批准的透明清理、显式区域搬移、帧选择和人工锚点对齐，不绘制尾巴/肢体、不插入重复姿势。两个独立修正帧真实导出 1254×1254，按整画布 512/1254 同轴归一化，不按角色外框逐帧拉伸。
- 新 `BP_DesktopQingyunBoss` 从旧 `BP_DogMonster` 受保护地复制，旧资源不重绑定。保留基础生命 45、攻击 6、移速 100、射程 320、间隔 1.7、0.35 秒触点及所有非表现默认值/组件；地图升级/三阶段增益不重新平衡。新 Boss 显式保留原创白/冰蓝调色，旧类此开关默认 false。
- 只青云山地图守关路线使用新类。显式 `BossMonsterClass` 覆盖优先，其他七张地图及未知地图回退旧类；世界/无尽不经过该 resolver。普通犬与犬召唤继续 R07B 新资源。
- 原生构造用固定 `FClassFinder` 加载专用新类，由 Transient 类引用持有；不是依赖旧 Spawner 蓝图未保存过的继承软路径。素材先导入、之后切换构造引用并在新进程编译验证。静态启动依赖审查不能等同 Shipping Cook 通过，后者仍留 R12。
- 共用编辑器绑定驱动保留 Dog 原配置/保护，额外核对 Boss 的旧 Hurt 实际时长：旧 7/15 秒，新 8/(120/7) 秒，均 0.466666669；`HurtAnimationDuration` 属性回退仍 0.35 秒。新 Attack 12 FPS 的第 4 帧对应原 0.35 秒触点，完整片段 8/12 秒小于阶段三攻击 CD。
- 保存前验证磁盘摘要、旧包脏状态、可编辑默认值/组件、Actor 与 Sprite 变换；临时 `SaveOnCompile=Never`、编译 `SkipSave`，最后只保存新目标包。`DuplicateSingleObject(..., false)` 的 false 是“不弹覆盖询问”，不是禁保存参数；本机 UE 实现确认该复制 API 本身不保存。
- 血条基线最终为脚上 275 UE，Boss 额外 +35，视觉尺度 2×1.35=2.7，仅表现参数改变。真实试玩截图发现 185 基线遮尾，已上移，不累计偏移。

## 验证结果

全部游戏运行使用独立 `-UserDir`，正式存档不用于试验；成功数以报告/日志、完整截图集合及外层脚本综合判断。

| 项目 | 当前证据 |
| --- | --- |
| 独立素材重建 | 11 个中间 PNG、5 个最终 PNG 编码 SHA 全相同；51 个输入文件未变；40 帧独立、脚线 span=0、0 警告；`Saved/Automation/R07QingyunBoss/Rebuild20261014` |
| 六组整理/预览工具测试 | 60 成功、0 失败/取消/跳过；指定本机 Chrome 执行离线预览测试；`Tools/ToolsReview_20261014.md` |
| 素材导入 | 退出 0；5 纹理、40 Sprite、5 Flipbook、50 包，0 错误/警告；`Import20261014_02/Import.log` |
| 新独立 BP 绑定 | 首次创建及血条修正后重绑定均退出 0、0 错误/警告；`Binding20261014_01`、`Binding20261014_02` |
| 最终 Development Editor | 实际编译链接成功，Attempt4：8 actions，41.20 秒；`Build/EditorAttempt4_20261014.log` |
| 最终 Development Game | 实际编译链接成功，Attempt3：4 actions，49.32 秒；`Build/GameAttempt3_20261014.log` |
| 最终完整 `ImmortalPath.` | 83 成功、0 测试警告、0 失败、0 未运行，实际进程退出 0；`Saved/TestReports/R07QingyunBossFinal20261014_02/index.json` |
| Boss 受控实际运行 | 227 检查、0 失败、唯一 PASS、实际退出 0、12/12 截图；`Saved/Automation/QingyunBossArt/R07C_20261014_Run02/R07QingyunBoss.log` |
| Dog 受控回归 | 216 检查、0 失败、17/17 截图、唯一 PASS/退出 0；`DogArt/R07CRegression_20261014_Run01` |
| Fox 受控回归 | 213 检查、0 失败、17/17 截图、唯一 PASS/退出 0；`FoxArt/R07CRegression_20261014_Run01` |
| Dog 正常 Walking 回归 | 14 检查、0 失败、3/3 截图、唯一 PASS/退出 0；`DogArt/R07CWalkingRegression_20261014_Run01` |
| Fox 正常 Walking 回归 | 13 检查、0 失败、3/3 截图、唯一 PASS/退出 0；`FoxArt/R07CWalkingRegression_20261014_Run01` |
| 新 Boss VerifyOnly/幂等 | 两次退出 0、already up to date、no package saved，新包 SHA 不变；`VerifyOnly20261014_01`、`BossIdempotence20261014_01` |
| 原 Dog 绑定 VerifyOnly | 退出 0、no package saved，新旧 Dog BP 不变；`DogVerify20261014_01` |
| 击杀事务与重启 | boss/final × FailAttempt 0/1/2，6 击杀+6 重启 Audit 均唯一 PASS；`Safety/R07C_20261014_Safety01/Report.md` |
| 错误目录/混合模式 | 两次确有初始化前拒绝、目录内 0 sav、无玩家加载/保存日志；实际 OS 退出均 0，不能仅凭退出码判断；同 Safety 报告 |

## 受控动作实测细节

真实地图 Spawner 在青云第 10 关自然创建唯一专用 Boss，然后保存该实例用于阶段验证；另外生成真实新 BP 的受控实例测试动作。测试只对受控实例设置高生命、长 CD、禁随机暴击、隔离移动射程 100 和无掉落实体，正式 BP 玩法值不改变。

- 待机实际访问八个姿势：81 次播放推进、1 次完整回绕。左右 AI 追击有真实 X 位移与速度，右向仅水平镜像；停步回 Idle。
- 咬击实际玩家 HP 只下降一次，0.367 秒、第 4 帧触点；0.35 秒设置及采样容差保留。
- 连续真实 TakeDamage 不重置/倒退 Hurt 播放，能到末帧并保持，再按保留的实际窗口回 Idle。
- 真实 Attack 期间 0.117 秒注入致命伤，严格早于 0.35 秒触点；注入前玩家 HP 72→72，死亡后也不受延迟伤害。Death 到第八姿势，后续采样仍保持且不循环。
- Spawner 拥有的实际 Boss 通过伤害跨 66%/33% 阈值进入阶段二/三；阶段二攻击×1.18/攻速×1.12/移速×1.05、阶段三再×1.25/×1.18/×1.08、暴击+0.08 均实测保持；真实委托召唤 1 个再新增 2 个普通地图随从。该 Boss 没被击杀，关卡仍 10、kills=0。
- 12 张实际 1707×320 源截图逐张复核：脚线/景深误差 0，实际新 Boss 比例、左右朝向、完整视口边界、血条不遮尾、死亡保持和阶段状态正确。靠近攻击段存在角色相互遮挡，不等于画布裁切，也不是完整近战视觉触及验收。

夹具采用 Flying 与受控 WorldStatic/Pawn 隔离，保留实际初始化 Z、不手动抬/压脚线。这证明本批状态与表现策略，不证明 Boss 自然 Walking、全部地形、鼠标交互或 Windows 原生合成。Hurt 短暂反向甩头、Death 侧翻属于当前素材的可见动作，不宣称主观流畅度已经全面完善。

## Boss 路由隔离烟测

`Saved/Automation/R07QingyunBoss/RouteSmoke_20261014_BindingReview01/summary.json` 记录 8 个实际隔离 NullRHI 进程，全退出 0、无超时，0 Warning/Error/Fatal/Assertion。采用初始化后 CSV 的真实演员查询，不直接编辑存档文件；游戏自动读写仅发生在隔离 UserDir，正式存档不变。

- 青云山第 10/999 关：新九尾 BP、新 Attack、0.35 秒。
- 四个世界 Boss：苍鳞妖龙、七星魔君、云渊蜃主、混沌天兽实际进入 phase3，本体旧 Dog/旧 dogattack/0.35 秒；世界召唤包含新 Dog/0.25 秒。
- 无尽第 10 层：实际阶段三旧本体/旧攻击/0.35 秒，新 Dog 随从/0.25 秒；约 3.5 秒后玩家阵亡并恢复普通地图，不能当通关/长时稳定证明。
- 妖狼谷旅行成功但加载其第 1 关，只证明普通新犬路线；未强改妖狼谷关卡为守关 Boss 阶段。其他地图守关路线由全部八地图 resolver 自动化与源码支持，未额外宣称实机全部覆盖。

## 存档事务与保护

boss 正常/FailAttempt2：一次原子写、掉落实体 0→7、10→11；final 正常/FailAttempt2：一次写、0→7、第 999 关完成。FailAttempt1 首写失败则保留原关卡与计数、0→0 掉落，重启 Audit 仍回滚，任务/宗门/宠物/地图指纹一致；不存在第二次隐式进度写。未修改存档结构，战斗仍不提供修为。

正式 SaveGames 的全部 22 个文件及两项旧 BP 在安全回归各进程后都核对集合与 SHA，均未变。最终本批核对：

- 主存档：`98DE75CD856463348A4547C4D7F5D202613A0EA984CB41CA675FB6F1FEF61579`
- 旧 Boss BP：`481256276284FAF245C6CFDC4FAD6DF63AA45882DB7F44EA8DEDBCA9ED863A76`
- R07B 新 Dog BP：`852E63DA676FAB272FF1CF64419F91AE3DD3EBC8429A13D1FDC103C7A314CA5B`
- 本批九尾 BP：`97A5E9A6F6887C3FF335AE42CC1807FE46D030934C26354B95F626CD7F6B1A34`

## 失败记录与后续边界

- 10/13 Editor Attempt1 三处项目编译错误（两个 GeneratedClass 测试参数类型、缺少 UPackage 头）真实修复后重建，未跳过测试；Attempt2 成功；构造路线切换后的 Attempt3 14 actions、159.24 秒成功。Game Attempt1/2 均实际成功，最终小幅修正以 Attempt3 为准。
- 前次浏览器测试默认 Chromium 缺失导致一次启动失败；10/14 指定已安装 Chrome，保持原全部断言，六组 60 项重新通过。
- 首次导入因 PowerShell 将未整体引用的 manifest 参数拆分而退出 1，未写资产；整体引用参数后第二次成功。没有修改引擎/导入验证绕过错误。
- Boss Run01 为 146 检查、1 失败、仅 3 张截图，外层正确失败，虽然应用退出 0。首次截图位于待机覆盖窗口内，怀疑冷启动截图开销导致观测缺口；旧日志没有完整采样统计，不能据此断言唯一原因。将截图移动到完整循环检查之后并增加统计日志，原八帧/推进/回绕断言不降低，Run02 全通过。
- Run01 实际血条压尾，基线 185→275，重绑定、两目标重建、完整 83 项重跑并截图复验。死亡测试还补入攻击前 HP/真实起点/触点前致命时刻，防止事后仅比 HP 的假通过；启动脚本默认 600 秒限时，仅终止该次创建的进程并保留失败证据。

本批源截图的洋红色键底是合成前证据，不是退出粉屏复现，也不证明原生透明桌面；R11 的前景合成、多 DPI/多屏、长时运行与 R12 Shipping/Cook 仍未完成。Boss 技能/转阶段的专属动画、剩余真实演员与旧特效尚需逐部分处理，正式射程 320 与完整视觉触及问题未在此重新平衡。人界美术完成前不扩展灵界/仙界玩法，独立飞升动画保留。只同步本批源码/正式素材/配置/文档，不上传个人存档、缓存、日志、截图、构建产物，法宝在制改动原样保留。

## 2026-10-15 补验：真实地图 Boss 自然 Walking

本节补足上文明确未覆盖的自然接地移动，不改写 10/13～14 的历史受控验收。只增加非 Shipping 的只读观察夹具、隔离预检与启动脚本 Walking 模式；不修改正式演员、蓝图、数值、存档格式或美术资产。**本补验通过，R07 整项仍未完成。**

### 观察方式与保护

- 使用真实 `NewMap`、青云第 10 关、`FixedSeed` 与全新独立 `UserDir`。由原地图 Spawner 自然创建专用九尾 Boss，自动战斗保持开启；不主动生成替代演员，不传送、不改移动模式/碰撞/地面/摄像机/属性，不强制动作、阶段或死亡。
- 从早期初始化开始按游戏时间每 0.05 秒观察，共 18 秒；前两秒标为 settling 但不丢弃。按同一演员分别记录 Move、Idle、攻击/受击、实际接地、血量与可见性；消失或被销毁不能冒充死亡，血量变化也不推断玩家/宠物伤害来源。
- 接地要求 Walking/NavWalking、`IsMovingOnGround` 和真实可行走 floor 同时成立。除胶囊底加玩家脚底偏移的独立公式外，稳定后另比对双方实际胶囊底与 Sprite 脚点，防止共同漂浮仍被相对公式判对。
- Move/Battle 截图排队后至少隔一帧，实际请求前再次检查同一存活 Boss 的当前动作和双方完整视口边界，Move 还须保持接地移动；三张截图错帧串行，不用后续其他动作截图补足缺失状态。
- 仅接受 Walking 标志、精确 `ImmortalTestStage=10`、`FixedSeed` 与 `Saved/Automation/QingyunBossWalking` 的新子目录。其他 `ImmortalTest*`、禁自动战斗、混合夹具、目录内既有 `.sav` 或 Walking 截图均拒绝。预检在 Spawner 读档前和 Player 初始化前接入，单进程缓存一次结果，避免本次合法新建存档被第二次检查误认成旧残留。
- 启动脚本默认仍为 Controlled；Walking 必须有唯一 PASS、至少 16 个实际检查、0 FAIL、准确 3 张指定截图与进程退出 0。默认受控模式仍核对其原 12 张截图，不把退出 0 单独当成功。

可复跑命令（每次更换 RunName）：

```powershell
& Build/Tests/run_r07_qingyun_boss_fixture.ps1 -Mode Walking -RunName FreshWalkingRun -TimeoutSeconds 300
& Build/Tests/run_r07_qingyun_boss_fixture.ps1 -RunName FreshControlledRun -TimeoutSeconds 300
```

### 最终源码验证

| 项目 | 结果与本地证据 |
| --- | --- |
| Development Editor | Attempt4，5 actions，30.71 秒，编译/链接成功；`Saved/Automation/QingyunBossWalking/Build/EditorAttempt4_20261015.log` |
| Development Game | Attempt3，4 actions，44.94 秒，编译/链接成功；同目录 `GameAttempt3_20261015.log` |
| 完整 `ImmortalPath.` | 83 成功、0 测试警告、0 失败、0 未运行，进程退出 0；`Saved/TestReports/R07QingyunBossWalking20261015_02/index.json` |
| 自然 Walking 最终 Run03 | 16 检查、0 失败、唯一 PASS、3/3 源截图、进程退出 0；`Saved/Automation/QingyunBossWalking/Natural_20261015_Run03` |
| 原受控 Boss 回归 Run02 | 227 检查、0 失败、唯一 PASS、12/12 源截图、进程退出 0；`Saved/Automation/QingyunBossArt/WalkingRegression_20261015_Run02` |
| 5 组负向启动 | WrongUserDir、MixedModes、DisabledBattle、GrantedResources、WrongStage 全部在初始化前拒绝，每组 1 条拒绝、0 玩家加载/保存、0 `.sav`；`QingyunBossWalking/Negative_20261015_02`，根外目录案例在相邻 `QingyunBossWalkingRejected` 下 |
| 残留文件拒绝 | `Residual_20261015_01/RefusedAfterFix.log`：1 条拒绝、0 加载/保存/断言，退出 0；原 43 字节人工假档摘要不变、未生成新档 |
| 脚本及差异审查 | PowerShell 语法、差异空白检查通过；独立只读审查未发现交付阻断 |

Run03 同一真实 Boss 共 360 次样本全部为 grounded Walking，最长连续接地 360；Move 102 次、89 次播放推进、6 个不同姿势，真实朝玩家位移 445.429 UE；Idle 20 次、15 次推进、5 个姿势。Attack 18 次、Hurt 220 次，Death 0 次；没有自然击杀/通关/阶段三证明。Boss 血量下降采样 27 次，玩家下降 1 次，观察尾段玩家 91.348 HP；不据此区分各伤害来源。实际 BP 玩家射程 450、Boss 射程 320，未按原生头文件默认值误判或调整平衡。

双方完整同屏 345 次、表现对齐 360 次、脚线和景深最大误差均为 0；稳定后共同接地 321 次，实际胶囊底/脚点差均为 0；视觉比例保持 2.7，无非预期移动模式或累计缩放。最终三张 1707×320 源截图已逐张复核，九尾、脚底和抬高的血条可见；近战彼此遮挡不等于裁切，也不证明完整咬击视觉触及。洋红源视口仍不证明原生透明桌面合成、退出粉屏、多 DPI/显示器或 Shipping。

负向测试前后对正式 22 个 SaveGames 文件和 3 个新旧 BP 共 25 项逐一比对摘要；最终又核对正式主档和三项 BP，均与上文摘要一致。全部运行只写独立测试目录；不提交本地日志、截图、假档或构建产物，法宝在制文件保留。

### 失败保留与下一优先部分

- Editor Attempt1 的 UE 5.7 非 const `GetFlipbook()` 调用错误已修复，不改引擎。自然 Run01 虽有真实画面但 16 检查中 10 失败：回调清理自身 timer 时销毁唯一强引用，聚合证据提前失效。将 `Sample` 改为值传 `TSharedRef` 持有完整回调生命周期，保留全部断言；Run02 通过，后续预检时序修正后最终 Run03 再通过。失败日志均保留。
- 残留假档首次拒绝测试真实退出 3：Spawner 早于 Player 读档，尚未到原玩家侧预检即进入引擎解析并触发 FName 长度断言。将本夹具预检前移到 Spawner 最前并缓存后，同一 43 字节原样假档被提前拒绝，未再读入，最终全部测试重跑。
- **新增 R01 高优先补漏，尚未修复：** 普通无夹具标志启动的通用读取函数，对至少 32 字节的畸形存档仍先调用 `LoadGameFromMemory`，后做类型/EOF 检查；错误 magic 的引擎古老格式回退可把随机字节解释为超长类名。本次只解决 Walking 的拒绝时序，不解决通用解析安全。下一独立部分应先建立隔离子进程复现及有边界的头部/属性流检查，覆盖主档、备份、恢复和保存前读取，保持合法旧档迁移及禁止异常自动覆盖，再继续剩余 Boss 美术。

R07 尚余其他实际演员/Boss、专属技能与转阶段表现、旧战斗特效；R08 地图装饰、R11 原生桌面及 R12 发行验收仍按原边界保留。原飞升动画与战斗不提供修为的设计不变。
