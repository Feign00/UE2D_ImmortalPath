# 《仙途》青云山美术素材清单

更新时间：2026-08-06

本文档只约束游戏运行素材。源稿、绿幕稿和拼接预览图可以保留，但不能替代最终逐帧 PNG。

本文件是人界总门禁中的青云山子清单；全人界进度、六张地图和统一界面验收以 `Docs/Step38_MortalRealmArtProduction.md` 为准。

## 一、统一交付规范

### 角色与怪物动画

- 每一帧必须是独立的 `512×512` PNG。
- 必须为真正的 RGBA 透明背景，不能保留绿幕 RGB。
- 每帧四周至少保留 `32 px` 透明安全边。
- 角色脚底中心固定在 `(256, 448)`。
- 同一角色在所有动作中的身体大小、服装、颜色、武器和朝向必须一致。
- 默认朝右；程序会根据移动方向自动翻转。
- 禁止阴影地面、场景、文字、边框、水印和被裁切的尾巴、武器、法术特效。
- 不要先生成拼接大图再切帧。应先生成独立帧，最后可额外提供 Sheet 作为预览。

### TBH 地图背景

- 最终运行构图必须为 `1707×320`，或者提供可无损裁成该比例的高分辨率母图。
- 角色脚底对应画面 `y≈230`，因此可站立道路需要从 `y=225~235` 开始并延伸到底边。
- 背景不能包含角色、怪物、掉落物、UI、文字或水印。
- 关键交互区域不要使用过亮高光或密集细节，以免影响人物和血条辨识。

## 二、当前素材状态

| 素材 | 当前状态 | 处理结论 |
|---|---|---|
| 青云山完整背景 | 已提供 | 原图保留；TBH 运行构图、导入与实机接入已完成 |
| Fox 普通怪 | Move / Attack / Hurt / Death 已有 | 已接入现有战斗 |
| Dog 普通怪 | Move / Attack / Hurt / Death 已有 | 已接入现有战斗 |
| 玩家人界基础动作 | 完成（当前版本）：新锚点及 Idle 8 / Move 8 / Attack 8 / Hurt 6 / Death 8 已生成并导入 | 程序、自动化和第三次 TBH 实机验收通过；默认 `MortalRealmVisualScaleMultiplier=2.0`，脚底对齐胶囊底部后 `MortalRealmGroundOffset=-115 cm` |
| 玩家飞升动画 | 已提供 17 帧规则 Sheet | 已导入并迁入独立飞升界面；按最新范围保留，不参与本轮动作重制，也不阻塞人界门禁 |
| 统一养成界面背景 | 缺少 | 逻辑和三界回退色已完成；需按 `Step36_UnifiedManagementInterface.md` 生成 |
| 统一功能图标 | 缺少 | 当前使用文字按钮；含 Quest 在内需生成 18 枚 `128×128` RGBA 图标 |
| 九尾狐 Boss 立绘 | 已提供 | 可用于头像或静态预览 |
| 九尾狐 Boss Idle | 已提供 8 帧 | 存在裁边与约 114 px 基线跳动，需要重新输出 |
| 九尾狐 Boss Move | 已提供 8 帧 | 存在裁边与约 96 px 基线跳动，需要重新输出 |
| 九尾狐 Boss Attack | 已提供 10 帧 | 特效裁边、基线不统一，需要重新输出 |
| 九尾狐 Boss Hurt | 缺少 | 下一批必须制作 |
| 九尾狐 Boss Death | 已提供 10 帧 | 存在裁边、基线跳动和潜在绿边，需要重新输出 |

## 三、下一批必须制作

### 1. 九尾狐 Boss Idle

建议帧数：8 帧，循环播放，8 FPS。

生成描述：

> 2D 横版仙侠 ARPG Boss 动画逐帧素材，青云山九尾灵狐，白色长毛、九条完整尾巴、青蓝色灵火、金色仙纹装饰，身体侧视并朝右。制作安静但有生命感的待机循环：呼吸起伏、耳朵轻动、九条尾巴缓慢摆动、灵火微弱流动，首尾姿势连续。每帧独立 512×512 PNG，真正透明 RGBA 背景，脚底中心严格固定在 (256,448)，角色比例和位置完全一致，四周至少 32 像素透明安全边，所有尾巴和特效完整留在画布内。无地面、无阴影、无场景、无文字、无水印、无绿幕。

### 2. 九尾狐 Boss Move

建议帧数：8 帧，循环播放，10 FPS。

生成描述：

