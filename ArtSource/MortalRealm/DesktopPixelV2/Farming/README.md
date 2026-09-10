# 灵田静态状态图集 v1

2026-09-09，使用内置 image_gen 生成，非 CLI。原创修仙像素风；不含参考游戏素材。原始 PNG 直接复制入项目，保留生成的 RGBA，不程序抠图、不改色、不逐格拉伸。

运行素材：`farming-atlas-v1.png`，1536×1024，3 列×2 行，每格 512×512。上排：空田、幼苗、灵草；下排：仙果、灵木、未解锁。此为静态状态插画，不是生长动画。Growing/Ripening/Mature 共用该作物插画，通过文字、进度与操作区区别状态。

每隔 4 像素抽样 98,304 点：61,828 点 alpha=0、36,476 点为部分透明；不是把黑底预览当成不透明图片。保留柔和透明边缘，因此不宣称严格二值像素图。无生成文字/棋盘格。通过 UMG UV 读取原图各格，缺失图片回退文字。

导入清单：`Config/ImportDesktopFarmingArt.json`。资源：`/Game/GAME/Asset/DesktopPixelV2/UI/T_FarmingAtlas`。无损 RGBA、无 mip、常驻；软引用计入资源依赖。

## 完整生成提示词

Use case: stylized-concept. Asset type: one production-ready pixel-art texture atlas for a Chinese xianxia idle desktop game's farming UI. Generate a landscape 3 columns by 2 rows atlas on genuine transparent alpha, ideally 1536x1024. Six equally sized square cells, no drawn grid or labels. Within every cell centered with generous clear padding, identical small diamond soil bed and consistent 3/4 top-down camera. Row 1 left to right: empty dark-brown tilled soil bed with 3 furrows; soil bed with a tiny bright jade-green seedling; lush mature medicinal spirit grass with a few pale turquoise glowing buds. Row 2 left to right: small mature red-orange spirit-fruit shrub with a few round gold/red fruits on the soil bed; miniature mature spirit-wood tree with compact jade foliage and clearly visible brown trunk on the same soil bed; uncultivated stony soil bed with a weathered little wooden barrier sign and a bronze padlock (no characters). Style: original cozy 16-bit pixel art with crisp square pixel clusters, dark teal outlines, jade and moss green, warm ochre earth, restrained gold highlights, no smooth vector shapes, no painterly blur, no gradients or bloom. Each object is completely contained in its cell with empty transparent margins, not touching other cells. Six objects only. No background landscape, no UI frames, no characters, no text, no logos, no watermark. Actual transparent RGBA background, NOT a checkerboard painted into the artwork. Designed for readable 100-180px in-game icons.
