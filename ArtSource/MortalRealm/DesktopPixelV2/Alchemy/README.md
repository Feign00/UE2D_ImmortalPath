# 炼丹图集 v1

2026-09-12，内置 image_gen 生成，非 CLI。原创像素 UI 图集，不使用参考游戏素材。

源文件：alchemy-atlas-v1.png；1536×1024，3 列×2 行，单元 512×512。保留原始 alpha，无重采样、背景清理或裁切。每 8 像素抽样：透明 17172，部分透明 7404，不透明 0；实机透明合成效果另行验收。

从左到右、从上到下：回血丹、聚气丹、筑基丹、悟道丹、破境丹、丹炉。运行时按 ID 映射，丹药品质通过文字/颜色区分，不伪造不同品质图。

## 最终提示词

Use case: stylized-concept. Asset type: original pixel-art alchemy UI texture atlas for a cozy Chinese xianxia desktop idle game. Primary request: six isolated game objects on genuine transparent alpha in a perfectly regular 3-column by 2-row grid, landscape 1536x1024, six equal square 512x512 cells. Top row left: small red medicinal pill in a shallow pale jade dish with a red leaf; top middle: cyan spiritual pill surrounded by a compact curled cloud in a jade dish; top right: golden foundation pill in a lotus-shaped bronze dish. Bottom row left: violet enlightenment pill in a dark jade dish with a tiny star sparkle; bottom middle: orange breakthrough pill in a bronze dish with a small flame-shaped accent; bottom right: complete traditional Chinese bronze three-legged alchemy cauldron with jade handles and a small contained orange fire in the base. Each object centered in its own cell with at least 60px transparent margins, no overlap between cells; similar overall icon scale. Crisp original 16-bit pixel game art, deliberate square pixel clusters and dark teal outlines, restrained bronze and jade palette, strong silhouettes readable at 64px. No background scenery, no panels, no visible grid, no text, no Chinese characters, no letters, no numbers, no watermark, no logos, no checkerboard, no photorealism, no painterly blur. Actual transparent RGBA outside each object.
## 材料图集

material-atlas-v1.png：内置 image_gen 生成，1536×1024，保留原始 alpha。3×2 格依次为灵草、妖丹、灵液、矿石、仙果、灵石。每 8 像素抽样：透明 15286，部分透明 9290，不透明 0。五种材料接入炼丹消耗列表；灵石图格预留给炼器/商店，未宣称全局材料图标已替换。

Use case: stylized-concept. Asset type: original pixel-art materials inventory atlas for a cozy Chinese xianxia desktop idle game. Genuine transparent RGBA canvas, landscape 1536x1024, exactly 3 columns and 2 rows of equal square cells. Six centered isolated objects with generous 60px clear margins within each cell. Top left: tied bundle of three fresh emerald spirit herb leaves. Top middle: small dark bronze-rimmed red-purple beast core jewel. Top right: pale cyan ceramic flask filled with spiritual liquid, stopper and one water droplet accent. Bottom left: rough angular silver-blue ore cluster. Bottom middle: a golden immortal peach fruit with two green leaves. Bottom right: small cluster of three luminous cyan spirit-stone crystals. Original crisp 16-bit pixel art, bold dark teal outlines, distinct square pixel clusters, restrained jade/bronze colors, same scale and readable at 32px. No text, no characters, no numbers, no panel borders, no visible grid, no scenery, no watermark, no copied game assets, no checkerboard, no painted background, no photographic rendering, no blur or broad halos. Actual transparent alpha everywhere outside the six objects.