> 2D 横版仙侠 ARPG Boss 移动动画逐帧素材，同一只青云山九尾灵狐，外观必须与待机帧完全一致，侧视朝右。制作稳定向右行走或轻盈奔跑的 8 帧循环，四肢运动清晰，身体起伏幅度小，九条尾巴顺势摆动但不能遮住身体，首尾动作连续。每帧独立 512×512 透明 PNG，脚底中心固定在 (256,448)，角色大小和水平位置一致，四周至少 32 像素透明边，不能裁掉耳朵、尾巴、爪子或灵火。无地面、无影子、无场景、无文字、无水印、无绿幕。

### 3. 九尾狐 Boss Attack

建议帧数：10 帧，非循环，12 FPS。

生成描述：

> 2D 横版仙侠 ARPG Boss 普通攻击逐帧动画，同一只青云山九尾灵狐，侧视朝右。完整动作顺序为蓄力、前扑挥爪、释放青蓝色弧形灵力斩、命中峰值、收招恢复，共 10 帧。攻击特效必须始终完整位于 512×512 画布内，不能触碰边缘；角色外观、大小和脚底位置与待机完全一致。每帧独立透明 RGBA PNG，脚底中心固定在 (256,448)，四周至少 32 像素透明安全边。无场景、无地面阴影、无文字、无水印、无绿幕。

### 4. 九尾狐 Boss Hurt

建议帧数：4~6 帧，非循环，12 FPS。

生成描述：

> 2D 横版仙侠 ARPG Boss 受击逐帧动画，同一只青云山九尾灵狐，侧视朝右。表现受到来自左侧攻击后的短促后仰、毛发和九尾震动、青蓝灵火闪烁，然后迅速恢复战斗姿势；不要倒地，不要表现死亡，不要改变角色设计。制作 6 帧非循环动作。每帧独立 512×512 真透明 RGBA PNG，脚底中心固定在 (256,448)，角色大小与其他动作一致，四周至少 32 像素透明安全边，所有尾巴完整。无背景、无地面、无血腥、无文字、无水印、无绿幕。

### 5. 九尾狐 Boss Death

建议帧数：10 帧，非循环，10 FPS，最后一帧停留。

生成描述：

> 2D 横版仙侠 ARPG Boss 死亡逐帧动画，同一只青云山九尾灵狐，侧视朝右。动作顺序为失去力量、身体下沉、九尾散开、倒地、青蓝灵火消散，最后保持清晰的倒地终帧，共 10 帧；不能突然缩放或变成另一只角色。每帧独立 512×512 真透明 RGBA PNG，角色与其他动作比例一致，站立阶段脚底中心保持在 (256,448)，倒地后最低接触点仍对齐 y=448，四周至少 32 像素透明边。无绿色残边、无背景、无地面、无文字、无水印、无绿幕。

## 四、后续建议素材

这些不阻塞当前青云山战斗，但在 Boss 完整后依次制作：

1. 九尾狐 Boss 技能动画：尾焰爆发，10~12 帧。
2. 九尾狐 Boss 召唤动画：召唤小怪，8~10 帧。
3. 九尾狐 Boss 二阶段转换动画：8~12 帧。
4. Boss 头像：`512×512`，透明 PNG，完整头部和九尾轮廓。
5. 青云山地图缩略图：`512×288`，不含 UI 和文字。
6. 前景遮挡层：`1707×320` RGBA，仅包含底部近景草、石块和轻雾。
7. 中景视差层：`1707×320` RGBA，仅包含中距离山体与云。
8. 远景层：`1707×320` RGB，天空与最远山脉。

分层背景必须共享同一个 `1707×320` 画布和同一地平线，禁止分别生成不同尺寸后强行叠加。

## 五、青云山背景接入记录

已完成内容：

- 保留用户原图 `Content/GAME/Asset/backgrounds/qingyun_mountain_side.png`。
- 新增非破坏派生稿 `qingyun_mountain_side_tbh_v2.png`。
- 通过 `Config/ImportQingyunArt.json` 幂等生成：
  - `/Game/GAME/Asset/backgrounds/generated/T_QingyunMountain_Background`
  - `/Game/GAME/Asset/backgrounds/generated/SP_QingyunMountain_Background`
- Sprite 运行区域为源图 `(1,200,2048,384)`，PPU 为 `1.2`，画面比例等于 `1707×320`。
- 青云山地图显示新背景；其他地图继续使用原有场景表现。
- 新背景加载或镜头反投影失败时，旧 TileMap 会保持可见，不会出现空白场景。
- 新背景只隐藏旧 TileMap 的渲染，不修改其碰撞，因此角色和怪物仍使用原有可靠地面。
- 摄像机参数保持不变：透视 FOV `115`、水平前置 `850 cm`、TBH 窗口 `1707×320`。
- 背景使用普通世界贴图压缩；Boss 透明动画仍保留独立的无损导入设置。

