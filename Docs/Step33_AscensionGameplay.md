# 第 33 步：飞升玩法

## 完成状态

本步骤已完成可重复飞升闭环。玩家修至终点境界并通关仙宫遗址后，可在青云山开启新一轮历练：

```text
完成本轮修炼与仙宫遗址
→ 返回青云山飞升
→ 获得仙印并记录历世地图成就
→ 境界与本轮八图重置
→ 青云山第 1 关立即恢复自动刷怪
→ 投资永久仙途
```

飞升继续使用 Taskbar Hero 式 1707×320 桌面横条视角，没有修改现有摄像机、人物站位或窗口运行方式。

## 飞升条件

飞升事务同时检查：

- 已到达终点“飞升”境界。
- 本轮仙宫遗址第 999 关已通关。
- 当前位于青云山。
- 世界妖王与无尽秘境均未进行。
- 飞升次数低于 999。
- 玩家未处于死亡状态。
- 地图生成器已完成初始化，且当前不在切图或怪物死亡结算中。

界面左侧显示五项长期玩法条件；死亡、地图切换和战利品结算属于瞬时安全门，未就绪时按钮会禁用并显示原因。

## 重置与保留规则

### 飞升时重置

- 大境界重置为炼气。
- 小境界重置为一层。
- 当前修为重置为 0。
- 本轮八张地图全部重置为第 1 关、0 击杀、未通关。
- 活动地图重置为青云山。
- 地面尚未拾取的装备、材料和灵石实体会随轮回场景清理。

生命和灵力不会因飞升免费回满，而是保留原值并按重置后的新上限截断。死亡状态不能利用飞升复活。

### 永久保留

- 已装备物品、背包装备、材料、丹药和任务物品。
- 灵石、装备掉落统计和背包管理状态。
- 法宝、功法、灵根和修炼流派。
- 百宝阁、洞府、种植和宗门。
- 世界妖王、无尽秘境和待领奖励。
- 灵宠拥有、出战、等级、经验和星级。
- 飞升次数、仙印、三条仙途。
- 每张地图的历世最高关卡与累计通关次数。

历世地图记录与当前轮回地图进度分离。这样既保留玩家成就，又避免“所有地图已通关，飞升后没有普通怪可刷”的死锁。

## 历世地图记录

`FImmortalAscensionState` 为八张已知地图各保存一条记录：

- `MapId`
- `HighestStage`
- `TimesCompleted`

每次飞升先把本轮地图状态合并到历世记录：

- 最高关卡只增不减。
- 本轮已通关的地图将累计通关次数加一。
- 重复、未知、越界或缺失记录会规范化为八张固定地图。
- 当前轮回重置不会删除历世记录。

最终实机验证记录为：

```text
lifetimeCompleted=8/8
finalLegacy=999/1
```

表示八张地图均已有历世通关记录，仙宫遗址最高第 999 关并通关 1 次。

## 仙印奖励

第 `N+1` 次飞升获得：

```text
clamp(3 + floor(N / 5), 3, 12)
```

示例：

- 第 1–5 次：每次 3 枚。
- 第 6–10 次：每次 4 枚。
- 后续每五次提高 1 枚。
- 单次最多 12 枚。
- 飞升最多 999 次。

`TotalImmortalSealsEarned` 是只增不减的审计高水位，规范化时至少覆盖“当前未用仙印 + 三条仙途历史累计消耗”。

## 永久仙途

三条仙途各 50 阶：

| 仙途 | 每阶效果 | 50 阶效果 |
| --- | ---: | ---: |
| 战道 | 全部玩家输出伤害 +4% | ×3.00 |
| 悟道 | 在线与离线修炼速度 +6% | ×4.00 |
| 福缘 | 普通装备掉落倍率 +3% | ×2.50 |

战道接入统一玩家输出管线，因此普通攻击、功法、法宝、流派技能和灵宠协战都会读取最终倍率。悟道同时进入在线修炼和离线收益计算。福缘进入普通战斗装备掉率与离线装备数量计算，不影响固定奖励。

### 递增仙印消耗

下一阶消耗：

```text
1 + 4 × 当前阶数
```

因此：

