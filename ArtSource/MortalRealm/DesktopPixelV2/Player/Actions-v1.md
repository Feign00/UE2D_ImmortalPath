# 第 41D 部分：攻击、受击、死亡生成记录

2026-09-07，使用内置图像生成工具（不是 CLI），以本目录 `idle-v1/atlas.png` 为身份参考制作三套新动作；每套另调用一次透明提取。输出尺寸实际为 1774×887，而非提示词要求的 2048×1024。初稿及透明提取结果均原样保存在 `sources/`，校验值见各 `*-cutout.json`。没有提取参考游戏的资产。

## 整理方法

用户已授权程序清理与人工锚点对齐。透明提取图存在红黄杂边，因此只使用其 alpha 遮罩，RGB 恢复为对应初稿的原始颜色。`prepare_sprite_cutout.mjs` 按 128 阈值生成硬透明；攻击/受击额外移除与画布外缘连通的明亮中性色棋盘格区域，封闭衣服内部不按白色删除。死亡原稿为黑底，关闭明亮背景清理，保留黑发和深色描边。

右侧补 2 px、底部补 33 px 透明留白，得到 1776×920，以 444×460 分格；行分界位于 y=460，完整保留死亡第四帧越过旧 y=444 分界的剑尖。没有裁去剑尖、缩短武器或对单帧拉伸。

每套使用一个统一等比缩放：攻击 1.26、受击 1.08、死亡 1.10。输出单帧为 576×512，Pivot [220,464]；加宽仅为了完整容纳挥出的剑，人物比例不随画布变化。像素密度与前批 Idle/Move 同为 2.56。

攻击按原稿索引 `[0,1,2,4,3,5,6,7]` 排列，接触姿态位于输出索引 3，12 FPS 时从开始到该帧为 0.25 秒。这只是素材时间定义，尚未将新版图集接到正式伤害事件。受击 12 FPS，死亡 10 FPS，各 8 张不同帧、FrameRun=1。

死亡锚点取脚/膝/倒地身体的接地点，剑尖允许低于接地线。其轮廓底部差 28 px（目标密度下约 10.94 UE 单位），单独审查容差为 30 px；不能据此宣称死亡轮廓底部差为零。攻击与受击继续使用 2 px 容差。

## 重建

从项目根目录运行；输出文件/目录必须不存在。三个动作把下面的 attack 分别替换为 hurt、death。完整源图哈希、遮罩哈希、padding、锚点和排列都在配置中。

```powershell
node ArtSource/Tools/prepare_sprite_cutout.mjs ArtSource/MortalRealm/DesktopPixelV2/Player/attack-cutout.json ArtSource/MortalRealm/DesktopPixelV2/Player/sources/attack-cutout-v1.png
node ArtSource/Tools/align_sprite_atlas.mjs ArtSource/MortalRealm/DesktopPixelV2/Player/attack-alignment.json ArtSource/MortalRealm/DesktopPixelV2/Player/attack-v1
```

不要覆盖已入库文件进行重建；验证应输出到忽略的 Saved 目录，并逐字节比对已有产物。

## 完整提示词

### 攻击初稿

```text
Use case: stylized-concept. Image 1 is CHARACTER IDENTITY AND ART STYLE reference only. Generate a new production sprite atlas of this exact ivory-and-jade Chinese sword cultivator performing ONE complete right-facing sword ATTACK. Canvas 2048x1024, exactly 4 columns by 2 rows, eight equal 512x512 cells in row-major sequence. Same compact young man, topknot, short ponytail, ivory hanfu and teal trims, jade pendant, boots and silver straight sword; identical head size and body proportions to reference. Character standing height about 400px. Ground baseline local y=464 and body root x=220 in every cell. Crisp pixel art, not realistic painting. Frame sequence: 1 neutral sword down-forward matching reference idle, 2 bend knees and draw sword back near shoulder, 3 lift sword up-back with anticipation, 4 swing downward and forward with hips driving, 5 contact pose sword extended horizontally to the RIGHT at chest height, 6 follow-through with blade low-right, 7 draw sword back to down-forward and stand, 8 return close to reference idle. A single connected slash, one sword only, believable hands and elbows, continuous motion arc. Keep all body parts and sword inside each cell with clear margin, no adjoining cells overlap. Both feet grounded with no scene translation. GENUINE transparent RGBA background, no checkerboard or solid backdrop. No floor, no shadow, no slash glow, no speed lines, no text, numbers, labels, grid lines, logo or watermark. Full body in every cell, fixed camera and facing.
```