实机结果：

- 角色保持在中间偏左区域。
- 玩家、宠物、普通怪和头顶血条同时可见。
- 新道路边界与角色脚底对齐。
- 运行日志记录 `Qingyun runtime background fitted`。
- 截图：`Saved/Screenshots/QingyunBackground_TBH_1707x320.png`
- 运行日志：`Saved/Logs/QingyunBackground_TBH_Runtime.log`
- 导入日志：`Saved/Logs/QingyunBackground_Import.log`

自动导入命令：

```powershell
& 'E:\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\UE2D\ImmortalPath\ImmortalPath.uproject' `
  -run=ImmortalPaperImport `
  '-ImportSettings=D:\UE2D\ImmortalPath\Config\ImportQingyunArt.json' `
  -unattended -nop4 -nosplash
```

第二次及以后执行会更新固定资产，不会生成 `_1`、重复 Sprite 或重定向器。

## 六、玩家飞升动画接入记录

- 原图：`Content/GAME/Asset/Player/player_ascension.png`。
- 尺寸：`2176×724`，17 列、1 行，每格 `128×724`。
- 生成 1 个纹理、17 个 Sprite 和 1 个 12 FPS Flipbook。
- 导入配置：`Config/ImportPlayerAscension.json`。
- 运行资产：`/Game/GAME/Asset/Player/ascension/generated/`。
- 动画只在独立飞升 UMG 页面内播放，后台自动战斗不会停止。
- 自动化已校验 17 帧、12 FPS、每帧 UV、尺寸和共享 Pivot。
- 当前多帧的光环、衣袖或尾迹已经触碰源格左右边缘，程序无法恢复被裁掉的像素；但按最新开发范围保留现有飞升动画，不重新生成，也不把它列为人界美术门禁阻塞项。
- 实机截图：`Saved/Screenshots/AscensionInterface_TBH.png`。
- 运行日志：`Saved/Logs/Step36_AscensionUI_Runtime.log`。

## 七、玩家人界基础动作接入记录

- 角色锚点：`ArtSource/MortalRealm/Player/Player_Mortal_Anchor_v1.png`。
- 最终透明帧：Idle 8、Move 8、Attack 8、Hurt 6、Death 8，统一为 `512×512`，Pivot `(256,448)`。
- 导入配置：`Config/ImportMortalPlayerArt.json`。
- 运行资产：`/Game/GAME/Asset/Player/mortal/generated/`。
- 导入结果：38 张纹理、38 个 Sprite、5 个 Flipbook，0 错误、0 警告。
- 自动化：`ImmortalPath.Art.MortalPlayerAnimationSet` 1/1 通过；日志 `Saved/Logs/MortalPlayer_Art_Automation.log`。
- 状态机已接入 Idle / Move / Attack / Hurt / Death；原有独立飞升动画保持不变。
- 第三次实机验收采用默认视觉缩放 `MortalRealmVisualScaleMultiplier=2.0`：先将逐帧脚底 Pivot 对齐胶囊体底部，再应用 `MortalRealmGroundOffset=-115 cm`。
- 调整只作用于 Sprite 组件的视觉缩放和相对位置；玩家 Actor 与胶囊碰撞体位置没有改变，既有移动、攻击范围和碰撞规则保持不变。
- 最终 TBH 画面中，玩家与刷怪前景怪物的视觉脚线对齐，且角色主体避开左上角玩家血条；Idle、Attack、Hurt、Death 的比例与地面基线一致。
- 玩家五套基础动作现标记为“完成（当前版本）”；原有独立飞升动画仍保持不变。
- 最终运行日志：`Saved/Logs/MortalPlayer_Runtime_Scale20_Offset115.log`。
- 最终四张验收截图：
  - `Saved/RuntimeUserMortalArtScale20Offset115/Saved/Screenshots/MortalPlayer_Idle_TBH.png`
  - `Saved/RuntimeUserMortalArtScale20Offset115/Saved/Screenshots/MortalPlayer_Attack_TBH.png`
  - `Saved/RuntimeUserMortalArtScale20Offset115/Saved/Screenshots/MortalPlayer_Hurt_TBH.png`
  - `Saved/RuntimeUserMortalArtScale20Offset115/Saved/Screenshots/MortalPlayer_Death_TBH.png`
- 完整门禁与后续验收见 `Docs/Step38_MortalRealmArtProduction.md`。
