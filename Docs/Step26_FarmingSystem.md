# 第 26 步：灵田种植系统

## 一、完成范围

本步把洞府中的灵田从被动灵草产出扩展为完整的可操作循环：

`选择作物 → 消耗灵石/材料播种 → 在线或离线成长 → 成熟 → 单块/批量收获 → 材料进入背包并用于炼丹、出售或炼器`

已实现：

- 灵草、仙果、灵木 3 种稳定作物；
- 6 块持久化田地和灵田等级解锁规则；
- 单块播种、批量播种、单块收获与一键收获；
- 独立 UTC 高水位、离线成长、整数余数和时钟倒退保护；
- 播种时冻结产量，成熟后不会自动领取；
- 材料堆接近上限时部分收获，剩余产量继续留在成熟田块；
- 播种、收获及批量操作的 SaveGame 写盘失败回滚；
- 仙果接入悟道丹，灵木接入流云法衣，二者均可出售；
- SaveGame v14 与 v1–v13 无追溯迁移；
- 1600×300 TBH 原生横条界面，以及旧 900×600 模态的 DPI 自适应；
- 自动化、Editor/Game 构建、真实旧档迁移、成熟/收获/重启/失败回滚实测。

洞府原有的被动灵草生产继续保留，界面中明确称为“自然培育”；它与本步主动播种互不覆盖。

## 二、作物目录

| 作物 | 稳定 ID | 产物 | 解锁 | 基础成熟时间 | 基础收成 | 播种成本 |
| --- | --- | --- | --- | --- | --- | --- |
| 灵草 | `SpiritGrassCrop` | `SpiritGrass` | 灵田 Lv1 | 300 秒 | 3–4 | 3 灵石 |
| 仙果 | `ImmortalFruitCrop` | `ImmortalFruit` | 灵田 Lv5 | 900 秒 | 2–3 | 8 灵石、灵草 ×1 |
| 灵木 | `SpiritWoodCrop` | `SpiritWood` | 灵田 Lv9 | 1800 秒 | 2–3 | 12 灵石、灵草 ×1、矿石 ×1 |

运行时会优先读取可选数据表：

```text
/Game/GAME/Data/DT_FarmingCrops
```

资产不存在时静默使用 C++ 内置目录；稳定作物 ID 不随显示名称变化。

## 三、田块、等级与成长公式

灵田状态始终保存 6 个田块记录，当前等级决定可用数量：

| 灵田等级 | 可用田块 |
| --- | --- |
| 1–4 | 2 |
| 5–8 | 3 |
| 9–12 | 4 |
| 13–16 | 5 |
| 17–20 | 6 |

成长倍率：

```text
GrowthMultiplier = 1 + 0.03 × (灵田等级 - 1)
```

每提升 4 级，播种时的最终产量增加 1：

```text
YieldBonus = floor((灵田等级 - 1) / 4)
FinalYield = Random(BaseMinimum, BaseMaximum) + YieldBonus
```

最终产量在播种时写入田块，不会因重启、界面刷新或收获时重新随机。洞府升级灵田前会先以旧等级结算过去区间，升级后的速度只影响升级之后的时间。

## 四、UTC 离线成长与防修改时间

`FImmortalFarmingState` 使用独立 `LastSettlementUtcTicks`，不与洞府生产、离线挂机或自动保存共用时间戳。

每个田块保存：

```text
作物 ID
冻结总成长 Tick
剩余成长 Tick
整数 permille 余数
待收获产量
播种时灵田等级
```

结算规则：

- 空田也推进高水位，之后播种不会吃到历史空置时间；
- 相同 UTC 重复结算为零修改；
- 分段结算与一次结算使用整数余数，结果一致；
- 系统时间早于高水位时冻结成长，不降低高水位，也暂时禁止播种；
- 时间恢复并超过旧高水位后，只结算新增区间；
- 作物成熟后停止倒计时，但不会自动进入背包；
- UI 每秒在状态副本上调用纯 `GetPlotView`，不会因查看倒计时写盘或改变存档。

## 五、播种事务

播种先结算同一 UTC 下的洞府与灵田，再对以下数据创建快照：

- 洞府状态；
- 灵田状态；
- 材料背包；
- 灵石；
- 材料修订号。

