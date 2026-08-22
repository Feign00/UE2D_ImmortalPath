# 第 31 步：无尽秘境系统

## 完成状态

本步骤已完成独立的无尽秘境玩法。玩家可从检查点进入秘境，角色与怪物继续自动战斗；通过当前层后自动进入下一层，主动退出、角色死亡或异常结算时结束本轮并恢复原地图挂机。

无尽秘境不会修改青云山等地图的 1–999 关进度，不增加地图击杀、宗门任务或击杀修为。修为仍只由独立的在线/离线修炼系统结算。

本步骤继续沿用 Taskbar Hero 式桌面底部横条，没有改回此前废弃的摄像机移动方案。

## 入口与界面

玩家可通过以下方式打开无尽秘境：

- 按 `N` 键。
- 点击顶部状态栏的“秘境”按钮。

原生 UMG 面板使用 1600×300 TBH 布局，显示：

- 最高通关层。
- 当前检查点。
- 累计首次通关层数和挑战次数。
- 待领奖励数量。
- 当前层类型、击杀进度、Boss 阶段和生命。
- 当前层强度与奖励预览。
- 开始、退出、重试待领奖励和关闭按钮。

开始挑战成功后面板自动收起，避免遮挡角色和怪物。检查点重打层会明确显示“本层奖励已领取，不会重复获得装备、灵石或材料”，不会再用新层奖励预览误导玩家。

## 楼层与检查点

当前原生规则支持第 1–9999 层，并预留 `/Game/GAME/Data/DT_EndlessDungeon` 数据表覆盖入口。数据表不存在或配置无效时自动使用完整的 C++ 规则。

楼层规则：

| 楼层 | 敌人 | 奖励特点 |
| --- | ---: | --- |
| 普通层 | 普通怪 ×3 | 灵石、材料 |
| 每 5 层且不是第 10 层 | 精英怪 ×2 | 灵石、材料、装备 ×1 |
| 每 10 层 | 三阶段 Boss ×1 | 灵石、材料、装备 ×2 |

生命、攻击和防御随楼层单调增长，精英与 Boss 阈值提供额外增幅；1–9999 层逐层检查未出现难度倒退。单层最多生成 2 件装备，避免无尽挂机快速灌满 30 格背包。

检查点每 10 层形成一段：

```text
max(1, (最高通关层 / 10) * 10 + 1)
```

例如：

- 最高 0 或 9 层：从第 1 层重打。
- 最高 10 层：从第 11 层开始。
- 最高 31 层：从第 31 层重打。

重打已通关楼层只负责恢复到历史最高位置，不重复生成奖励。只有严格等于“最高通关层 + 1”的新楼层才能写入新纪录和奖励。

## 自动战斗与TBH可视出生带

进入秘境时会先保存当前地图，暂停普通刷怪并清理旧怪；地图上的实体掉落不作为秘境奖励重新结算。

无尽怪物使用独立 RunId、楼层和类型标记。死亡回调在普通地图逻辑之前拦截，因此不会串入地图关卡、普通掉落或宗门任务。

为满足角色与怪物同时出现在 1707×320 横条中的要求，秘境不再使用全地图左右随机出生：

- 第一只敌人生成在玩家前方约 520 cm。
- 同层后续敌人向前间隔约 180 cm。
- Boss 生成在玩家前方可视位置。
- Boss 召唤物优先生成在 Boss 两侧，并以玩家前方作为最小 X 下限，Boss 靠近玩家后也不会把召唤物放到玩家身后或镜头左侧。

实机截图确认玩家保持在左侧，两个精英、Boss、召唤物和头顶血条均位于摄像机可见区域。摄像机仍使用现有 TBH 参数：

- 1707×320 无边框置底窗口。
- 横向前导 850 cm。
- 透视 FOV 115；正交相机时宽度 1600。

## Boss 三阶段

每 10 层的守层 Boss 使用三阶段状态机：

- 阶段 1：满血阶段。
- 阶段 2：生命降低到阈值后强化，并召唤 1 只怪物。
- 阶段 3：低生命狂暴，并召唤 2 只怪物。

