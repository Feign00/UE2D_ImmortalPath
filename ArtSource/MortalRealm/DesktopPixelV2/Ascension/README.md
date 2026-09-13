# 飞升图集 · 41Q 素材准备

2026-09-14（Asia/Shanghai）。内置 image_gen 生成，非 CLI；原创修仙插图，不使用参考游戏的贴图。本阶段是素材准备与引擎导入，尚未接入飞升界面。原角色飞升动画未修改。

## 最终素材与格序

- 最终：ascension-atlas-v2.png，1536×1024 RGBA，3 列×2 行，每格 512×512。
- 从左至右、从上至下：战道（火绕剑）、悟道（卷轴与灵珠）、福缘（锦鲤玉钱）、仙印（仙鹤印玺）、飞升台（云阶玉门）、轮回（云环与明珠）。
- 引擎纹理：/Game/GAME/Asset/DesktopPixelV2/UI/T_AscensionAtlas。
- 导入配置：Config/ImportDesktopAscensionArt.json；不创建或替换角色动画。
- 最终 SHA256：6225080f135b3910d4e8f12792c3e076fbd891f1c794c81394d873e6760bea55。

## 来源与透明处理

1. 2026-09-13 首次请求因 usage_limit_reached 被拒绝，没有产出。额度恢复后于 2026-09-14 重试。
2. 原始输出 ascension-atlas-glow-v1.png（生成标识 exec-1a7a8baa-564b-4b2d-856e-7535182e302d）：实际 1536×1024、RGBA，alpha 0–254，但轮廓外有大范围彩色光晕，不能直接作为运行素材。
3. 背景清理编辑输出 ascension-atlas-source-v1.png（生成标识 exec-8960de4b-980b-4f8b-97d8-32ba1eea56a1）：实际 RGB，仍是烘焙棋盘格，不是真透明。保留为可追溯源图，禁止将该文件导入运行纹理。
4. 按用户此前“同意，使用程序整理并实测”的授权，Build/Art/clean_ascension_atlas.py 复用已有连接背景洪泛算法，使用人工确认的背景/孔洞种子，只处理 SHA256 精确匹配的源图。
5. 连接域检查发现六个独立中性灰背景残点，按明确坐标清除，未采用“删除所有小块”的规则，保留游离火焰与卷轴云饰。共清除 1063482 个背景像素；全部 RGB 与未删除区域的 RGBA 逐一断言不变。
6. 初版程序结果 v1 仅保留于本地 Saved/Automation/DesktopPanels/Ascension41QAtlasV1.png；v2 是可重现的最终版本。原始两幅生成图不覆盖。

最终局部非透明边界（每格 512×512）：

| 图标 | 左、上、右、下 |
|---|---|
| 战道 | 181,26,367,493 |
| 悟道 | 40,87,454,453 |
| 福缘 | 74,58,440,456 |
| 仙印 | 107,29,418,452 |
| 飞升台 | 28,16,482,456 |
| 轮回 | 64,54,451,427 |

提示词要求的 56 像素边距并未全部实现；实际六格边缘均透明且没有跨格。未为凑边距缩放或拉伸单个图标。风格为强描边的玉绿/金色插画，并非逐像素手绘的严格像素素材。72/128 像素的深色底预览可辨，实机显示仍待后续接入验收。

## 提示词与复现

- 完整生成提示词：prompt-v1.txt。
- 完整背景编辑提示词：prompt-cleanup-v1.txt。
- 生成与编辑均使用内置工具，没有改用 API/CLI，也不需要提供密钥。
- 清理命令：python Build/Art/clean_ascension_atlas.py ArtSource/MortalRealm/DesktopPixelV2/Ascension/ascension-atlas-source-v1.png <新的输出路径.png>。
- 单元测试：python -m unittest discover -s Build/Art -p "test_clean_*.py" -v。
- 脚本拒绝覆盖现存输出、源图或预览；需 Pillow 与 NumPy。
- 引擎只读校验：Build/Art/validate_ascension_assets.py。

验证结果与后续接入边界见 Docs/Step41Q_AscensionArtPreparation.md。
