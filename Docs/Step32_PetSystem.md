# Step 32：灵宠系统

## 完成范围

本步骤在现有 Taskbar Hero（1707×320）战斗窗口中加入了完整的首版灵宠闭环：

- 两只可成长灵宠：青丘灵狐、镇岳灵犬。
- 灵宠驯服、出战切换、等级、经验、五星培养。
- 独立跟随、寻怪、近战/远程自动攻击。
- 复用 Fox/Dog 的 move、attack、hurt、death 动画。
- 灵宠造成的伤害统一通过玩家伤害管线结算。
- v20 存档、v19 迁移、损坏数据修复和事务失败回滚。
- 原生 1600×300 TBH 灵宠界面、状态栏按钮和 `P` 快捷键。
- 操作提示和升级提示持续 5 秒。

击杀怪物仍然不会奖励玩家修为。灵宠获得的是独立的“灵兽历练”，与在线/离线修炼系统完全分开。

## 初始灵宠

| ID | 名称 | 初始状态 | 定位 | 基础攻击系数 | 攻击间隔 | 范围 |
|---|---|---:|---|---:|---:|---:|
| `SpiritFox` | 青丘灵狐 | 默认拥有并出战 | 远程、高频、高暴击 | 18% | 1.15 秒 | 560 |
| `SpiritHound` | 镇岳灵犬 | 需要驯服 | 近战、慢速、重击 | 26% | 1.45 秒 | 205 |

镇岳灵犬驯服消耗：

- 灵石 400
- 妖丹 12

配置优先读取可选数据表 `/Game/GAME/Data/DT_Pets`；数据表不存在或行无效时使用原生 C++ 目录，因此当前工程不依赖额外编辑器配置即可运行。

## 培养规则

### 等级

- 等级范围：1–50。
- 升级经验：`100 + (等级-1)×42 + (等级-1)²×5`。
- 普通击杀经验：`8 + 难度索引/8`。
- 精英经验 ×2，Boss 经验 ×5。
- 世界 Boss 在 Boss 倍率后再 ×3，并保证至少 180 点。

灵宠升级不会消耗或生成玩家修为。

### 升星

- 星级范围：0–5 星。
- 升至下一星消耗：`250×目标星级` 灵石和 `3×目标星级` 妖丹。
- 升级、升星都会提高协战伤害系数。
- 驯服、切换出战、升星均为“内存状态 + 资源 + 写盘”原子事务；写盘失败时完整回滚。

### 伤害

灵宠每击伤害系数：

```text
基础系数
+ (等级-1) × 每级系数
+ 星级 × 每星系数
```

真正伤害通过：

```text
Pet Actor
→ Player::ResolvePetAttack
→ Player::ApplyOutgoingDamage
→ Monster::TakeDamage / Die
→ Spawner 单次权威死亡结算
```

`DamageCauser` 始终为玩家，而不是灵宠 Actor。这样可以复用现有掉落、吸血、Boss 伤害和死亡结算，同时避免宠物再发放一次奖励。

只有 `DamageCauser` 为玩家的权威击杀才会增加灵宠经验；世界 Boss/无尽秘境召唤物、陈旧怪物、环境伤害和外部伤害不会增加灵宠成长。

## 运行时 Actor

`AImmortalPetCharacter` 是独立 `APaperCharacter`：

- 只有 `Pet` 标签，不具有 `Monster` 标签。
- 不实现受攻击目标，不会被玩家或其他灵宠选中。
- 没有血条、掉落表或死亡奖励。
- 以玩家前侧偏移跟随，超出缰绳距离时安全传送回来。
- 在搜索范围内自行选择最近的有效敌人。
- 玩家死亡时播放宠物 death；玩家自动复活后恢复 move 与协战。
- hurt/death 动画保留开发预览入口，未来加入宠物生命玩法时无需重做动画接线。

Fox/Dog 的动画按 TBH 窗口缩放，并将 Sprite 放在 TileMap 前方，保证角色、灵宠和怪物同时位于镜头内。没有修改现有 TBH 摄像机参数。

## UI

灵宠界面：

