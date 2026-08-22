# 第 30 步：世界 Boss 系统

## 完成状态

本步骤已完成独立世界 Boss 挑战系统。世界 Boss 不占用地图 1–999 关进度，不提供击杀修为，也不会推进普通击杀、守关 Boss 或宗门任务；进入挑战时暂停普通刷怪，胜利、超时、角色死亡或主动退出后恢复原地图挂机。

本步骤继续使用 Taskbar Hero 式桌面底部横条。没有改回旧摄像机方案，也没有改变 1707×320 窗口、角色中间偏左和来怪可见的运行方式。

## 世界 Boss 目录

当前内置 4 个挑战，并预留 `/Game/GAME/Data/DT_WorldBosses` 数据表覆盖与扩展入口：

| Boss | 解锁境界 | 推荐关卡 | 限时 | 保底装备 | 灵石 | 稀有材料 | 首通固定法宝 |
| --- | --- | ---: | ---: | --- | ---: | --- | --- |
| 苍鳞妖龙 | 炼气 | 30 | 75 秒 | 玄品及以上 ×4 | 360 | 妖丹 ×8、法宝碎片 ×3 | 玄光仙剑 |
| 七星魔君 | 金丹 | 160 | 90 秒 | 地品及以上 ×4 | 900 | 灵铁 ×10、法宝碎片 ×5 | 七星幡 |
| 云渊蜃主 | 化神 | 360 | 105 秒 | 天品及以上 ×5 | 1800 | 妖骨 ×14、法宝碎片 ×7 | 流云伞 |
| 混沌天兽 | 合体 | 650 | 120 秒 | 仙品及以上 ×6 | 3600 | 法宝碎片 ×22 | 混沌珠 |

Boss 定义、解锁、战斗倍率、技能参数、召唤数量和掉落均由 `FImmortalWorldBossDefinition` 驱动。DataTable 行名与 `BossId` 不一致时也可以按有效 `BossId` 查找。

当前原生面板固定展示前 4 个挑战。未来加入第 5 个以上 Boss 时，需要再增加分页或滚动选择，不影响本步骤的四个内置挑战。

## 独立挑战流程

玩家按 `V` 或点击顶部“妖王”入口打开 1600×300 世界 Boss 面板。面板显示：

- 四个 Boss 的解锁状态、推荐关卡和限时。
- 三阶段技能说明。
- 独立掉落池和首通法宝。
- 历史击杀次数与最佳时间。
- 当前阶段、生命和倒计时。
- 待领奖励数量与重试按钮。

点击“开始挑战”成功后，面板会立即收起，让玩家直接看到角色、Boss、头顶血条和召唤怪。挑战过程中禁止切换地图。

挑战状态流程为：

1. 保存当前地图进度。
2. 暂停普通刷怪并清理旧怪。
3. 在玩家右侧可见位置生成世界 Boss。
4. 胜利、超时、角色死亡或主动退出时结束独立挑战。
5. 清理 Boss 与召唤怪，恢复原地图和普通挂机。

连续发起挑战时会先清理上一场的延迟恢复计时器，避免第二场结束后普通挂机不能恢复。关卡卸载时会先关闭挑战状态并解除怪物委托，不会在世界销毁阶段误触发“Boss 意外消失”流程。

## 三阶段战斗

世界 Boss 复用并扩展已有守关 Boss 状态机：

- 阶段 1：100%–67% 生命。
- 阶段 2：66%–34% 生命，提高攻击与攻速，并召唤 2 只独立侍从。
- 阶段 3：33% 以下进入狂暴，再次强化并召唤 3 只独立侍从。

世界 Boss 具有真正的远程施法距离。它在普通近战范围外、技能范围内会停止移动并结算远程技能，而不是必须先贴到玩家身边。实机日志确认苍鳞妖龙在距离 427 时于阶段 1 发起远程技能；其普通攻击范围加技能范围为 740。

召唤怪使用世界 Boss 的推荐关卡独立缩放，不叠加当前地图难度。召唤怪的装备、灵石和材料通用掉率全部为 0，也不会增加地图击杀、关卡、宗门任务或修为。

世界 Boss 本体仍被装备“首领伤害”词条识别；召唤怪不属于 Boss，不会错误获得首领伤害判定。

## 独立掉落与自动领取

世界 Boss 不调用普通地图 Boss 的实体掉落池。胜利时只生成一次独立奖励事务：

- 配置数量和最低品质的高级装备。
- 配置数量的灵石。
- 稀有材料和法宝碎片。
- 首次击败时固定法宝。
- 修为奖励固定为 0。

奖励成功后自动进入背包/材料库存/法宝库存，并显示 5 秒摘要。摘要明确显示高级装备、灵石、材料、首通法宝以及“本次奖励不包含修为”。

### 两阶段持久化

为避免满背包、退出或磁盘写入失败造成丢失/重复，奖励使用两阶段事务：

1. 先记录胜利并预生成所有装备 GUID、材料、灵石和首通法宝，作为 `PendingRewards` 写入存档。
2. 全部奖励能够进入库存后删除 pending，再进行第二次写盘。
3. 第二次写盘失败时，装备、材料、法宝、灵石和修订号全部回滚，保留第一步已落盘的完整 pending。
4. 下次启动或打开世界 Boss 面板时自动重试。

待领奖励是自包含数据，不依赖当前 Boss/DataTable 行继续存在。以后 Boss 行改名或内容暂时不可用时，不会在读档归一化阶段静默删除已经持久化的奖励。

