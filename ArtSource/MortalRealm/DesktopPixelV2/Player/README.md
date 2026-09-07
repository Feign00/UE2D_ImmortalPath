# 玩家像素动画预览批次 v1

状态：完成透明整理、人工锚点对齐、UE 导入与独立预览；尚未替换游戏默认五动作。

## 来源与处理授权

源图由内置图像生成工具制作，不是参考游戏的提取资产。2026-09-07 用户明确同意使用图像处理程序清理透明边缘、按人工确认的脚底锚点对齐，保留原图，不逐帧拉伸或重复帧冒充动作。

- `sources/idle-alpha-v1.png`、`sources/move-alpha-v1.png`：上批透明提取结果，字节原样保存。不要直接用于运行。
- `idle-alignment.json`、`move-alignment.json`：源图校验值、人工锚点、统一目标位置、整套缩放与阈值。
- `idle-v1/atlas.png`、`move-v1/atlas.png`：整理后的 1536×1024 / 4×2 图集；每帧 384×512，脚底锚点 [176,464]，透明度只有 0 和 255。
- 各动作 `review.json`：逐帧检查及哈希记录。无自动逐帧居中、无光流补帧、无非等比拉伸。

原待机画面人物约高 440 px，移动约高 403 px；待机整套等比缩放 0.92，移动整套为 1.0，统一参考比例。八个待机帧使用同一缩放值，八个移动帧使用同一缩放值。每帧水平锚点由脚位/骨盆参考人工确认，垂直锚点纠正图集排版误差，保留行走屈膝起伏。

## 可复现整理

需要 Node.js 和 `sharp`。输出目录必须不存在，不会覆盖已验收版本。

```powershell
node ArtSource/Tools/align_sprite_atlas.mjs ArtSource/MortalRealm/DesktopPixelV2/Player/idle-alignment.json ArtSource/MortalRealm/DesktopPixelV2/Player/idle-rebuild
node ArtSource/Tools/align_sprite_atlas.mjs ArtSource/MortalRealm/DesktopPixelV2/Player/move-alignment.json ArtSource/MortalRealm/DesktopPixelV2/Player/move-rebuild
```

源图 SHA256 不匹配、锚点非法、可见轮廓被裁切、整理后质量检查失败时拒绝输出。处理器保留人物 RGB，按阈值 128 二值化 alpha，用最近邻采样，不重新绘画面部或服饰。新图集并不代表攻击、受击、死亡或全演员美术已完成。

## 生成提示词记录

最初角色锚点作为人物身份参考：白衣青边汉服、黑色发髻及短马尾、玉佩、深色靴、银色直剑、朝右侧视。图集初稿中误画出的棋盘格未被当作透明通道。

待机图集初稿：

```text
Use case: stylized-concept. Image 1 is CHARACTER IDENTITY REFERENCE ONLY, do not copy its checkerboard background. Create ONE production sprite sheet of this original ivory-and-jade Chinese sword cultivator performing an 8-frame calm battle IDLE breathing loop, strict right-facing side view. Layout: exactly 4 equal columns by 2 equal rows on a 1536x1024 canvas, each frame 384x512. Read left-to-right, top-to-bottom. Same compact 4-head-tall body, black topknot, short dark ponytail, ivory hanfu with dark teal trim, jade belt pendant, dark boots, silver straight sword held down-forward in right hand. Crisp 16-bit pixel art, simplified forms, limited palette, no blur. All eight sprites must have identical scale, head dimensions, clothing and sword. In each cell the feet stay grounded on local y=464, pelvis centered near x=160, head near y=100; full body and sword entirely inside cell with generous clear margin. Actual successive drawn movement: frame 1 neutral low exhale, 2 chest starts rising, 3 mid inhale, 4 full inhale, 5 soft pause and sleeve/ponytail settling, 6 mid exhale, 7 almost neutral, 8 leading naturally into frame 1. Subtle shoulder/sleeve/tassel changes, no step, no translation or camera change. Genuine RGBA TRANSPARENT EMPTY BACKGROUND: outside sprites alpha zero, opaque character pixels, no painted checkerboard, no white/black/colored backdrop, no floor, no shadow, no glow, no fog. No text, numbers, lines, borders, labels, extra figures, or watermark. This image will be cropped as an exact uniform 4x2 atlas and animated in a game; do not arrange a poster or perspective character sheet.
```

待机透明提取：

```text
Use case: background-extraction. Remove the entire background from the supplied 4-by-2 character sprite atlas. Output a genuinely transparent RGBA PNG with alpha zero in ALL empty areas, including between arms, legs, sword and body. Keep every character pixel, all eight poses, pixel art, colors, dimensions, exact locations, identical framing and 1536x1024 canvas unchanged. Opaque hard-edged sprite pixels with no glow or translucent haze. This is alpha extraction only; no background replacement, no new artwork, no text.
```

移动图集初稿：

```text
Use case: stylized-concept. Image 1: reference for CHARACTER IDENTITY ONLY. Create an eight-frame WALK CYCLE sprite atlas for this exact ivory-and-teal Chinese sword cultivator, in strict right-facing side view. One 1536x1024 PNG, 4 columns and 2 rows, each cell 384x512. Clean genuine transparent RGBA background. Opaque crisp hand-pixeled 16-bit game sprite. Keep identical face, black topknot and short ponytail, compact body proportions, jade belt, ivory robe, dark teal boots, silver sword in right hand angled down-forward, consistent lighting and size across frames. Exact animation sequence in row-major order: 1 left foot forward/right foot back contact; 2 left leg weight-bearing downward compression; 3 right foot passes left under pelvis; 4 right knee forward/upward stride; 5 right foot forward/left foot back contact; 6 right leg weight-bearing downward compression; 7 left foot passes right under pelvis; 8 left knee forward/upward stride leading back into frame 1. The feet genuinely change positions; visible complete alternating gait, natural opposite arm movement with restrained sword travel, robe hem and ponytail follow the motion. Not running, no leaps. Body centered consistently at local x=168, average head top y=96, ground baseline y=464 in each cell. At least one planted foot touches the baseline in every frame, all body parts and sword stay inside each cell with margin. No camera movement, no facing change, no enlarged/cropped frames. No labels, lettering, numbers, cell borders, floor, scene, shadow, glow or backdrop. Preserve the empty alpha channel for game use.
```

移动透明提取：

```text
Use case: background-extraction. Image 1 is the edit target: an eight-frame sprite atlas. Remove the painted gray-and-white checkerboard completely, including gaps between limbs and sword. Preserve all eight characters exactly, their positions, colors and 1536x1024 dimensions. Deliver a cutout RGBA PNG with a genuinely transparent background (alpha zero), solid opaque character interiors, clean sharp edges, no halo. Do not replace the background with another color or pattern. No pose changes or new drawings.
```

提示词是生成意图，不是质量保证；原图不足与本批修正、实机验证边界见 `Docs/Step41B_AnimationAssetValidation.md` 和 `Docs/Step41C_PixelPlayerPreview.md`。
