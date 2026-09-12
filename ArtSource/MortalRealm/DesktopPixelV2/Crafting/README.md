# 炼器 / 装备图集（41O 素材准备）

2026-09-12。内置 image_gen 生成，非 CLI；原创仙侠图标，没有复制参考游戏的素材。
当前完成生成、透明整理及引擎导入；尚未接入炼器 UI，不能当作炼器大面板或全局装备图标替换完成。

## 文件与格序

- `equipment-atlas-v1.png`：原始输出保留 alpha，实际 1254×1254，3×3 格，每格 418×418；并非提示词要求的 1536。格序：剑、冠、衣、护腕、腰带、靴、左戒、右戒、玉佩。仅用于装备部位图标，不代表九件唯一装备，也不区分品质。输出源标识 `exec-e8fc3e4c-57a5-4334-942a-76b8653c9c6b.png`。
- `forge-atlas-source-v1.png`：最后一次生成编辑的原图，1536×1024，不透明棋盘格；只作可追溯源文件，**禁止作为运行纹理导入**。输出源标识 `exec-3fbb6f80-3bbc-4308-9598-03979fc39da2.png`。
- `forge-atlas-v1.png`：程序从上述原图按人工确认的背景种子清理，1536×1024，3×2 格，每格 512×512。格序：妖骨、灵铁、法宝残片、灵木、锻砧、炼器炉。保留全部 RGB 与未被背景洪泛选中的像素，不缩放、不逐格拉伸。
- 矿石及灵石计划复用上一批 `../Alchemy/material-atlas-v1.png`，不另造同义图标。
- 导入配置：`Config/ImportDesktopCraftingArt.json`；运行纹理 `/Game/GAME/Asset/DesktopPixelV2/UI/T_EquipmentAtlas` 与 `T_ForgeAtlas`。

## 透明整理与检查

生成首版出现背景光晕；第一次编辑与第二次编辑均产生烘焙棋盘格，alpha 255，不能作为透明素材使用。
遵循用户此前“同意，使用程序整理并实测”的授权，使用 `Build/Art/clean_forge_atlas.py` 从外部灰白背景洪泛，不对所有灰色像素直接抠色。深色轮廓阻止洪泛，两个炉环孔用人工确认的额外种子。
背景删除 976501 像素，保留 596363 像素；四条画布边缘 alpha 为 0，保留像素和 RGB 数组逐一断言不变。六格局部非透明包围盒：

```text
妖骨       58,58 — 457,469
灵铁       26,101 — 482,463
法宝残片   37,66 — 454,463
灵木       61,73 — 457,417
锻砧       26,46 — 478,427
炼器炉     40,13 — 453,449
```

实际边距没有全部达到提示词要求的 60 像素，但均没有跨格。深墨绿底合成检查未见棋盘格/光晕，骨、铁、炉体和炉环结构保留；实机小尺寸阅读及 UV 裁切待 UI 接入后检查。
装备图每 8 像素抽样：alpha 0 为 15254，alpha 255 为 17，其他为部分透明；保留生成透明通道，实机可见度仍需检查。
清理流程首次 Python 执行退出 1 且无诊断输出，未生成最终文件；限制该进程 CPU 亲和性后执行成功，未修改系统级设置。整理流程 3 项合成样例测试通过。

## 生成提示词

### 装备图集

Use case: stylized-concept. Asset type: original equipment UI icon atlas for a cozy Chinese xianxia desktop idle RPG. Square 1536x1536 canvas, EXACT regular three columns by three rows, nine equal square 512x512 cells. Genuine transparent RGBA outside objects. Row one left to right: elegant silver Chinese straight sword with jade hilt, dark jade cultivator crown with small bronze crest, folded teal-and-cream Chinese cultivator robe. Row two left to right: matched pair of bronze-and-jade wrist bracers, coiled brown silk belt with round jade buckle, matched pair of dark teal embroidered cloth boots. Row three left to right: gold ring with pale blue diamond-shaped gem, silver ring with violet round gem, jade pendant necklace on a short braided dark cord. All nine objects fully visible, each centered in its own cell, generous empty 60px margins, no cell overlap. Crisp original 16-bit pixel art, deliberate square pixel clusters, dark teal outlines, compact readable silhouettes at 64px. Consistent bronze, jade, cream highlights; no characters, no human mannequin, no panels, no text, no letters, no numbers, no logos, no watermark, no checkerboard, no background scene, no painterly rendering or blurred glow.

### 炼器图集初稿

Use case: stylized-concept. Asset type: original crafting materials and forge UI icon atlas for a cozy Chinese xianxia desktop idle RPG. Landscape 1536x1024 canvas, EXACT regular three columns by two rows, six equal square 512x512 cells. Genuine transparent RGBA background outside the objects. Row one left to right: ivory demon beast bone with small curved dark horn fragment; three stacked silver-blue spirit iron ingots with a subtle jade crystalline vein; three broken bronze magical artifact fragments engraved with abstract non-letter patterns. Row two left to right: two short spirit-wood logs tied with dark teal cord and one tiny green leaf; heavy dark steel anvil with bronze-handled forging hammer resting across it; compact Chinese bronze forging furnace with jade ornamental rim and orange fire visible through lower opening. Each object or grouped icon completely contained and centered within its own cell with 60px transparent margins. Original crisp 16-bit pixel art, deliberate square pixel clusters, dark teal outline, bronze jade cream palette, strong silhouettes readable at 48px, consistent lighting. No characters, no panels, no labels, no text, no letters, no numbers, no watermark, no checkerboard, no background scene, no external smoke, no blurry glow.

### 去光晕编辑（未通过：烘焙棋盘格）

Use case: background-extraction. Image 1 is the edit target: six crafting icons in an exact 3-column by 2-row grid. Remove ONLY all cloudy colored backdrop, diffuse glow, haze, and shadows outside the six isolated object groups. Make every pixel outside the silhouettes genuinely fully transparent (alpha zero), including all grid gutters and canvas edges. Preserve the bone and horn, ingots, bronze fragments, logs and leaf, anvil and hammer, furnace with contained flame exactly: same pixels/design, color palette, positions, sizes, six-cell ordering, 1536x1024 canvas. Make solid material interiors opaque; transparency is only outside objects and in real gaps. No black or colored fill backdrop, no checkerboard, no new objects, no text, no UI panels. Crisp clean pixel-art cutouts without edge halos.

### 第二次透明编辑（仍未通过；保留为程序整理源图）

The attached image is the edit target. Its gray-and-white checkerboard is incorrectly painted into the PNG: every background pixel is opaque. REMOVE that entire checkerboard pattern and return a real RGBA transparent-background cutout image. Do NOT draw a checkerboard or any replacement background. The actual alpha channel outside objects must be zero. Keep only the six crafting object groups in the exact same positions and sizes, exact same 1536x1024 dimensions and 3x2 grid: bone with horn, ingots, artifact fragments, logs with leaf, anvil with hammer, furnace. Preserve their dark outline, internal colors and solid opaque interiors. Empty space, between objects, around objects, and holes in objects must all be genuinely transparent. No added effects. This is image background removal, not an illustration of transparency.

## 重现

使用已安装 Pillow 与 NumPy 的 Python 运行：
```text
python Build/Art/clean_forge_atlas.py ArtSource/MortalRealm/DesktopPixelV2/Crafting/forge-atlas-source-v1.png <新的输出路径.png>
python -m unittest discover -s Build/Art -p "test_*.py" -v
```
脚本拒绝覆盖现存结果、源图或预览，不需要第三方图像处理服务。不上传本地背景合成预览或用户桌面截图。