实机验证覆盖：

- 30/30 满背包时奖励保持为 1 份 pending，没有被覆盖或丢弃。
- 首次写盘强制失败时，胜利和奖励一起回滚，pending 为 0。
- 第二次写盘强制失败时，胜利和完整奖励保留为 1 份 pending。
- 重启后自动发放该 pending，装备 ×4、灵石 ×360、材料 ×2、玄光仙剑全部写盘，pending 变为 0。
- 再次重启后击杀次数为 1、最佳时间和首通法宝标记保留，没有重复发奖。

## 地图、宗门与修炼边界

世界 Boss 本体和召唤怪的死亡回调在普通地图进度逻辑之前独立拦截。

运行时分别验证了胜利、主动退出、超时和角色死亡：

- 活跃地图、当前关卡和挑战开始时的击杀数不变。
- 宗门状态与任务进度不变。
- 世界 Boss 奖励修为为 0。
- 独立修炼系统仍按时间继续运行，因此挑战期间修为可以自然增长，但不是击杀奖励。
- 挑战结束后 `active=false`，普通挂机恢复为 3 只怪物。
- 超时、角色死亡和主动退出均不产生奖励。

## SaveGame v18

SaveGame 版本升级为 18，新增：

- `bWorldBossInitialized`
- `FImmortalWorldBossState`
- 四个 Boss 的击杀次数、最佳时间、最后击杀时间和首通法宝标记
- 可重启恢复的 `PendingRewards`
- 世界 Boss 状态修订号

真实 v17 fixture 已验证：

- 首次启动：`loaded version 17 -> current version 18`，只初始化世界 Boss 状态。
- 地图、装备、套装、法宝、材料、洞府、灵田、宗门等旧数据继续保留。
- 第二次启动：`migration=false`，不会重复迁移。

第 29 步 `<17` 的装备扩展迁移边界保持不变。

## TBH 面板与摄像机

世界 Boss 面板使用原生 UMG 和现有底图，不依赖新的美术资源：

- 逻辑尺寸 1600×300。
- `V` 键和顶部“妖王”按钮均可打开。
- 开始挑战后自动关闭不透明面板。
- 战斗 HUD 顶部显示 Boss 名称、阶段、生命百分比和剩余时间。
- 第三阶段截图中角色位于左侧，Boss、召唤怪和血条均在 1707×320 视野内。

摄像机继续输出：

- `TBH taskbar window applied: 1707x320`
- 透视投影、FOV 115
- 角色中间偏左

本步骤没有重新实现摄像机，也没有采用此前废弃的摄像机移动方案。

## 验证结果

### 自动化

- `ImmortalPath.WorldBoss.CatalogAndProgression`：通过。
- `ImmortalPath.WorldBoss.RewardPersistenceSafety`：通过。
- 全项目 `ImmortalPath.*`：20/20 通过。

### 构建

- `ImmortalPathEditor Win64 Development -DisableUnity`：通过。
- `ImmortalPath Win64 Development -DisableUnity`：通过。

### 运行时

- v17→v18 迁移与 v18 重启：通过。
- 世界 Boss 面板、三阶段、远程施法、阶段 2/3 召唤和狂暴：通过。
- 胜利自动发奖与 5 秒摘要：通过。
- 满背包 pending、首次写盘失败、第二次写盘失败与重启补发：通过。
- 主动退出、超时、角色死亡：通过。
- 地图/宗门不串线、召唤物零通用掉落、普通挂机恢复：通过。
- 日志未发现 Fatal、Unhandled Exception、Accessed None 或 Ensure。

验证截图：

- `Saved/Screenshots/Step30_WorldBoss_UI.png`
- `Saved/Screenshots/Step30_WorldBoss_Battle_Phase3.png`
- `Saved/Screenshots/Step30_WorldBoss_Reward.png`

## 主要代码

- `Source/ImmortalPath/WorldBoss/ImmortalWorldBossTypes.h/.cpp`
- `Source/ImmortalPath/WorldBoss/ImmortalWorldBossTests.cpp`
- `Source/ImmortalPath/Characters/ImmortalMonsterCharacter.h/.cpp`
- `Source/ImmortalPath/Spawning/ImmortalMonsterSpawner.h/.cpp`
- `Source/ImmortalPath/Characters/ImmortalPlayerCharacter.h/.cpp`
- `Source/ImmortalPath/UI/ImmortalWorldBossWidget.h/.cpp`
- `Source/ImmortalPath/UI/ImmortalCombatFeedbackWidget.h/.cpp`
- `Source/ImmortalPath/UI/ImmortalPlayerStatusWidget.h/.cpp`
- `Source/ImmortalPath/Save/ImmortalPathSaveGame.h`

## 素材需求

本步骤没有阻塞开发的素材需求，现有 Fox、Dog、血条、装备光球和原生 UMG 已能完成全部功能验收。

后续美术润色时可以选择补充：

- 4 张世界 Boss 头像。
- 4 套独立 Boss 角色/动画。
- 远程技能弹道与范围预警特效。
- 阶段切换、召唤和狂暴特效。
- 世界 Boss 专属音效与首通演出。

这些素材都不是进入下一步开发的前置条件。

## 下一步

第 30 步完成后可以进入第四阶段的下一部分，优先开发无尽秘境；若要先完善表现，也可以先替换世界 Boss 头像、角色和技能特效。
