# 第 24 步：多地图历练系统

## 一、完成范围

本步把原本仅有青云山的一套关卡进度扩展为 8 张可切换的逻辑地图，同时保持 Taskbar Hero（TBH）式固定横条窗口、自动战斗和同一战斗场景。

已完成：

- 8 张地图与稳定地图 ID；
- 每张地图独立 1–999 关；
- 每 10 关一个守关 Boss，第 999 关为最终 Boss；
- 按修炼大境界解锁地图；
- 各地图独立保存关卡、当前击杀数和通关状态；
- 地图差异化怪物强度、怪物名称、Boss 名称、装备等级/品质、灵石倍率和材料池；
- 当前地图决定在线实体掉落与离线挂机掉落；
- `M` 键、HUD“地图 [M]”按钮和 900×600 两栏地图界面；
- SaveGame v12 与 v1–v11 兼容迁移；
- 安全切图事务、失败回滚、旧地图怪物/实体掉落清理；
- 逻辑地图色调与动态地图关卡 HUD。

## 二、地图目录与解锁

| 顺序 | 地图 ID | 显示名 | 解锁境界 | 普通妖兽 | Boss | 场景定位 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | `QingyunMountain` | 青云山 | 炼气 | 青云妖兽 | 青云妖王 | 入门地图 |
| 2 | `DemonWolfValley` | 妖狼谷 | 炼气 | 噬月妖狼 | 啸月狼王 | 炼气进阶 |
| 3 | `MyriadBeastForest` | 万妖林 | 筑基 | 万妖林灵兽 | 万妖树皇 | 妖丹/妖骨方向 |
| 4 | `BlackWindCave` | 黑风洞 | 筑基 | 黑风魔兽 | 黑风魔君 | 矿石/灵铁方向 |
| 5 | `AncientRuins` | 上古遗迹 | 金丹 | 遗迹傀儡 | 遗迹守将 | 灵铁/法宝残片方向 |
| 6 | `NetherValley` | 幽冥谷 | 金丹 | 幽冥妖灵 | 幽冥鬼王 | 灵液/妖丹方向 |
| 7 | `NineNetherSecretRealm` | 九幽秘境 | 元婴 | 九幽魔物 | 九幽冥皇 | 高阶装备与法宝材料 |
| 8 | `ImmortalPalaceRuins` | 仙宫遗址 | 元婴 | 仙宫守卫 | 仙宫天将 | 当前最高阶历练地图 |

所有地图本地关卡范围均为 1–999。高阶地图的第 1 关不会等同于青云山第 1 关：地图定义会额外叠加生命、攻击、防御、装备等级、最低品质和掉落倍率。

## 三、关卡与独立进度

每张地图分别保存：

```text
MapId
Stage
StageKills
bCompleted
```

普通关击杀 10 只妖兽后进入下一关；每 10 关进入一只 Boss 的守关关卡；第 999 关固定为最终 Boss。Boss 关不保存伪造的部分击杀数，普通关持久化范围为 `0..9`，通关地图保存最终 Boss 的一次击杀。

示例实测：

```text
青云山：第 22 关
妖狼谷：第 7 关
黑风洞：第 1 关
```

从妖狼谷返回青云山后恢复第 22 关，再次进入妖狼谷仍恢复第 7 关，三张地图不会互相覆盖。

## 四、安全切图流程

项目目前只有一张真实战斗关卡 `/Game/GAME/Maps/NewMap`。多地图采用同关卡内切换逻辑数据，不调用 `OpenLevel`，避免重建玩家、摄像机、HUD 和自动战斗。

切图流程：

1. 校验地图 ID 与玩家大境界；
2. 保存当前地图独立进度；
3. 暂停刷怪并进入切图保护状态；
4. 在内存切换目标地图进度并尝试提交 SaveGame；
5. 目标存档成功后才清理旧怪物与装备、灵石、材料三类实体掉落；
6. 应用目标地图色调、HUD、怪物/Boss 配置并恢复刷怪；
7. 任一存档步骤失败时恢复原地图状态，原地图未拾取掉落也不会因失败事务丢失。

旧地图怪物的延迟死亡回调会按 `MapId` 拒绝，不会把击杀错误结算到新地图。

## 五、地图差异化战斗与掉落

`AImmortalMonsterCharacter` 新增地图配置入口：

```cpp
ConfigureForMapStage(MapId, Stage)
ConfigureAsMapBoss(MapId, Stage)
```

地图定义控制：

- 生命、攻击、防御倍率；
- 普通怪和 Boss 显示名、色调；
- 装备物品等级加成；
- 普通怪与 Boss 的最低装备品质；
- 装备、材料掉率加成；
- 灵石实体数量倍率；
- 四种地图专属材料候选池。

Boss 继续保底生成装备、灵石和材料实体；普通怪按各地图概率生成。拾取仍沿用现有自动追踪、五秒提示、背包写入和自动换装流程。

离线挂机也读取退出时的活动地图：装备等级/最低品质、灵石倍率与材料池均来自该地图，不再固定使用青云山掉落表。

## 六、地图界面与 TBH 摄像机

按 `M` 或 HUD“地图 [M]”打开 900×600 两栏界面：

- 左侧列出 8 张地图、当前/锁定/已通关状态和独立关卡；
- 右侧显示境界要求、地图说明、进度、Boss 规则、怪物强度与掉落预览；
- 当前地图不可重复前往；
- 锁定地图仍可查看详情，但“前往历练”按钮禁用；
- 切换成功后界面自动关闭并恢复游戏输入；
- 地图界面与背包、炼丹、炼器、法宝、功法、问道、百宝阁保持互斥。