材料成本会先按稳定 ID 聚合，随后在副本上完成完整检查和扣除。只有作物、田块、等级、灵石和全部材料均有效时才提交。

批量播种会遍历所有已解锁空田；资源只能支持部分田块时，明确返回已播种数量，不会对同一田块重复扣费。若最终 SaveGame 写盘失败，单块或整个批次都会恢复到操作前，成功计数与子结果同步清零，界面不会误报成功。

## 六、收获、部分入包与防重复

收获使用播种时冻结的产量。材料库存能够容纳全部产量时，田块清空并累计一次完整收获；只能容纳一部分时：

- 实际可容纳数量进入材料背包；
- 田块保持成熟；
- 未入包数量继续保存在 `PendingYield`；
- 玩家腾出空间后可以再次收取剩余部分。

材料堆完全已满时田块零修改。完整收获后重复点击或重启再收获不会获得第二份产物。

收获先完成运行时副本事务，再写盘，最后才发布材料变化事件。强制写盘失败实测会同时恢复成熟田块、材料、统计值和修订号；返回结果中的收获数量、完整/部分成功数也会归零。

## 七、产物的真实用途

本步没有生成只能查看的占位材料：

- 灵草继续用于炼丹、洞府升级和其他既有配方；
- 仙果加入悟道丹材料：`ImmortalFruit ×1`；
- 灵木加入流云法衣打造材料：`SpiritWood ×2`；
- 仙果与灵木在百宝阁拥有正出售价格；
- 两者的怪物掉落权重为 0，只从灵田等明确来源获得。

因此三种作物分别覆盖炼丹、出售与炼器用途。

## 八、界面、洞府入口与 TBH DPI

灵田不新增 HUD 按钮或快捷键，入口位于洞府灵田建筑详情中的“进入灵田 · 播种与收获”。洞府界面与灵田界面保持互斥，关闭后恢复游戏输入，后台自动战斗仍会继续。

灵田界面使用 1600×300 原生 TBH 布局：

- 左侧显示 3 种作物、成本、成熟时间、产量、选择状态和批量播种；
- 右侧 6 块田单行排列，显示锁定、幼苗、生长、将成熟、成熟五种状态；
- 每块田拥有进度、剩余时间、冻结产量和上下文操作按钮；
- 底部提供一键收获与灵田等级效果提示。

第一次实机截图发现 UE 在 1707×320 窗口应用约 0.444 的 UMG DPI，1600×300 逻辑面板实际只显示约 710×134。最终实现使用 `GetViewportScale` 补偿 DPI，并以物理像素计算居中与适配：

```text
FitScale = min(可用宽度 / 逻辑宽度, 可用高度 / 逻辑高度)
RenderScale = FitScale / UMG_DPI_Scale
```

修复后实测面板边界约为 `x=53..1653, y=10..310`，完整占用 1707×320 横条且没有裁切。洞府等既有 900×600 模态也会等比缩放，洞府中的灵田入口在 TBH 下可见可点击。摄像机本身没有修改，仍保持 Taskbar Hero 固定横条视角。

## 九、SaveGame v14 与迁移

SaveGame 新增：

```text
FImmortalFarmingState FarmingState
```

并升级到 v14。迁移同时检查：

```text
LoadedSaveVersion < 14 || !FarmingState.bInitialized
```

v1–v13 旧档从当前 UTC 创建 6 块空田，不追溯生成作物、成长时间或收获物。该初始化状态会立即随完整玩家数据写入 v14。

`SaveProgress()` 在写入前统一结算洞府与灵田；加载或写盘失败时恢复两套结算前状态，因此不会丢失未保存的时间区间，也不会把失败写盘误当成已经推进的高水位。

## 十、测试与运行验证

新增自动化测试：

```text
ImmortalPath.Farming.CatalogStateAndPlantTransactions
ImmortalPath.Farming.GrowthHarvestAndBatchSafety
```

覆盖：

- 3 种作物、材料用途、成本、时长和产量；
- 6 田块与 `1/5/9/13/17` 解锁门槛；
- 默认、旧版、未知、重复、负数和越界状态规范化；
- 单块/批量播种、资源不足、锁定、占用和时间回退；
- 播种成本聚合、完整扣费与失败零修改；
- 整数分段结算、成熟精确边界、空田高水位和纯 UI 预览；
- 部分收获、二次收完、防重复与批量收获；
- SaveGame v14 断言。

