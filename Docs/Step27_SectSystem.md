# 第 27 步：宗门系统

## 完成范围

本步已用 UE 5.7 C++ 完成四宗门加入、每日任务、宗门贡献、贡献商店、功法奖励、TBH 横条界面与 v15 持久化。宗门是一级系统，不改变现有 Taskbar Hero 摄像机、玩家位置或自动战斗状态。

## 四宗门与加入条件

| 稳定 ID | 宗门 | 条件 | 宗门功法 |
| --- | --- | --- | --- |
| `QingyunSect` | 青云宗 | 炼气、青云山第 1 关 | 基础吐纳诀 |
| `HeavenlySwordSect` | 天剑宗 | 炼气、青云山第 25 关 | 青云剑诀 |
| `MyriadDemonValley` | 万妖谷 | 筑基、万妖林第 1 关 | 焚天诀 |
| `DemonSect` | 魔宗 | 金丹、幽冥谷第 1 关 | 九霄雷法 |

当前版本加入后不可退宗或改投。加入前可查看四宗门的说明、门槛和真传功法；加入后界面会锁定其他选择并明确显示该规则，避免误操作。

## 每日任务与贡献

宗门每日任务使用独立的中国标准时间（UTC+8）日键和 UTC 高水位：

- 除妖卫道：击杀 20 只怪物，奖励 60 贡献。
- 推进历练：推进 3 个关卡，奖励 90 贡献。
- 斩破守关：击败 1 名 Boss，奖励 120 贡献。

怪物死亡由 Spawner 记录普通击杀；只有地图进度成功写盘后，才会记录关卡和 Boss 指标，防止地图存档失败后通过重启重复获得宗门任务进度。三项任务达到上限后，继续杀怪不会再触发额外宗门同步写盘。

每日刷新具有以下规则：

- 同一时间和同一天重复调用保持幂等。
- 只生成当前日状态，不追补错过日期的任务或奖励。
- 跨日重置任务与每日兑换次数，但永久兑换记录保留。
- 系统时间小于宗门高水位时冻结进度、领奖和兑换，并且状态零修改。
- 任务奖励必须手动领取；重复领取不会增加贡献。

身份按累计获得贡献显示：外门弟子、内门弟子、真传弟子、宗门长老。当前贡献、累计获得、累计消耗和领取次数分别保存，便于后续扩展宗门等级与统计。

## 宗门贡献商店

每个宗门拥有四类兑换：

- 宗门物资：40 贡献，每日 5 次。青云宗为灵草 ×5、天剑宗为灵铁 ×2、万妖谷为妖丹 ×3、魔宗为法宝碎片 ×1。
- 灵石俸禄：80 贡献获得灵石 ×250，每日 3 次，累计获得 60 贡献后开放。
- 悟道玉简：150 贡献获得悟道点 ×1，每日 2 次，累计获得 150 贡献后开放。
- 宗门真传：500 贡献永久领悟本宗功法，累计获得 300 贡献后开放，每个存档只能兑换一次。

材料满堆、灵石或悟道点到达上限、功法已经领悟、贡献不足、每日次数耗尽或累计贡献门槛不足时均不会扣除贡献。宗门功法直接安全加入功法库，不占用玩家当前的两个功法槽；玩家可在功法界面自行决定装备。

## 事务与存档

`UImmortalPathSaveGame::CurrentSaveVersion` 已升级为 `15`，新增 `FImmortalSectState`：

- 宗门 ID、当前/累计贡献和任务领取统计。
- 独立任务日键与 UTC 高水位。
- 三项每日任务进度。
- 四项兑换的每日次数与永久次数。
- 独立修订号。

v1–v14 存档从当前 UTC 创建“未加入、0 贡献、当日空任务”状态，不追溯产生宗门进度或奖励。迁移同时检查版本号与 `bInitialized`，即使 Spawner 先把旧档写成 v15，也不会漏掉宗门字段初始化。迁移保存仍位于离线收益结算之后，不会提前改变离线时间基准。