- 0→1 阶：1 枚。
- 1→2 阶：5 枚。
- 2→3 阶：9 枚。
- 单条仙途升到 50 阶累计消耗：4,950 枚。
- 三条全部满阶累计消耗：14,850 枚。

999 次飞升最多产出 11,763 枚仙印，低于三条仙途总容量，因此后期仍然需要选择成长方向，不会在第 29 次飞升后失去仙印用途。

## 修炼可达性

从炼气一层到飞升共 90 次突破。自动化已验证按当前层需求逐层完成 90 次后可以到达终点境界。

当前自然修炼曲线累计约需 47.4 亿修为，基础 2 修为/秒时非常漫长。现有玩法以破境丹作为金丹后的主要快速突破手段：

- 前 20 次突破到达金丹一层。
- 破境丹在金丹一层解锁。
- 再完成 70 次当前层突破即可到达飞升。

本步骤没有擅自修改全局境界需求倍率，以免破坏炼丹、离线收益和旧存档节奏。若未来要求完全不操作炼丹也能较快首次飞升，应作为独立数值平衡步骤统一调整。

## 单写盘飞升事务

飞升使用一次持久化写入和一次不写盘的场景应用：

1. 预检玩家、独立挑战、地图生成器和新轮回状态。
2. 快照飞升状态、境界、修为、生命和灵力。
3. 合并历世地图记录，生成八图第 1 关的新轮回状态。
4. 在内存中重置修炼。
5. 通过 `SaveProgressWithMapOverride` 将玩家、飞升和新地图状态一次写入同一个 SaveGame。
6. 写盘失败时恢复全部玩家快照；地图生成器、怪物和地面掉落从未被修改。
7. 写盘成功后，地图生成器应用已持久化的新轮回，不再进行第二次保存。
8. 清理旧轮回怪物与实体掉落，更新场景/HUD/蓝图事件，并立即补足青云山初始怪物。

普通 `SaveProgress()` 不会接管地图写入，仍保留地图生成器的单写者语义；只有飞升事务通过显式地图覆盖完成跨系统原子提交。

## TBH 界面

- 状态栏按钮：`飞升 [U]`
- 快捷键：`U`
- 面板尺寸：1600×300
- 运行窗口：1707×320
- 显示五项飞升条件、重置/保留说明、历世地图通关数、仙印、三路倍率和下一阶实际消耗。
- 飞升或加点结果显示 5 秒。
- 飞升面板与其他主界面互斥。
- 状态栏已按窗口 DPI 反向补偿，按钮和文字保持实际像素可读。

界面截图确认所有文本、第五项上限条件和三条仙途按钮均未裁切。

## SaveGame v21

新增：

- `bAscensionSystemInitialized`
- `FImmortalAscensionState`
- 飞升次数、未用仙印、累计仙印、三条仙途阶数
- 最后飞升时间和状态修订号
- 八张地图历世最高关卡与累计通关次数

迁移与修复：

- v20 及更早存档创建默认飞升状态，不追溯补发仙印。
- 真实 v20→v21 迁移日志为 `loaded version 20 -> current version 21`。
- 第二次 v21 启动为 `migration=false`。
- v21 外层/内层初始化标记不一致时，保留非零次数、仙印、仙途和历世地图记录，仅修复标记。
- 负数、越界阶数、重复地图记录、未知地图和审计高水位都会规范化。

## 验证

### 构建

- `ImmortalPathEditor Win64 Development -DisableUnity -NoUBA`：通过。
- `ImmortalPath Win64 Development -DisableUnity -NoUBA`：通过。
- UHT：通过。

### 自动化

- `ImmortalPath.Ascension`：6/6 通过。
- 全项目 `ImmortalPath.*`：33/33 通过，0 失败、0 Automation Error。
- 覆盖状态规范化、v21 Schema、五项条件矩阵、重复奖励与 999 次上限、递增成本、三路倍率、历世地图合并、本轮八图重置和 90 次突破可达性。

### 实机