召唤物带 `EndlessMinion` 标记，不计入 Boss 层击杀，也不产生通用装备、灵石、材料或修为掉落。只有当前 RunId、当前楼层的权威 Boss 死亡才能完成该层。

## 独立奖励与五秒提示

每个首次通关的新楼层都会生成一次独立奖励：

- 灵石。
- 通用材料。
- 第 5 层倍数的精英层装备 ×1。
- 第 10 层倍数的 Boss 层装备 ×2。
- 修为固定为 0。

奖励自动进入背包、材料库存和灵石余额。成功后显示 5 秒摘要，明确列出楼层、装备、灵石、材料以及“本次奖励不包含修为”。

实机结果：

- 第 1 层：装备 ×0、灵石 +12、材料 ×1。
- 第 5 层：装备 ×1；原存档 30/30 满背包时没有覆盖装备，而是保留为待领奖励。
- 第 10 层：装备 ×2、灵石 +39、材料 ×1，自动入包并成功写盘。

## 两阶段奖励事务

无尽奖励使用两阶段持久化：

1. 严格记录新最高层并预生成装备 GUID、品质、词条、灵石和材料。
2. 把“进度 + 完整 `PendingRewards`”作为第一份持久事务写盘。
3. 全部奖励成功进入库存后删除 pending，再进行第二次写盘。
4. 第二次写盘失败时回滚背包、已装备物品、材料、灵石、生命、法力和各修订号，保留第一步已落盘的 pending。
5. 下次启动自动重试；第二次重启不会重复发放。

额外防重与修复：

- `RewardId` 重复时只保留一份。
- 同一楼层即使出现不同 GUID，也只保留一份奖励。
- 有效 pending 可以反向修复损坏的永久最高层，防止补发后再次通关同层重复领奖。
- 最高层只按存档格式上限 9999 修复，不会因可变 DataTable 暂时降低最大层数而丢失。
- 待领奖励中的装备采用不查询实时套装表的结构性修复；已经预生成的未知套装 ID 和名称不会因内容表临时缺行而改变。

## 退出、失败与互斥

以下情况会结束本轮、清理秘境实体并恢复原地图挂机：

- 玩家主动退出。
- 玩家死亡。
- 开发/异常失败结算。
- 主战怪物意外销毁。
- 下一层生成失败。
- 楼层进度首次写盘失败。

秘境与地图切换、世界 Boss 双向互斥。实机运行时同时尝试切图和开启世界 Boss，两项都被拒绝。

失败、死亡和主动退出均不会产生本层奖励。地图 ID、关卡、击杀数和地图修订号保持不变；宗门修订号保持不变。挑战期间修为仍可随独立在线修炼自然增加，但没有击杀修为。

## SaveGame v19

SaveGame 版本升级为 19，新增：

- `bEndlessDungeonInitialized`
- `FImmortalEndlessDungeonState`
- 最高通关层
- 累计首次通关层数
- 挑战次数
- 最后通关时间
- 里程碑审计 ID
- 可重启恢复的预生成 `PendingRewards`
- 无尽秘境状态修订号

迁移与损坏恢复已验证：

- v18 首次启动：`loaded version 18 -> current version 19`，初始化无尽秘境状态。
- 第二次 v19 启动：`migration=false`，不会重复迁移。
- v19 外层/内层初始化标记不一致时，不清空原状态；先修复标记，再补发仍然有效的 pending。
- 旧地图、装备、法宝、材料、洞府、灵田、宗门和世界 Boss 数据不受无尽秘境迁移影响。

## 验证结果

### 自动化

- `ImmortalPath.EndlessDungeon.RulesScalingAndFloorKinds`：通过。
- `ImmortalPath.EndlessDungeon.ProgressionCheckpointAndNormalization`：通过。
- `ImmortalPath.EndlessDungeon.RewardBundleSafety`：通过。
- 全项目 `ImmortalPath.*`：23/23 通过，0 失败、0 Error。

### 构建

- `ImmortalPathEditor Win64 Development -DisableUnity`：通过。
- `ImmortalPath Win64 Development -DisableUnity`：通过。

### 运行时