加入、任务领奖和兑换均遵循“候选状态 → 写盘 → 成功后发布事件”的顺序。兑换会同时快照宗门、材料、灵石、悟道点、功法库及修订号；写盘失败会全部恢复，不发布成功事件。功法奖励不会调用普通 `LearnTechnique()`，因此不会错误消耗普通学习材料，也不会在存档成功前发出功法事件。

开发参数 `-ImmortalTestForceSectSaveFailure=Exchange` 可注入兑换写盘失败。实机验证中，贡献、材料、功法数量、兑换次数与修订号均成功回滚，重启后没有重复奖励。

## TBH 界面与输入

- 按 `J` 或点击底栏“宗门 [J]”打开。
- 玩家状态栏宽度由 1420 扩展为 1520，新增按钮不会被裁切。
- 宗门采用 `1600×300` 原生横条，而不是缩成难以阅读的 `900×600` 高屏模态。
- 左区显示四宗门，中区显示三项任务，右区显示四项兑换；顶栏显示当前宗门、身份、贡献、灵石和任务摘要。
- 宗门与背包、炼丹、炼器、法宝、功法、流派、百宝阁、地图、洞府和灵田保持互斥。
- UMG DPI 补偿后，Windows 实际 `1707×320` 窗口中面板完整居中，无裁切。
- `NativeTick` 只比较宗门、功法、材料修订号和灵石变化，不刷新日期、不修改状态、不写盘。

## 验证结果

- `ImmortalPath.Sect` 专项自动化：2/2 通过。
- `ImmortalPath.*` 全项目自动化：14/14 通过。
- `ImmortalPathEditor Win64 Development`：通过。
- `ImmortalPath Win64 Development`：通过。
- 真实 v14 存档迁移：通过，迁移为 v15 且原角色、装备、材料、法宝、功法、地图、洞府和灵田继续恢复。
- 魔宗加入、三任务完成/领取、贡献总账、九霄雷法兑换：通过。
- 材料、灵石、悟道点三类兑换：通过。
- 重启持久化、真传防重复和每日/永久次数恢复：通过。
- 强制兑换写盘失败与再次重启：通过，贡献 `770→770`、材料不变、功法 `2→2`、宗门物资次数仍为 `0`。
- TBH 截图人工检查：通过，`1600×300` 宗门面板在 `1707×320` 窗口完整可见。
- 所有运行日志均未发现 `Fatal error`、`Unhandled Exception`、`Accessed None` 或 `Ensure condition failed`。
- 用户原存档已从独立 Step27 备份恢复；主存档和备份 SHA-256 均为 `E3833E79F60F31F93EFCAD69640DE8FC3010971BF23914AE9F29DFD6CF01B86A`。

主要验证证据：

- `Saved/Logs/Step27SectCoreTestsFinal.log`
- `Saved/Logs/Step27FinalTests.log`
- `Saved/Logs/Step27RuntimeMigrationJoinTasksManual.log`
- `Saved/Logs/Step27RuntimeRestartPersistence.log`
- `Saved/Logs/Step27RuntimeForcedExchangeRollback.log`
- `Saved/Logs/Step27RuntimeRollbackRestart.log`
- `Saved/Logs/Step27RuntimeMaterialExchange.log`
- `Saved/Logs/Step27RuntimeSpiritStoneExchange.log`
- `Saved/Logs/Step27RuntimeTechniqueInsightExchange.log`
- `Saved/Screenshots/Step27_SectJoinedTasksManual.png`
- `Saved/Screenshots/Step27_SectRestart.png`

## 主要代码

- `Source/ImmortalPath/Sects/ImmortalSectTypes.h/.cpp`
- `Source/ImmortalPath/Sects/ImmortalSectTests.cpp`
- `Source/ImmortalPath/UI/ImmortalSectWidget.h/.cpp`
- `Source/ImmortalPath/UI/ImmortalPlayerStatusWidget.h/.cpp`
- `Source/ImmortalPath/Characters/ImmortalPlayerCharacter.h/.cpp`
- `Source/ImmortalPath/Spawning/ImmortalMonsterSpawner.cpp`
- `Source/ImmortalPath/Save/ImmortalPathSaveGame.h`

当前无需等待新的美术素材。
