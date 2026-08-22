# 第 38 步：人界全量美术生产与开发门禁

更新时间：2026-08-06

## 门禁结论

本步骤是后续玩法开发之前的强制美术门禁，当前状态为 **进行中，尚未完成**。

执行顺序固定为：

1. 完成本轮已经开始的基本玩法、统一养成界面、任务、存档和运行验证。
2. 暂停新增玩法系统，集中完成人界全部正式美术。
3. 人界玩家 Idle / Move / Attack / Hurt / Death 全部使用同一新版角色重制并实机验收。
4. 完成六张人界地图、全部普通怪与 Boss、统一养成界面、图标、掉落及必要战斗表现。
5. 只有本清单所有阻塞项通过后，才能恢复灵界、仙界或其他后续系统开发。

现有飞升动画按用户最新要求保留，不属于本轮玩家动作重制，也不会因为源图边缘问题阻塞人界美术门禁；除非以后单独提出，不重新生成或替换飞升动画。

## 统一运行规格

### 玩家、怪物与 Boss

- 最终运行帧为独立 `512×512` RGBA PNG。
- 四周保留透明安全边，不得裁切身体、武器、尾巴或必要特效。
- 默认朝右，同一角色的外观、比例、颜色、武器和锚点保持一致。
- 站立动作脚底中心统一为 `(256,448)`；倒地动作最低接触点保持同一地面基线。
- 不包含场景、文字、水印、地面阴影或残留绿幕。
- 源稿可以是独立帧或规则 Sheet；进入项目的最终验收对象必须是经过逐帧检查的独立透明 PNG。

### 地图和统一界面

- TBH 运行尺寸为 `1707×320`。
- 战斗地图需要稳定可站立道路，并与玩家脚底和怪物脚底处于同一视觉地面。
- 统一养成背景左侧为固定导航保留安全区，中右区域为内容页保留可读空间。
- 背景不得包含角色、怪物、掉落、文字、按钮或水印。
- 需要视差的战斗地图应使用共享画布和同一地平线输出前景、中景和远景层。

## 玩家新版角色：当前实际进度

玩家 Idle / Move / Attack / Hurt / Death 已完成第三次 TBH 实机验收，当前状态为 **完成（当前版本）**。这只代表玩家五套基础动作通过，不代表第 38 步全人界美术总门禁已经完成。

### 已完成的资产工作

- 人界角色锚点：`ArtSource/MortalRealm/Player/Player_Mortal_Anchor_v1.png`
- Idle：8 帧，最终目录 `ArtSource/MortalRealm/Player/Animations/Idle/frames_final/`
- Move：8 帧，最终目录 `ArtSource/MortalRealm/Player/Animations/Move/frames_final/`
- Attack：8 帧，最终目录 `ArtSource/MortalRealm/Player/Animations/Attack/frames_final/`
- Hurt：6 帧，最终目录 `ArtSource/MortalRealm/Player/Animations/Hurt/frames_final/`
- Death：8 帧，最终目录 `ArtSource/MortalRealm/Player/Animations/Death/frames_final/`
- 透明化、分帧和统一锚点工具：`ArtSource/Tools/prepare_chroma_sprite_sheet.mjs`
- 导入配置：`Config/ImportMortalPlayerArt.json`
- 运行资产：`/Game/GAME/Asset/Player/mortal/generated/`

已生成 Flipbook：

- `FB_Player_Mortal_Idle`：8 帧，8 FPS，循环。
- `FB_Player_Mortal_Move`：8 帧，10 FPS，循环。
- `FB_Player_Mortal_Attack`：8 帧，12 FPS，非循环。
- `FB_Player_Mortal_Hurt`：6 帧，12 FPS，非循环。
- `FB_Player_Mortal_Death`：8 帧，10 FPS，非循环并停留终帧。

动画状态机已经接入玩家 C++：移动速度驱动 Idle/Move，自动攻击、受击、死亡触发对应一次性动画，自动复活后恢复 Idle。独立飞升 UMG 使用的 17 帧动画未被替换。

### 已通过的程序验证

- 导入日志：`Saved/Logs/MortalPlayer_Import.log`
- 导入结果：38 张纹理、38 个 Sprite、5 个 Flipbook，0 错误、0 警告。
- 自动化：`ImmortalPath.Art.MortalPlayerAnimationSet` 1/1 通过，退出码 0。
- 自动化日志：`Saved/Logs/MortalPlayer_Art_Automation.log`
- 自动化已验证五套 Flipbook 的帧数、FPS、每帧 `512×512`、Pivot `(256,448)`，并验证旧飞升 Flipbook 仍为 17 帧。
- 初次实机诊断日志：`Saved/Logs/MortalPlayer_Runtime.log`
- 初次实机诊断截图目录：`Saved/RuntimeUserMortalArt/Saved/Screenshots/`

### 第三次实机视觉验收：通过（当前版本）

前两次实机检查发现角色比例过小、脚底偏高。第三次调整采用以下最终参数和定位顺序：