### 攻击透明提取

```text
Use case: background-extraction. Image 1 is the edit target. Remove only the entire checkerboard background from this eight-frame sword attack sprite sheet, including gaps between hands and blade. Preserve every character, pose, sword, color, detail, scale, layout and original canvas size exactly. Deliver genuine alpha transparency outside the sprites, no painted background, no glow, no haze. Keep ivory fabric opaque. Do not redraw or reposition characters.
```

### 受击初稿

```text
Use case: stylized-concept. Image 1 is character identity and pixel-art style reference. Create a NEW eight-frame HURT RECOVERY sprite atlas of this exact ivory-and-teal Chinese sword cultivator, strict right-facing side view. 2048x1024 canvas, 4 equal columns by 2 equal rows, each cell 512x512. Same youthful face, black topknot and short ponytail, ivory hanfu, teal trim, jade belt pendant, dark boots, single silver straight sword. Standing height about 400px, no changes of scale, anatomical proportions, costume or camera. Ground baseline local y=464 in every cell, center of stance x=220. Frames row-major: 1 neutral guard sword down-forward; 2 struck from the right, shoulders recoil slightly left, eyes squeeze closed; 3 maximum restrained backward recoil, bent knees, sword remains grasped; 4 recoil settles with robe and ponytail lag; 5 begin straightening knees; 6 shoulders return forward; 7 recover guard; 8 calm original neutral pose ready to idle. All eight are successive distinct drawings of a SHORT flinch recovery, no falling, no stepping away, no attack slash. Feet grounded and consistent position, full body and blade within every cell with margin. Crisp outlined pixel game art. Genuine RGBA alpha transparent empty background, no drawn checkerboard, no solid background, no floor or shadow, no sparks or glow. No blood, wounds, text, frame numbers, borders or watermark.
```

### 受击透明提取

```text
Use case: background-extraction. Image 1 is the edit target. Remove only the entire checkerboard background from this eight-frame hurt-recovery sprite sheet, including gaps between arms, legs and sword. Preserve every character, pose, color, detail, scale, layout and original canvas size exactly. Deliver genuine alpha transparency outside the sprites, no painted background, no glow, no haze. Keep ivory fabric opaque. Do not redraw or reposition characters.
```

### 死亡初稿

```text
Use case: stylized-concept. Image 1 is character identity/style reference. Create a new eight-frame NON-LOOPING DEATH / COLLAPSE sprite atlas of this exact ivory-and-jade sword cultivator for a side-view pixel game. Canvas 2048x1024, 4 equal columns and 2 equal rows, 512x512 cells, row-major order. Same black topknot/short ponytail, youthful face, ivory robe with teal border, jade belt ornament, dark boots, single silver straight sword, same pixel-art proportions and lighting. Character begins facing RIGHT, upright height about 400px. Every cell uses SAME scale and ground line y=464; root reference x=220. Sequence: 1 wounded upright beginning knee buckle; 2 knees sag and shoulders slump; 3 kneel on one knee, lowered head; 4 second knee reaches ground and torso leans forward; 5 support with free hand, lower onto left hip; 6 torso descends gently sideways toward ground; 7 lie on side curled slightly, head low with eyes closed, sword down beside hand; 8 settled still body lying on side, eyes closed, robe and ponytail settled, permanently down. Continuous physically connected fall, not disappearing, not standing up again, no flat rotation of one rigid standing sprite. All body and sword fit inside cell; crouch naturally rather than shrinking the person; no cross-cell clipping, no perspective/camera change. Genuine transparent RGBA empty background, no checkerboard, solid fill, floor, shadow, magical effects or glow. Non-graphic: no blood, injuries, gore. No labels, numbers, borders or watermark.
```

### 死亡透明提取

```text
Use case: background-extraction. Image 1 is the edit target. Remove only the entire black background from this eight-frame character collapse sprite sheet, including all gaps around the sword and limbs. Preserve black hair, dark outlines and boots; do not erase black pixels belonging to the character. Preserve all eight different poses, colors, exact positions, size and framing. Return a cutout on a genuinely transparent RGBA background with alpha zero outside the body/sword. No glow, haze, shadow, new background, redraw or repositioning. Keep the fully lying-down final two frames intact.
```
