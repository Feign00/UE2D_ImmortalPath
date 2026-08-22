# 第 36 步：统一养成界面、独立修炼与独立飞升

更新时间：2026-08-06

## 目标

本步骤按最新界面规则拆分两种运行状态：

- 历练界面只显示玩家血条和怪物头顶血条。
- 修炼、装备、炼丹、炼器、法宝、功法、灵根/流派、百宝阁、地图、任务、洞府、灵田、宗门、世界妖王、无尽秘境、灵宠和设置全部进入同一个养成界面。
- 点击养成界面的固定导航按钮，只在同一容器内切换背景和功能内容，不再把旧功能窗口叠到历练画面上。
- 飞升是养成容器之外的独立界面，飞升成功后在该界面播放动画。
- 切换任何界面都不暂停世界，不停止怪物生成、自动攻击、掉落结算或独立修炼。

## 最终交互

- 点击左上角玩家血条：进入养成主页。血条上没有可见的额外按钮。
- `Tab`：在历练界面和养成界面之间切换。
- 养成界面左侧：固定双列功能导航。
- 点击功能：右侧 `UWidgetSwitcher` 在原位切换相应页面，并切换该功能对应的阶段背景。
- `Esc`：子页返回养成主页；养成主页返回历练；飞升页返回修炼页。
- `U`：直接进入独立飞升页。
- 旧的 `I/L/K/F/G/H/B/M/C/J/V/N/P` 快捷键仍兼容，但现在会进入同一个养成容器并定位到相应页面。

## 统一界面结构

逻辑尺寸保持 `1600×300`，系统窗口继续使用 TBH `1707×320`，没有改成普通大窗口。

```text
历练世界（始终运行）
  ├─ 玩家/怪物/生成器/掉落/自动攻击
  ├─ 独立自动修炼计时器
  └─ UMG 显示状态
       ├─ 历练：纯血条 HUD
       ├─ 养成：统一 Management 容器
       │    ├─ 固定双列导航
       │    ├─ 阶段 + 功能背景
       │    └─ WidgetSwitcher 当前功能页
       └─ 飞升：独立 Ascension 页面
```

旧功能页只创建一次，并注册到统一容器。当前页面由 `WidgetSwitcher` 激活，其他页面不再作为独立 Viewport 弹窗。旧的 `900×600` 页面暂时通过 `ScaleBox` 等比缩小；后续可逐页改造成原生横向 `1286×238` 布局，不影响现有业务和存档。

## 修炼页

修炼页显示：

- 当前完整境界。
- 当前修为 / 本层所需修为。
- 进度条。
- 每秒自动修为。
- “战斗不产修为”的明确说明。
- 前往独立飞升页的按钮。

修炼由 `UImmortalCultivationComponent` 独立计时。打开宗门、背包或其他页面不会停止修炼；击杀结算的 `ReceiveKillRewards` 不会写入修为。

## 飞升页和动画

- 飞升事务仍先完成全部资格检查和一次原子存档。
- 只有写盘成功并提交新轮回后才播放动画；失败回滚不播放。
- 动画不再替换战斗角色的 Flipbook，不再调用 `StopAutoAttack`，因此后台挂机不中断。
- 动画使用 `/Game/GAME/Asset/Player/ascension/generated/T_Player_Ascension`。
- 原图为 `2176×724`，17 列、12 FPS，约 `1.42` 秒。
- UMG 按帧更新 UV；只裁掉所有帧共有的上下透明区，随后保留完成画面 `1.2` 秒。
- 当前原图多帧的左右边缘已经在源文件中被裁掉；按最新开发范围继续保留现有飞升动画，不把它列入本轮人界动作重制或美术门禁。除非后续单独提出，不重新输出。

## 三界背景规则

当前阶段映射为：

- 人界：炼气、筑基、金丹。
- 灵界：元婴、化神、炼虚。
- 仙界：合体、大乘、渡劫、飞升。

背景软路径规则：

```text
/Game/GAME/Asset/ui/management/backgrounds/{tier}/T_BG_{Tier}_{Feature}
```

其中：

- `{tier}`：`mortal`、`spirit`、`immortal`。
- `{Tier}`：`Mortal`、`Spirit`、`Immortal`。
- `{Feature}`：`Home`、`Cultivation`、`Inventory`、`Alchemy`、`Crafting`、`Artifact`、`Technique`、`CharacterBuild`、`Shop`、`Map`、`Quest`、`Cave`、`Farming`、`Sect`、`WorldBoss`、`EndlessDungeon`、`Pet`、`Settings`。

第 36 步最初完成时共有 17 项；第 37 步把任务作为第 18 项接入同一容器，具体实现和增量验证见 `Docs/Step37_QuestSystem.md`。

例如人界炼丹背景：

```text
/Game/GAME/Asset/ui/management/backgrounds/mortal/T_BG_Mortal_Alchemy
```

页面专属背景缺失时会显示人界绿色、灵界青色、仙界紫色占位，并在页眉显示待补路径；不会暴露空白页面或停止功能。

## 需要制作的素材

### 第一优先级：三个阶段主页背景

- `T_BG_Mortal_Home.png`
- `T_BG_Spirit_Home.png`
- `T_BG_Immortal_Home.png`