- 人界玩家 Sprite 默认视觉缩放：`MortalRealmVisualScaleMultiplier=2.0`。
- 先把动画帧脚底 Pivot 对齐玩家胶囊体底部。
- 再应用 `MortalRealmGroundOffset=-115 cm` 的视觉偏移。
- 只调整 Sprite 组件的视觉比例和相对位置，不移动 Actor，也不修改胶囊碰撞体的位置或碰撞规则。

最终 1707×320 画面中，玩家脚线与刷怪前景怪物的视觉脚线对齐，角色主体避开左上角玩家血条；Idle、Attack、Hurt 和 Death 均保持相同比例与地面基线。玩家五套基础动作因此可以标记为“完成（当前版本）”，现有独立飞升动画继续保留。

最终证据：

- 运行日志：`Saved/Logs/MortalPlayer_Runtime_Scale20_Offset115.log`
- Idle：`Saved/RuntimeUserMortalArtScale20Offset115/Saved/Screenshots/MortalPlayer_Idle_TBH.png`
- Attack：`Saved/RuntimeUserMortalArtScale20Offset115/Saved/Screenshots/MortalPlayer_Attack_TBH.png`
- Hurt：`Saved/RuntimeUserMortalArtScale20Offset115/Saved/Screenshots/MortalPlayer_Hurt_TBH.png`
- Death：`Saved/RuntimeUserMortalArtScale20Offset115/Saved/Screenshots/MortalPlayer_Death_TBH.png`

## 六张人界战斗地图总清单

| 地图 | 普通怪主题 | Boss | 背景与分层 | 普通怪全动作 | Boss 全动作 | 实机验收 |
|---|---|---|---|---|---|---|
| 青云山 | 青云妖兽 | 青云妖王 | 运行背景已有，正式分层待总审 | Fox/Dog 旧素材可运行，正式统一待审 | 九尾狐素材不完整且需重制 | 待完成 |
| 妖狼谷 | 噬月妖狼 | 啸月狼王 | 待制作 | 待制作 | 待制作 | 待完成 |
| 万妖林 | 万妖林灵兽 | 万妖树皇 | 待制作 | 待制作 | 待制作 | 待完成 |
| 黑风洞 | 黑风魔兽 | 黑风魔君 | 待制作 | 待制作 | 待制作 | 待完成 |
| 上古遗迹 | 遗迹傀儡 | 遗迹守将 | 待制作 | 待制作 | 待制作 | 待完成 |
| 幽冥谷 | 幽冥妖灵 | 幽冥鬼王 | 待制作 | 待制作 | 待制作 | 待完成 |

每套可战斗单位至少需要 Idle、Move、Attack、Hurt 和 Death；Boss 技能动作只有在对应技能已经进入基本玩法范围时才成为本门禁阻塞项。青云山的详细规格和现有素材问题继续由 `Docs/QingyunArtAssetChecklist.md` 管理。

## 统一养成界面美术总清单

统一养成容器当前共有 18 项功能：

```text
Home
Cultivation
Inventory
Alchemy
Crafting
Artifact
Technique
CharacterBuild
Shop
Map
Quest
Cave
Farming
Sect
WorldBoss
EndlessDungeon
Pet
Settings
```

人界需要：

- 18 张对应 Token 的 `1707×320` 正式背景。
- 18 枚对应功能的 `128×128` RGBA 透明图标。
- 统一按钮底板、选中态、禁用态、页签和面板装饰。
- 玩家、装备、材料、任务、地图和 Boss 所需的头像、缩略图与物品图标。

Quest 的人界背景必须命名为 `T_BG_Mortal_Quest`，不能遗漏或用第 17 项之后的临时命名替代。

## 掉落与战斗表现

人界还需要统一完成并实机检查：

- 通用装备光球和装备品质颜色表现。
- 灵石实体掉落及拾取反馈；金色外观在玩法语义中仍表示灵石，不是独立金币货币。
- 必要材料、装备和任务奖励图标。
- 玩家与怪物血条的最终皮肤。
- 普通攻击、受击、死亡和掉落所需的轻量特效；不得遮挡 TBH 窄屏中的角色或血条。

## 门禁验收条件

只有同时满足下列条件，才能把第 38 步标记为完成：

- 已通过：新玩家五套动作在 TBH 实机中比例正确、脚底落地、动作状态无闪回或错帧。
- 六张人界地图背景及所需层级全部接入并通过 `1707×320` 截图验收。
- 六套普通怪和六套 Boss 的阻塞动作齐全、透明、无裁边、基线稳定，并在真实战斗中播放。
- 18 张人界功能背景和 18 枚功能图标全部按固定 Token 接入。
- 掉落、血条和必要物品图标不再使用程序占位或临时素材。
- 导入过程可以幂等重跑，不产生 `_1` 资产、重复 Sprite 或重定向器。
- 自动化专项与当时最新的 `ImmortalPath.*` 全量回归均为 0 失败。
- 主存档在测试前后保持不被测试数据污染。

在上述条件全部满足之前，后续玩法开发保持暂停；本文件中的“已生成”或“已导入”均不等于“门禁已通过”。