- 状态栏按钮：`灵宠[P]`
- 快捷键：`P`
- 尺寸：1600×300，适配 1707×320 窗口
- 四栏布局：名册、身份说明、成长数值、培养操作
- 显示拥有状态、出战状态、等级经验、五星、预计 DPS、驯服/升星资源
- 驯服、出战、升星结果在面板内显示 5 秒
- 面板关闭时仍可通过战斗 HUD 看到 5 秒结果横幅

灵宠面板与背包、炼丹、炼器、法宝、功法、角色构筑、百宝阁、地图、洞府、种植、宗门、世界 Boss、无尽秘境互斥，避免输入模式和窗口层叠冲突。

## 存档

SaveGame 版本由 v19 升至 v20，新增：

- `bPetSystemInitialized`
- `FImmortalPetState PetState`
- 当前出战 ID
- 每只灵宠拥有状态、等级、当前级经验、星级、协战击杀审计
- 全局灵宠击杀审计和修订号

迁移行为：

- v19 及更早存档：创建默认灵狐状态，不追溯生成宠物经验。
- v20 外层/内层初始化标记不一致：保留原成长和出战选择，仅修复标记。
- 重复条目、越界等级/经验/星级、负击杀数和未知活动 ID 会被规范化。
- 未知宠物的非活动进度会保留，便于未来目录扩展。
- 所有写盘入口在最终 `SaveToDisk` 边界统一应用版本/损坏标记测试夹具，避免玩家与地图退出写盘相互覆盖。

## 验证

### 构建

- `ImmortalPathEditor Win64 Development -DisableUnity -NoUBA`：通过。
- `ImmortalPath Win64 Development -DisableUnity -NoUBA`：通过。
- UHT：通过。

### 自动化

- `ImmortalPath.Pets`：4/4 通过。
- `ImmortalPath.` 全量：27/27 通过。
- 覆盖目录、四套动画同步加载、默认状态、解锁/选择、损坏修复、幂等规范化、未知 ID 前向兼容、大经验/溢出保护、升级升星、战斗缩放和 v20 Schema。

### 实机

- Fox/Dog 均在 1707×320 镜头内并产生真实攻击与命中。
- 宠物致死日志确认 `damageCauser=Player`、`duplicateRewards=0`。
- 实机击杀确认地图只推进一次，灵宠经验只增加一次，玩家修为增量为 0。
- Fox hurt/death 动画加载并播放；玩家死亡后进入宠物死亡状态，玩家复活后宠物恢复。
- 驯服、切换、升星三类强制写盘失败均完整回滚。
- v19→v20 只迁移一次；第二次 v20 启动 `migration=false`。
- v20 标记损坏修复后保留镇岳灵犬及成长。
- 镇岳灵犬 1 星、出战状态在重启后保留。

截图：

- `Saved/Screenshots/Step32_Pet_UI.png`
- `Saved/Screenshots/Step32_Pet_Battle_Fox.png`
- `Saved/Screenshots/Step32_Pet_Battle_Dog.png`
- `Saved/Screenshots/Step32_Pet_Notification.png`

主要日志：

- `Saved/Logs/Step32_Automation_Pets_Final.log`
- `Saved/Logs/Step32_Automation_All.log`
- `Saved/Logs/Step32_Runtime_PetUI_Final.log`
- `Saved/Logs/Step32_Runtime_FoxBattle_Final2.log`
- `Saved/Logs/Step32_Runtime_DogBattle_Final.log`
- `Saved/Logs/Step32_Runtime_PetNotification.log`
- `Saved/Logs/Step32_Runtime_PetLethalAttribution.log`
- `Saved/Logs/Step32_Runtime_PetAnimationsOwnerDeath.log`
- `Saved/Logs/Step32_Runtime_Migration_v19_to_v20.log`
- `Saved/Logs/Step32_Runtime_Migration_v20_Restart.log`
- `Saved/Logs/Step32_Runtime_MarkerMismatch_Repair.log`
- `Saved/Logs/Step32_Runtime_Rollback_Unlock.log`
- `Saved/Logs/Step32_Runtime_Rollback_Equip.log`
- `Saved/Logs/Step32_Runtime_Rollback_Star.log`
- `Saved/Logs/Step32_Runtime_PetRestart_Star1.log`

## 素材状态

当前步骤不需要再制作素材，已经复用现有 Fox/Dog 四套动画。未来如果要把名册中的文字图标替换为独立宠物头像，可以再提供 128×128 透明 PNG；这不是当前功能的阻塞项。
