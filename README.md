# Immortal Path: Idle Loot

UE 5.7 + C++ 开发的 2D 横版挂机刷怪 ARPG。

## 当前开发进度

### 第 1 步：玩家自动攻击核心（已完成）

现有 `Content/GAME/Player/Player` 蓝图已改为继承 `ImmortalPlayerCharacter`，并已在 UE 5.7 编辑器中编译、保存和确认战斗参数可见。

C++ 已实现：

- 周期性搜索攻击范围内的目标
- 优先选择距离玩家最近的目标
- 攻击前摇结束后通过 UE `ApplyDamage` 统一结算伤害
- 目标离开范围或失效时不造成伤害
- 可在蓝图中配置伤害、攻击范围、攻击间隔和前摇
- 为攻击动画提供 `On Auto Attack Started` 蓝图事件
- 为命中反馈/恢复待机提供 `On Auto Attack Resolved` 蓝图事件
- 临时蓝图目标只需带 `Monster` Actor Tag 即可被攻击

主要代码：

- `Source/ImmortalPath/Characters/ImmortalPlayerCharacter.h/.cpp`
- `Source/ImmortalPath/Combat/AutoAttackTarget.h`

### 将现有 Player 蓝图接入 C++

接入已经完成。如需复查，打开 `Content/GAME/Player/Player`，顶部应显示父类 `ImmortalPlayerCharacter`；在 Class Defaults 的 `Immortal Path | Combat` 分类下可以调整参数。

默认参数：

- Attack Damage：10
- Attack Range：220 cm
- Attack Interval：1 秒
- Attack Windup：0.25 秒

### 临时测试方法

在下一步的正式怪物类完成前，可以给一个带碰撞的 Pawn/Actor 添加 `Monster` Actor Tag，碰撞对象类型使用 Pawn 或 WorldDynamic，并在它的 `AnyDamage` 事件后接 `Print String`。把它放到玩家 220 cm 内即可验证自动攻击。

### 第 2 步：怪物基础与刷怪器（已完成）

C++ 已实现：

- 怪物最大生命值、当前生命值和生命百分比
- 接收 UE `ApplyDamage` 伤害并在零血时死亡
- 死亡后关闭移动与碰撞并延迟销毁
- 玩家自动攻击使用正式 `AutoAttackTarget` 接口锁定存活怪物
- 受伤、死亡事件留给蓝图播放动画和反馈
- 刷怪区域、初始数量、最大存活数和补怪间隔
- 防止怪物直接出生在玩家身上
- 自动匹配玩家的 2D 深度与地面高度

已创建并接入：

- `/Game/GAME/BP_Monster`
- `/Game/GAME/BP_MonsterSpawner`
- `BP_MonsterSpawner` 已放置到 `NewMap`
- 运行时验证从初始 3 只补到最大 5 只，World Outliner 显示 5 个 `BP_Monster_C`

### 第 3 步：怪物自动战斗与动画（已完成）

- Fox 和 Dog 均已接入移动、攻击、受击、死亡 Flipbook
- 怪物自动寻找玩家、沿横版路线接近并在攻击范围内停下
- 攻击前摇结束后对玩家结算伤害
- 受击会打断当前动作，零血后播放死亡动画并延迟销毁
- 玩家已有生命值、受伤与死亡状态
- 刷怪器会随机生成 `BP_Monster`（Fox）或 `BP_DogMonster`（Dog）

### 第 4 步：挂机战斗镜头（已完成）

- 玩家保持原地挂机，不主动追怪
- 运行时自动调整现有 SpringArm/Camera，让玩家位于画面中间偏左
- 透视镜头视野扩大到 115 度，为右侧来怪预留空间
- 正交镜头使用 1600 cm 宽度作为兼容配置
- Camera Lead Distance、FOV 与 Ortho Width 均可在 Player 蓝图的 `Immortal Path | Camera` 中调整
- 已在 PIE 中验证玩家、Fox/Dog 与地图同时可见，怪物会移动到攻击范围后交战

## 下一步

将战斗数值数据化：先整理玩家生命、攻击、攻击速度、攻击范围，再整理 Fox/Dog 的生命、攻击、移动速度与奖励，为后续血条、掉落和关卡成长做准备。