统一规格：`1707×320`，RGB 或 RGBA，无文字、无人物、无怪物、无 UI、无水印。左侧约 `300 px` 会放导航，中央和右侧会放功能内容，重要建筑和视觉中心不要放在左侧导航下方。

通用生成描述模板：

> 2D 横版中国修仙挂机游戏的养成界面背景，TBH 超宽窄幅构图，1707×320，{人界山门 / 灵界浮空仙城 / 仙界云海天宫}，中国风手绘游戏美术，清晰的前中远景层次，柔和光照，中间与右侧保留低细节可读区域供 UI 内容显示，左侧 300 像素避免重要主体。纯背景，无人物、无怪物、无文字、无按钮、无边框、无水印。

### 第二优先级：人界功能背景

先制作当前最常用的人界版本：

- 修炼：静室、蒲团、灵气环流。
- 装备：储物戒内部或兵器架。
- 炼丹：丹炉、药柜、灵火。
- 炼器：锻造台、灵火、矿石架。
- 法宝：悬浮法宝陈列台。
- 功法：藏经阁、卷轴与玉简。
- 灵根/流派：五行阵盘、测灵台。
- 百宝阁：仙侠商铺与宝物柜。
- 地图：云雾山河图或历练沙盘。
- 任务：任务卷轴、玉简、宗门令与完成印记。
- 洞府：修士洞府内景。
- 灵田：灵草田、仙泉、木架。
- 宗门：宗门大殿。
- 世界妖王：远山祭坛与封印阵。
- 无尽秘境：秘境入口与旋涡。
- 灵宠：灵兽园。
- 设置：简洁云纹屏风，可复用主页背景。

文件名必须使用上面的 `{Feature}` Token。每张仍为 `1707×320`，遵守主页背景的留白规则。灵界和仙界版本以后按同名 Token 补齐即可，无需改 C++。

### 第三优先级：功能图标

每个功能一枚 `128×128` RGBA 透明图标，共 18 枚。统一正方形构图、同一视角和描边宽度，不含文字。建议分别以蒲团、储物戒、丹炉、铁砧、飞剑、玉简、五行盘、宝箱、山河卷、任务卷轴、洞府门、灵草、宗门令、妖王角、秘境门、灵兽爪、齿轮和仙府印作为主体。

图标生成描述模板：

> 中国风仙侠游戏 UI 功能图标，{功能主体}，正视或轻微俯视，精致手绘，金色与青玉色统一边缘光，轮廓清楚，适合缩小到 48 像素显示，128×128，真正透明 RGBA 背景，无文字、无边框、无水印、主体四周保留 12 像素透明边。

### 飞升背景和现有动画保留

- 独立飞升背景：`1707×320`，云海、天门、上升光柱，中央保留角色区域。
- 现有 17 帧飞升动画继续使用，不属于本轮人界玩家 Idle / Move / Attack / Hurt / Death 重制门禁。除非后续单独提出，不重新生成或替换飞升动画。

## 验证证据

构建：

- `ImmortalPathEditor Win64 Development -DisableUnity -NoUBA`：通过。
- UHT：通过。

自动化：

- `ImmortalPath.*`：35/35 通过，0 失败。
- 最终日志：`Saved/Logs/Step36_Final_Automation.log`。
- 最终报告：`Saved/TestReports/Step36_Final/index.json`。

以上 35/35 是第 36 步完成时的历史证据；任务系统接入后的 5/5 专项与 40/40 历史全量结果见 `Docs/Step37_QuestSystem.md`，不在本节改写原有步骤数字。

实际 1707×320 窗口：

- 切换修炼→宗门期间：`worldPaused=false`。
- 自动攻击计时器：始终 `active=true`。
- 同一段时间内关卡击杀：`2 -> 3`。
- 同一段时间内修为：`9593 -> 9603`。
- 返回历练后仅显示玩家和怪物血条。
- 飞升页打开及动画期间自动攻击仍为 `active=true`。
- 主存档测试后恢复为 SHA256 `E3833E79F60F31F93EFCAD69640DE8FC3010971BF23914AE9F29DFD6CF01B86A`。

截图：

- `Saved/Screenshots/Management_Cultivation_TBH.png`
- `Saved/Screenshots/Management_Sect_TBH.png`
- `Saved/Screenshots/Combat_HealthOnly_TBH.png`
- `Saved/Screenshots/AscensionInterface_TBH.png`

运行日志：

- `Saved/Logs/Step36_Management_Runtime.log`
- `Saved/Logs/Step36_AscensionUI_Runtime.log`

## 主要代码

- `Source/ImmortalPath/UI/ImmortalManagementTypes.h`
- `Source/ImmortalPath/UI/ImmortalManagementWidget.h/.cpp`
- `Source/ImmortalPath/UI/ImmortalCultivationWidget.h/.cpp`
- `Source/ImmortalPath/UI/ImmortalPlayerStatusWidget.h/.cpp`
- `Source/ImmortalPath/UI/ImmortalAscensionWidget.h/.cpp`
- `Source/ImmortalPath/Characters/ImmortalPlayerCharacter.h/.cpp`
