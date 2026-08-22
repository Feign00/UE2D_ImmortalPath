# 第 37 步：任务系统与统一养成界面接入

更新时间：2026-08-06

## 目标与当前结论

本步骤在第 36 步统一养成容器中加入第 18 个功能页“任务”，完成主线任务、每日任务和成就的事件驱动进度、奖励领取、每日刷新及存档迁移。

任务页只切换 `UWidgetSwitcher` 的当前内容，不会替换地图、暂停世界或停止自动战斗与独立修炼。战斗击杀仍然不直接提供修为；任务系统只观察真实玩法事件并记录任务进度。

功能、专项测试、历史全量回归和 1707×320 实机验证均已通过。本步骤只确认程序功能完成，任务页正式背景与图标仍属于第 38 步人界美术总门禁，不能据此宣称人界美术完成。

## 任务内容

当前目录包含：

- 主线任务：7 项，按前置任务依次解锁。
- 每日任务：5 项，每个游戏日独立记录和领取。
- 成就：7 项，使用永久累计计数，不随每日刷新清空。

可记录的真实事件指标包括：

- 击败妖物、通过关卡、击败首领和通关地图。
- 拾取装备与拾取灵石。
- 修炼突破、炼丹、炼器和领取宗门任务。
- 完成飞升。

任务奖励可以包含灵石、功法悟性和材料。领取过程先校验任务状态与奖励容量，再提交任务领取和奖励；写盘失败时回滚任务状态及奖励，避免重复领取或只发出一半奖励。

## 每日刷新与时间保护

- 每日任务使用 UTC 时间加项目时区偏移生成日期键。
- 跨日时只重置每日计数和每日领取记录。
- 主线、成就和永久累计计数不会被每日刷新清空。
- 检测到系统时钟回拨时不会把日期键倒退，也不会借此重复领取。
- 旧存档加载后会正规化未知 ID、重复领取项、负数计数和失效状态。

## 存档

任务系统将存档版本推进到 `SaveSchemaV22`，保存：

- 永久和每日事件计数。
- 已领取的主线、每日与成就 ID。
- 每日日期键、最后观察时间、总领取次数和修订号。
- 任务系统初始化标记。

版本 22 之前的存档会创建默认任务状态并执行迁移。加载、每日刷新、记录进度和领取奖励均使用持久化失败回滚，现有装备、材料、灵石、宠物和其他玩法数据不会因任务迁移被清空。

## 统一界面接入

- `EImmortalManagementFeature::Quest` 是统一养成界面的第 18 项功能。
- Quest 页面注册到同一个 `UWidgetSwitcher`，不作为独立 Viewport 弹窗叠在历练画面上。
- 页面包含“主线 / 每日 / 成就”分类、任务进度、奖励说明、领取状态和领取按钮。
- 人界任务背景软路径为：

```text
/Game/GAME/Asset/ui/management/backgrounds/mortal/T_BG_Mortal_Quest
```

- 正式任务背景和任务图标尚未制作时使用统一容器的回退色和文字按钮，不会出现空白页，也不会影响玩法逻辑。

## 后台不中断实机证据

实机命令以 `/Game/GAME/Maps/NewMap` 启动，窗口强制为 TBH `1707×320`，随后在同一养成容器内依次打开修炼、宗门和任务页。

运行日志确认：

- 打开任务页时 `worldPaused=false`。
- 自动攻击计时器 `autoAttackActive=true`。
- 顶层仍是统一养成容器，任务页不是新的独立游戏世界。
- 任务页打开期间击杀计数继续从 1 增长到 4。
- 独立修炼值继续从 9447 增长到 9452。
- 关闭养成界面后自动战斗仍保持启用。

证据：

- 运行日志：`Saved/Logs/Step37_Management_Runtime.log`
- 任务页截图：`Saved/Screenshots/Management_Quest_TBH.png`
- 截图记录尺寸：`1707×320`

## 构建与自动化

Quest 增量构建：

- 首次构建已完成 C++ 编译和链接，但 UnrealBuildTool 在 `WriteMetadata` 阶段遇到运行环境的 .NET `BadImageFormatException`。
- 使用 `-NoUBA` 重试后结果为 `Succeeded`，目标为最新状态。
- 日志：`Saved/Logs/QuestVerify_Build.log`、`Saved/Logs/QuestVerify_Build_Retry.log`。

Quest 专项测试：

- `ImmortalPath.Quests`：5/5 通过，0 失败，退出码 0。
- 覆盖目录与前置关系、主线领取、每日刷新与时钟回拨、成就正规化及 `SaveSchemaV22`。
- 日志：`Saved/AutomationUserQuestVerify_20260806_1433/Saved/Logs/ImmortalPath.log`。

加入 Quest 后的历史全量回归：

- `ImmortalPath.*`：40/40 通过，0 失败，退出码 0。
- 使用隔离的 UserDir，避免本机旧缓存影响自动化启动。
- 日志：`Saved/AutomationUserFull/Saved/Logs/ImmortalPath.log`。

这里的 40/40 是第 37 步完成时的历史结果。第 38 步随后增加了玩家美术自动化，因此完成玩家比例和落地修正后必须重新运行当时的最新全量测试，不能继续把 40/40 当作当前最终总数。

## 主要代码

- `Source/ImmortalPath/Quests/ImmortalQuestTypes.h/.cpp`
- `Source/ImmortalPath/Quests/ImmortalQuestTests.cpp`
- `Source/ImmortalPath/UI/ImmortalQuestWidget.h/.cpp`
- `Source/ImmortalPath/UI/ImmortalQuestEntryWidget.h/.cpp`
- `Source/ImmortalPath/UI/ImmortalManagementTypes.h`
- `Source/ImmortalPath/UI/ImmortalManagementWidget.h/.cpp`
- `Source/ImmortalPath/Characters/ImmortalPlayerCharacter.h/.cpp`
- `Source/ImmortalPath/Save/ImmortalPathSaveGame.h/.cpp`