摄像机没有改成传统横版跟随或打开新关卡。它继续使用此前完成的 TBH 固定横条方案：窗口实测为 1707×320，玩家位于中间偏左区域，角色与来怪同时可见。切换地图只改变 `PaperTileMap` 色调和战斗数据，因此不会造成镜头跳动或重新定位。

## 七、SaveGame v12 与兼容迁移

新增 `FImmortalMapSystemState`，保存活动地图和 8 份独立进度。Spawner 是地图状态的唯一权威写入者，玩家保存其他系统时只保留已经存在的地图字段，避免两个 Actor 用旧快照互相覆盖。

v1–v11 迁移规则：

- 旧 `QingyunStage / QingyunStageKills / bQingyunMountainCompleted` 原样迁入青云山；
- 另外 7 张地图从第 1 关开始；
- 活动地图默认为青云山；
- 保存 v12 时继续镜像旧青云山字段，兼容现有炼器、法宝和功法的青云山解锁条件；
- 缺失、重复、未知或越界地图记录会归一化；
- 迁移加载阶段不立即写盘，避免 Actor `BeginPlay` 顺序提前更新时间戳并吞掉离线收益。

使用真实 v11 用户存档副本实测迁移成功：青云山第 22 关、金丹九层、64,475 灵石、5 件穿戴、30 件背包装备、7 类材料、2 件法宝、1 部功法均正常读取；8 张地图状态补全后重启为 `migrationPending=false`。

## 八、测试、构建与运行验证

新增自动化测试：

```text
ImmortalPath.Maps.CatalogProgressMigrationAndDrops
```

覆盖：

- 8 图稳定 ID、顺序和境界解锁矩阵；
- 1–999 关、每 10 关 Boss 与 999 最终 Boss；
- v11→v12 迁移、脏档修复与幂等归一化；
- 地图独立进度与非法写入零修改；
- 难度、装备等级/品质、灵石倍率递增；
- 8 个不同材料池与 Boss 确定性候选验证。

建议验证命令：

```powershell
& 'E:\UE_5.7\Engine\Build\BatchFiles\Build.bat' ImmortalPathEditor Win64 Development 'D:\UE2D\ImmortalPath\ImmortalPath.uproject' -WaitMutex -DisableUnity
& 'E:\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE2D\ImmortalPath\ImmortalPath.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests ImmortalPath.;Quit' -TestExit='Automation Test Queue Empty' -log
& 'E:\UE_5.7\Engine\Build\BatchFiles\Build.bat' ImmortalPath Win64 Development 'D:\UE2D\ImmortalPath\ImmortalPath.uproject' -WaitMutex -DisableUnity
```

非 Shipping 验证参数：

| 参数 | 作用 |
| --- | --- |
| `-ImmortalTestStage=N` | 临时设置当前地图关卡 |
| `-ImmortalTestMapRealm=N` | 临时设置大境界，用于解锁边界验证 |
| `-ImmortalTestTravelMap=MapId` | 延迟调用真实切图事务 |
| `-ImmortalTestLogMaps` | 输出活动地图与 8 份独立进度 |
| `-ImmortalTestOpenMaps` | 自动打开地图界面 |
| `-ImmortalTestScreenshotMaps` | 截取 `Saved/Screenshots/MapSelectionTest.png` |
| `-ImmortalTestExitAfterScreenshot` | 截图后干净退出 |

最终验证结果：

- 全项目 9/9 项 `ImmortalPath.*` 自动化测试全部为 `Success`；
- `ImmortalPathEditor` 与 `ImmortalPath` Win64 Development `-DisableUnity` 最终构建均为 `Succeeded`；
- 炼气进入黑风洞被拒绝，状态保持青云山；
- 青云山第 22 关与妖狼谷第 7 关来回切换、退出和重启后均独立保持；
- 筑基一层成功进入黑风洞，场景色调、怪物属性和掉落等级切换正确；
- 黑风洞第 1 关怪物实测装备等级为 76，生命/攻击/防御倍率生效；
- 真实 v11 存档副本迁移并重启成功；
- 最终 1707×320 运行日志无 `Error`、`Fatal`、`Ensure` 或 `Accessed None`；
- 用户原存档恢复后的 SHA-256 为 `E3833E79F60F31F93EFCAD69640DE8FC3010971BF23914AE9F29DFD6CF01B86A`，与测试前完全一致。

验证截图：

- `Saved/Screenshots/MapSelectionTest.png`：最终黑风洞解锁/切图与 TBH 视角；
- `Saved/Screenshots/MapSelectionLockedTest.png`：炼气境锁定验证；
- `Saved/Screenshots/MapSelectionIndependentProgressTest.png`：青云山 22 / 妖狼谷 7 独立进度；
- `Saved/Screenshots/MapSelectionMigrationTest.png`：真实 v11 存档迁移后的 8 图界面。

## 九、素材需求

本步不依赖新增素材，现有地图、Fox/Dog 动画、面板纹理与颜色占位已能完整游玩。

后续可选补充，但不会阻塞下一步：

- 8 张地图预览图：建议 512×288，统一仙侠横版风格；
- 妖狼谷至仙宫遗址的独立横版背景或 TileMap 图层；
- 各地图普通怪/Boss 的独立移动、攻击、受击、死亡动画；
- 8 个地图图标：建议 128×128、透明背景。

下一步进入第 25 步“洞府系统”。