- 普通层 3 怪、精英层 2 怪、Boss 层 1 Boss：通过。
- Boss 阶段 2/3 与 1/2 只召唤物：通过。
- TBH 前方可视出生带：通过。
- 地图切换与世界 Boss 互斥：通过。
- 主动失败、角色死亡与普通挂机恢复：通过。
- 入口写盘失败：挑战取消，挑战次数回滚。
- 楼层首次写盘失败：楼层、奖励和 pending 一起回滚。
- 奖励第二次写盘失败：完整 pending 保留。
- 重启补发：只补发一次；再次重启不重复。
- v19 初始化标记损坏：pending 保留、修复并补发。
- v18→v19 迁移与第二次启动：通过。
- 日志未发现 Fatal、Unhandled Exception、Accessed None 或 Ensure。

验证截图：

- `Saved/Screenshots/Step31_EndlessDungeon_UI.png`
- `Saved/Screenshots/Step31_EndlessDungeon_EliteWave.png`
- `Saved/Screenshots/Step31_EndlessDungeon_Battle_BossPhase3.png`
- `Saved/Screenshots/Step31_EndlessDungeon_Reward.png`

运行日志：

- `Saved/Logs/Step31_Automation_All.log`
- `Saved/Logs/Step31_Runtime_NormalFloor.log`
- `Saved/Logs/Step31_Runtime_EliteFloor.log`
- `Saved/Logs/Step31_Runtime_BossFloor.log`
- `Saved/Logs/Step31_Runtime_BossCameraFinalClean.log`
- `Saved/Logs/Step31_Runtime_Failure.log`
- `Saved/Logs/Step31_Runtime_PlayerDeath.log`
- `Saved/Logs/Step31_Runtime_SaveFailure_Start.log`
- `Saved/Logs/Step31_Runtime_SaveFailure_Commit.log`
- `Saved/Logs/Step31_Runtime_SaveFailure_Delivery.log`
- `Saved/Logs/Step31_Runtime_PendingRestart_1.log`
- `Saved/Logs/Step31_Runtime_PendingRestart_2.log`
- `Saved/Logs/Step31_Runtime_MarkerMismatch_Repair.log`
- `Saved/Logs/Step31_Runtime_Migration_v18_to_v19.log`
- `Saved/Logs/Step31_Runtime_Migration_v19_Restart.log`

测试前的主存档已恢复，恢复后 SHA-256 与备份一致：

```text
E3833E79F60F31F93EFCAD69640DE8FC3010971BF23914AE9F29DFD6CF01B86A
```

## 主要代码

- `Source/ImmortalPath/Endless/ImmortalEndlessDungeonTypes.h/.cpp`
- `Source/ImmortalPath/Endless/ImmortalEndlessDungeonTests.cpp`
- `Source/ImmortalPath/Characters/ImmortalMonsterCharacter.h/.cpp`
- `Source/ImmortalPath/Spawning/ImmortalMonsterSpawner.h/.cpp`
- `Source/ImmortalPath/Characters/ImmortalPlayerCharacter.h/.cpp`
- `Source/ImmortalPath/UI/ImmortalEndlessDungeonWidget.h/.cpp`
- `Source/ImmortalPath/UI/ImmortalCombatFeedbackWidget.h/.cpp`
- `Source/ImmortalPath/UI/ImmortalPlayerStatusWidget.h/.cpp`
- `Source/ImmortalPath/Save/ImmortalPathSaveGame.h`

## 素材需求

本步骤没有新的必需素材。现有 Fox、Dog、玩家、怪物血条和原生 UMG 足以完成无尽秘境全部功能。

以后只做表现升级时，可以选择补充：

- 精英怪专属轮廓或光效。
- 守层 Boss 独立角色与三阶段特效。
- 秘境入口图标和面板背景。
- 层数推进、检查点解锁和失败音效。

这些素材不是下一步开发的前置条件。

## 下一步

第 31 步完成后进入第四阶段的下一部分：宠物系统。宠物逻辑可先复用现有 Fox/Dog 做跟随、自动攻击和成长验证；到需要替换宠物图标、待机/移动/攻击/受击/死亡动画时再通知准备素材。