- 飞升成功：次数 1、仙印 +3、境界炼气一层、修为 0。
- 悟道 1 阶：消耗 1 枚，在线/离线修炼倍率 ×1.06。
- 八图均重置到第 1 关；仙宫当前轮回未通关。
- 历世地图 8/8；仙宫历世记录 999/1。
- 飞升提交后青云山立即生成 3 只怪物。
- 重启后仍在青云山第 1 关，历世记录与悟道倍率保留，并继续自动刷怪。
- 强制飞升写盘失败：飞升次数、仙印、境界、地图和场上 3 只怪物全部保持。
- 强制仙途写盘失败：阶数与仙印保持，实际消耗回滚为 0。
- v20→v21、第二次 v21 启动和 v21 标记损坏修复均通过。

回滚日志中的 `Error` 是测试参数故意注入的写盘失败，不是崩溃、Ensure 或运行异常。

截图：

- `Saved/Screenshots/Step33_Ascension_UI.png`
- `Saved/Screenshots/Step33_Ascension_StatusButton.png`

主要日志：

- `Saved/Logs/Step33_Automation_All.log`
- `Saved/Logs/Step33_Ascension_FinalSuccess.log`
- `Saved/Logs/Step33_Ascension_FinalRestart.log`
- `Saved/Logs/Step33_Ascension_FinalRollback.log`
- `Saved/Logs/Step33_Ascension_FinalPathRollback.log`
- `Saved/Logs/Step33_Ascension_V20Write.log`
- `Saved/Logs/Step33_Ascension_V20Migration.log`
- `Saved/Logs/Step33_Ascension_V21Restart.log`
- `Saved/Logs/Step33_Ascension_FinalMarkerWrite.log`
- `Saved/Logs/Step33_Ascension_FinalMarkerRepair.log`

## 主要代码

- `Source/ImmortalPath/Ascension/ImmortalAscensionTypes.h/.cpp`
- `Source/ImmortalPath/Ascension/ImmortalAscensionTests.cpp`
- `Source/ImmortalPath/Characters/ImmortalPlayerCharacter.h/.cpp`
- `Source/ImmortalPath/Spawning/ImmortalMonsterSpawner.h/.cpp`
- `Source/ImmortalPath/Progression/ImmortalCultivationComponent.h/.cpp`
- `Source/ImmortalPath/UI/ImmortalAscensionWidget.h/.cpp`
- `Source/ImmortalPath/UI/ImmortalPlayerStatusWidget.h/.cpp`
- `Source/ImmortalPath/Save/ImmortalPathSaveGame.h/.cpp`

## 素材状态

本步骤不需要新增素材。现有玩家、Fox、Dog、血条、装备光球和原生 UMG 足以完成全部功能。

需要提升视觉表现时，再按独立素材清单制作八张地图分层背景、地图普通怪/Boss、玩家施法/突破/飞升动画和通用特效，不阻塞第 33 步功能。

## 下一步

第 33 步完成后进入第四阶段最后一项：Steam 版本优化。

## 2026-08-06 界面迁移补充

最新界面规则已在第 36 步替代本文件中旧的“状态栏飞升按钮/战斗角色播放动画”表现：

- 历练 HUD 不再显示飞升或其他功能按钮，只保留血条。
- 飞升入口位于统一养成界面的修炼页，`U` 快捷键继续保留。
- 飞升页仍是独立页面，不属于统一功能 `WidgetSwitcher`。
- 飞升成功后由 `UImmortalAscensionWidget` 在页面内播放 17 帧动画。
- 已删除战斗角色上的 Flipbook 替换、缩放、`StopAutoAttack` 和动画后恢复逻辑。
- 飞升动画不会暂停地图、怪物、自动攻击或独立修炼。
- 新截图：`Saved/Screenshots/AscensionInterface_TBH.png`。
- 新运行日志：`Saved/Logs/Step36_AscensionUI_Runtime.log`。
- 全项目自动化更新为 35/35 通过，报告位于 `Saved/TestReports/Step36/index.json`。
- 飞升写入边界回滚证据另见 `Saved/Logs/Step33_Ascension_WriteBoundaryRollback.log` 和 `Saved/Logs/Step33_Ascension_PathWriteBoundaryRollback.log`；这两项是确定性的最终写入边界故障注入，不等同于物理磁盘写满或操作系统权限故障测试。
