# R07A：人界妖狐与灵狐动作源

制作：2026-10-01～2026-10-02。原创游戏素材，使用 Codex 内置图像生成工具；不是参考游戏素材的提取或改色。现有玩家像素动作仅作为画风参考，选定的 `idle-original-v2.png` 作为后续动作的身份参考。原飞升动画不在此次修改范围。

## 文件与来源

- `sources/*-original-*.png`：生成原图，保留不覆盖。Idle 第一次稿尾巴触及分格边界，保留但不导入；第二次稿为选定来源。
- `*-regions.json`：人工复核的原图裁切区域、SHA-256；死亡动作另有明确的连通域种子，解决第三/第四帧尾巴横向范围交叠，不自动删掉孤立像素。
- `sources/*-regrid-v1.png`：将完整轮廓移入 512×512 格子的中间产物。只平移与硬化透明，不重绘、不缩放，每个 alpha≥128 原图像素均保留且只属于一帧。
- `*-alignment.json`：逐帧人工脚底/躯干根锚点；整套动作统一 0.7 倍等比最近邻整理，没有按帧拉伸，也没有用重复帧补帧。
- `*-v1/atlas.png`：实际导入图集；`review.json` 为确定性整理后的静态报告。
- `Config/ImportDesktopPixelFox.json`：5 张纹理、40 个独立 Sprite、5 个 Flipbook；导入路径为 `/Game/GAME/Asset/Monsters/desktop_pixel_v2/fox`。

所有选定生成原图实际为 1536×1024、RGBA。原图 alpha 最大值为 254，透明边缘不能直接当作已验收游戏素材。整理阈值 128，成品 alpha 为 0/255。工具不从背景颜色猜测轮廓，也不凭最大连通域猜测哪只狐狸属于哪一帧。

## 成品规格

| 动作 | 帧数 | FPS | 播放 |
| --- | ---: | ---: | --- |
| Idle | 8 | 8 | 循环 |
| Move | 8 | 8 | 循环 |
| Attack | 8 | 12 | 单次；接触点 0.25 秒（索引 3） |
| Hurt | 8 | 12 | 单次 |
| Death | 8 | 10 | 单次；末帧保持 |

每张成品 2048×768，4×2 行优先排列；单帧 512×384，脚底 pivot `(256,336)`，2.56 像素/UE 单位，MaskedUnlitSpriteMaterial，源画朝左。普通妖狐场景视觉倍率 2；灵狐宠物复用同源动作，场景视觉倍率 1.25。这是整套精灵的场景显示倍率，不是逐帧拉伸。仅新脚底 pivot 素材使用玩家实际精灵的脚线偏移和景深；碰撞胶囊、地面与攻击距离不改。整理工具使用最近邻，但当前 UE 导入器的纹理过滤仍是既有 Bilinear，不宣称引擎中使用最近邻过滤。

静态检查：40 个帧图均不重复、无越界/碰格边、无不透明洋红像素；Idle/Move/Attack/Hurt 接地轮廓跨度 0 像素，Death 为 1 像素。静态报告不等于运行时动作、正常地图地面或透明桌面合成验收；实际运行结果见 `Docs/Step47A_DesktopPixelFox.md`。

## 生成提示词记录

以下按实际请求整理生成规格与动作序列，非声称模型完美遵循了网格和锚点。跨格尾巴及第二行脚线偏差由上面的显式配置整理，不用裁尾或逐帧缩放补救。

共同身份与美术约束：

> Original Qingyun mountain spirit fox for a Chinese xianxia desktop idle game. Compact crisp outlined 16-bit side-view pixel art. Orange-red fur, ivory muzzle/chest and a single large ivory-tipped tail, dark brown paws, triangular ears, amber eyes, restrained jade-teal forehead marking. Animal on all four legs, not humanoid; no clothing, collar, accessories or nine tails. Strict left-facing profile, same individual and same scale in every frame. Exactly eight distinct authored poses in a row-major 4×2 sprite atlas, ideally 1536×1024. Complete tail/muzzle/legs remain inside each frame with transparent margins. Genuine RGBA transparency between legs, tail and body; opaque crisp edges, limited palette. No painted checkerboard, blur, glow, white halo, scenery, floor, shadow, labels, text, numbers, grid borders, logos or watermark. Production animation atlas, not a poster.

