# R07B：人界犬／妖狼与灵犬动作源

原图生成：2026-10-06；整理：2026-10-07。使用 Codex 内置图像生成工具制作原创素材，不提取、描摹或改色参考游戏资产。Idle 为新作；其余四套以选定 Idle 原图作为同一个体的身份及风格参考，不是对 Idle 图集的逐帧编辑。旧 Boss 的犬类素材与独立飞升动画不在本批替换范围。

## 文件与处理

- `sources/*-original-v1.png`：五张生成原图，完整保留。
- `*-regions.json`：原图 SHA-256、人工区域及四邻域连通域种子。所有 alpha≥128 的原图像素在重排中保留且只归属一帧，孤立像素也明确归属，不使用最大连通域自动删尾巴或肢体。
- `sources/*-regrid-v1.png`：4×2、512×512 单格，只平移和硬化透明、不缩放的中间产物。
- `*-alignment.json`：人工确认的逐帧脚底／躯干根锚点。五套动作使用同一 0.7 倍等比最近邻整理，无逐帧拉伸或重复帧补帧。
- `*-v1/atlas.png`、`review.json`：最终导入图集及静态检查报告。
- `Config/ImportDesktopPixelDog.json`：5 张纹理、40 个 Sprite、5 个 Flipbook，独立目录 `/Game/GAME/Asset/Monsters/desktop_pixel_v2/dog`。

原图实际 1536×1024 RGBA，alpha 最大值 254；整理阈值 128，成品 alpha 为 0/255。显式区域处理了跨格尾巴和不同排的脚线偏差，不把模型未遵循网格的原图直接当成成品。

## 成品规格

| 动作 | 帧数 | FPS | 播放 |
| --- | ---: | ---: | --- |
| Idle | 8 | 8 | 循环 |
| Move | 8 | 8 | 循环 |
| Attack | 8 | 12 | 单次；接触点 0.25 秒，索引 3 |
| Hurt | 8 | 12 | 单次 |
| Death | 8 | 10 | 单次；末帧保持 |

每张成品 2048×768，4×2 行优先，512×384 单帧；脚底 Custom pivot `(256,336)`、PPUU 2.56、MaskedUnlitSpriteMaterial，源画朝左。普通犬类整套场景视觉倍率 2，SpiritHound 为 1.25；这些是演员整套视觉倍率，不是逐帧拉伸。整理时使用最近邻，UE 纹理过滤仍沿用现有导入器的 Bilinear。

静态检查通过：40 个独立帧，0 半透明、重复、裁切、触边或不透明洋红像素；五套轮廓底部跨度均为 0 像素。静态结果不等于运行时动作流畅、伤害时序、真实地面接地或透明桌面合成验收。运行证据单独记录在 `Docs/Step47B_DesktopPixelDog.md`。

## 生成请求规格记录

以下按实际请求整理共同约束与动作顺序，不声称模型完美遵循网格和锚点。

共同约束：原创青云山四足灵犬／妖狼，紧凑清晰的 16-bit 横版像素风；板岩蓝和深灰毛色，浅银鼻口及胸毛、深色爪、琥珀眼、克制的玉青额纹、竖耳、单条正常低垂蓬松尾巴。严格左向侧视，同一个体、同一比例；无衣服、饰品、光效或九尾。8 个不同姿势，4×2 行优先图集，透明 RGBA，尾巴、鼻口及四肢完整保留且有边距。无背景、地面、阴影、白边、模糊、文字、标号、网格、标志、水印或画出的棋盘格。

- Idle：平静警戒的呼吸循环，胸腔吸气和呼气、轻微耳动、眨眼及连续尾摆；首尾自然相接，不平移身体，不每帧换一个角色。
- Move：完整八帧四足步行／小跑循环，前后腿交替接触、承重、经过和对侧接触，尾巴随步态摆动；不是移动一张站立图，不加速度线。
- Attack：1 警戒；2 下压蓄力；3 前移重心；4 向左张嘴咬击接触；5 合嘴随动；6 收回；7 恢复；8 接回待机。无火球、闪光或独立特效。
- Hurt：警戒→闭眼、压耳、向右回缩→弯膝最大回缩→尾巴惯性回落→恢复→直起→耳回正→接回待机；不是攻击或死亡，无血液、打击线或闪光。
- Death：膝弯→前腿失力、胸降低→肘着地、后躯降低→腹部着地→翻向侧面→完整侧卧→余动落定→永久侧卧；末帧不站回、不消失，不加亡魂、粒子或淡出。

## SHA-256

| 动作 | 原图 `sources/*-original-v1.png` | 成品 `*-v1/atlas.png` |
| --- | --- | --- |
| Idle | `ec2be6485067178a3365f93d9b86a354c96baf046b19c097acb5da893a4f2dec` | `42d4a1b983a440dd6078bba93ccc2c5ecb0e85e1bf8db44414f39973966ced0e` |
| Move | `2190b84d15bf13606ecc94f718cee8335afbefc1346e672f02a744043bcc1fda` | `f5f69603c26d40fb818da6a1a56c0d6b8374b9b8d86a4d4dc2c354b11d03f310` |
| Attack | `b60bfd356bdd848919756218df3f976a9b1a2fff14fcefdaa102ec83e52065cb` | `c9e62d8f34e1db4c127209f469cb0d52370c070ab2e56705b36bc17ff6d55011` |
| Hurt | `0229025d88b0c43e9086c6a38a04524bc4f422a540c6bcdcd6e99e3e07f3f86a` | `2993dd4bd7280e9a8842cb077058b3e8775a4af2c8e77d0fe70cf0a9d07d29c1` |
| Death | `a3e89f9e065252db5657270fa8f442de346b09634535912846a2a72d9817af9a` | `0eee775b653381884a086ab409ba699dc741403779f5c70e75f663c5dd0ffa4c` |

## 可复现只读验证

从项目根目录运行，需要 Node 和 sharp：

```powershell
node --jitless --test ArtSource/Tools/audit_sprite_atlas.test.mjs ArtSource/Tools/align_sprite_atlas.test.mjs ArtSource/Tools/prepare_sprite_cutout.test.mjs ArtSource/Tools/prepare_authored_sprite_regions.test.mjs
node --jitless ArtSource/Tools/verify_dog_sprite_assets.mjs
```

验证器从保留原图及显式配置在内存重建，并与中间／最终 PNG 逐字节比较，同时检查导入规格，不覆盖交付文件。独立输出新的版本时，整理工具要求新路径，不覆盖原图与现有版本。