最终验证结果：

- `ImmortalPath.Farming.*` 2/2 `Success`；
- 全项目 `ImmortalPath.*` 12/12 `Success`；
- `ImmortalPathEditor` Win64 Development `-DisableUnity` 构建成功；
- `ImmortalPath` Win64 Development `-DisableUnity` 构建成功；
- 真实 v13 存档迁移为 v14，首次创建空田，没有追溯产物；
- 灵田 Lv9 实测解锁 4 块田，批量播种冻结产量为 `5/6/5/6`；
- 注入 301 秒后 4 块田全部成熟，批量收获准确获得 22 份灵草；
- 再次重启后田块为空、累计收获仍为 22，没有重复领取；
- 强制收获存档失败时返回 `items=0`，4 块成熟作物保留；再次重启仍成熟，累计收获仍为 22；
- 1707×320 中灵田面板和洞府入口均完整可见；
- 运行日志无 `Fatal`、`Ensure`、`Accessed None` 或未处理异常；
- 用户原存档最终恢复为 SHA-256 `E3833E79F60F31F93EFCAD69640DE8FC3010971BF23914AE9F29DFD6CF01B86A`。

验证日志：

- `Saved/Logs/Step26FarmingCoreTestsFinal.log`
- `Saved/Logs/Step26FinalTests.log`
- `Saved/Logs/Step26Runtime1Plant.log`
- `Saved/Logs/Step26Runtime3Mature.log`
- `Saved/Logs/Step26Runtime4Harvest.log`
- `Saved/Logs/Step26Runtime5Restart.log`
- `Saved/Logs/Step26Runtime6HarvestRollback.log`
- `Saved/Logs/Step26Runtime7RollbackRestart.log`
- `Saved/Logs/Step26Runtime8CaveEntry.log`

验证截图：

- `Saved/Screenshots/Step26_Farming_DpiFixed.png`
- `Saved/Screenshots/Step26_Farming_Mature.png`
- `Saved/Screenshots/Step26_Farming_Harvested.png`
- `Saved/Screenshots/Step26_Farming_HarvestRollback.png`
- `Saved/Screenshots/Step26_Cave_FarmingEntry.png`

## 十一、开发验证参数

以下参数仅在非 Shipping 构建生效：

| 参数 | 用途 |
| --- | --- |
| `-ImmortalTestFarmingFieldLevel=N` | 临时设置灵田测试等级 |
| `-ImmortalTestGrantFarmingResources` | 发放测试灵石、灵草和矿石 |
| `-ImmortalTestFarmingCrop=CropId` | 指定测试作物 |
| `-ImmortalTestFarmingPlot=N` | 指定测试田块索引 |
| `-ImmortalTestPlantFarming` | 在指定田块播种 |
| `-ImmortalTestPlantAllFarming` | 在全部可用空田播种 |
| `-ImmortalTestFarmingSeconds=N` | 注入 N 秒成长时间 |
| `-ImmortalTestHarvestFarming` | 收获指定田块 |
| `-ImmortalTestHarvestAllFarming` | 一键收获全部成熟田块 |
| `-ImmortalTestForceFarmingSaveFailure=Plant\|Harvest` | 强制播种或收获写盘失败，验证回滚 |
| `-ImmortalTestLogFarming` | 输出田块与持久化审计 |
| `-ImmortalTestOpenFarming` | 自动打开灵田界面 |
| `-ImmortalTestScreenshotFarming` | 保存 `Saved/Screenshots/FarmingTest.png` |
| `-ImmortalTestExitAfterScreenshot` | 截图后干净退出 |

## 十二、素材需求与下一步

本步不依赖新素材，现有文字、色块和材料占位字符已经能够完整操作。可选美术增强：

- 灵田横条背景：1600×300；
- 灵草、仙果、灵木图标：128×128，透明背景；
- 幼苗、生长、成熟三阶段小图；
- 播种、成熟和收获短特效。

下一步进入第 27 步“宗门系统”：加入青云宗、天剑宗、万妖谷、魔宗，宗门选择、任务、贡献值、宗门商店、功法奖励以及存档。