Idle 首稿：

> Eight-frame calm alert breathing loop. Fox including tail occupies about 75% cell width and 55% cell height; grounded paws at local 86% height, torso root at 50% width. Frames: 1 neutral exhale; 2 chest expands and tail starts lifting; 3 inhale rises; 4 full inhale, ears perk; 5 gentle blink and tail settles; 6 exhale lowers; 7 near-neutral ears relax; 8 returns naturally into frame 1. Subtle distinct drawings, no body translation; continuous tail/breathing, not a redesigned fox per frame.

Idle 第二稿修正请求：

> Preserve the exact fox identity, colors, eight idle poses and continuous tail/breathing/blink sequence. Repair production layout and silhouette margins on a 1536×1024 canvas with 384×512 cells. One consistent slightly smaller whole-character scale, full tail and muzzle visible, at least 40 transparent pixels at left/right of every cell, no tail touching a boundary. Body root x=192 and paws baseline y=440 in both rows; silhouette width/standing height no more than 290 pixels. Do not shorten the tail or change the identity. Genuine alpha-zero empty background and opaque sprite pixels. Eight distinct poses, not duplicates.

其余四套均以选定 Idle 第二稿作为狐狸身份参考，沿用共同约束：

- Move：一个八帧步行循环。前后腿交替接触、承重压缩、经过、相反腿接触，再进入反向经过并回到首帧；尾巴随步态连续摆动，身体比例不变，四足接地。约 300 像素站高、躯干根约 x=170、脚线约 y=440，要求至少 24 像素边距。不是平移一张站立图，不加速度线或特效。
- Attack：一次连续的向左咬击。1 警戒；2 压低蓄力；3 重心前移；4 向左张嘴咬击接触；5 合嘴随动；6 收回；7 恢复警戒；8 接回待机。躯干根约 x=190、脚线约 y=440、站高约 300。无火球、闪光、特效、漂浮或独立拉伸。
- Hurt：一次清晰的向右受击回缩。1 警戒；2 闭眼、耳后压并向右回缩；3 最大回缩、膝弯；4 尾巴惯性回落；5 恢复；6 直起；7 耳回正；8 接回待机。躯干根约 x=180、脚线约 y=440。不是攻击或倒地，无血液、打击线或闪光。
- Death：一次永久倒下。1 膝弯；2 前腿失力、胸降低；3 肘着地、后躯降低；4 腹部着地；5 翻向侧面；6 完整侧卧；7 余动落定；8 永久侧卧。全过程同一比例和地面约 y=440，末帧不返回站立；无血液、粒子、淡出、亡魂或消失效果。

## 选定源图 SHA-256

| 动作 | 源图 | SHA-256 |
| --- | --- | --- |
| Idle | `sources/idle-original-v2.png` | `5a6eecf02f17bcd8c7bd483cc7cf4ea0f58914c03ab22218ff1c38d4b99482e5` |
| Move | `sources/move-original-v1.png` | `f58c2c751455ab2ecd092d3fdc92a139c00a510ba55dc6e20cbf0a7f618b00a3` |
| Attack | `sources/attack-original-v1.png` | `9169a41463580ed03d064f60b3a15f22e2e37ef78a458e7967da4cc3cd609e53` |
| Hurt | `sources/hurt-original-v1.png` | `79d6ca862466294181c2542295b628b164e1cebea03c0f318a09512420438cc3` |
| Death | `sources/death-original-v1.png` | `ef84b2102ccbf68d6d3a6e24a973585c03758d6526a9516ea61be18f3e1058e0` |

## 可复现验证

需要 Node 与 sharp。设置本机依赖路径后，从项目根运行：

```powershell
node --test ArtSource/Tools/audit_sprite_atlas.test.mjs ArtSource/Tools/align_sprite_atlas.test.mjs ArtSource/Tools/prepare_sprite_cutout.test.mjs ArtSource/Tools/prepare_authored_sprite_regions.test.mjs
node ArtSource/Tools/verify_fox_sprite_assets.mjs
```

第二条只读地由原图和显式配置重建到内存，并与已交付的 regrid 和最终 PNG 字节比较；不会覆盖文件。独立重建某动作时，给 `prepare_authored_sprite_regions.mjs` 与 `align_sprite_atlas.mjs` 指定新的输出路径，工具拒绝覆盖已有版本。
