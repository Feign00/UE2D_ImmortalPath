# 人界建筑入口 v1

日期：2026-09-08。使用内置 image_gen 工具生成原创像素风建筑插画，不使用参考游戏美术。

## 交付资产

- `hub-buildings-v1.png`：1536×1024，3 列 × 2 行，每格 512×512。顺序为修炼、宗门、炼丹、炼器、洞府营造、灵田。
- `market-building-v1.png`：1536×1024，独立百宝阁建筑插画。
- 导入清单：`Config/ImportDesktopManagementBuildings.json`。保留无损图像压缩；洞府图集通过 UMG UV 区域逐格展示，原图不裁切、不改色。

## 质量与边界

最终使用的两张原始输出均为 32-bit RGBA，保留生成的透明通道，没有程序抠图或颜色修改。初次检查误把预览中的暗色效果当成不透明背景；随后对文件格式及 alpha 像素复核，并在游戏内确认可透明叠加。曾额外请求一次洞府背景编辑，但该编辑将棋盘格画入 24-bit RGB 图片，已排除，不进入项目。无需继续此前提出的程序抠图确认。

本批仅替换人界主页七个入口，不是全部功能页 UI 美术重制。旧资产和独立飞升动画保持不变。该图集为静态建筑素材，不是动画，也不宣称严格低分辨率逐像素手工绘制。

## 原始生成提示词（内置工具）

### 洞府图集

Use case: stylized-concept. Asset type: a single production game sprite atlas, not a mockup. Create a cohesive original Chinese xianxia mortal-realm building sprite sheet for a cozy desktop idle game. Landscape 1536x1024, exactly 3 columns by 2 rows of equal 512x512 cells, no visible grid lines. Each cell contains one small readable architectural diorama centered within its own cell with generous 40px clear margins. Transparent background with genuine alpha, no checkerboard painted into the image. Consistent front three-quarter low-angle pixel-art view, crisp deliberate pixel clusters like detailed 16-bit RPG environment art, dark ink outlines, jade-green curved tile roofs, warm timber, ivory walls, muted gold accents, soft warm lantern light. Top row left: quiet meditation pavilion with jade lotus cushion and bamboo. Top row middle: dignified sect hall with paired stone lanterns and broad steps. Top row right: open alchemy workshop with a prominent round bronze cauldron and potion jars. Bottom row left: open blacksmith workshop with a glowing orange forge and clearly visible anvil. Bottom row middle: cultivator residence built into a mossy rock cave with a circular doorway and small garden. Bottom row right: terraced herb garden with green medicinal plants, irrigation channel and small wooden seed shed. Every building full silhouette visible, centered, no overlapping cells, same apparent scale, compact soil or stone footing entirely within its cell. No people, no animals, no text, no letters, no labels, no numbers, no UI frames, no buttons, no mist outside silhouettes, no bloom, no photorealism, no watermarks. Readable at roughly 170x170 pixels per building.

### 百宝阁

Use case: stylized-concept. Asset type: original production game menu building illustration for a Chinese xianxia desktop idle game. Draw one complete mortal-realm treasure emporium, a welcoming broad Chinese two-storey timber shop with elegant jade-green tiled roof, ivory plaster, muted gold roof ornaments, warm hanging lanterns. Open ground-floor shop front with clearly readable silhouette of an equipment chest, sword display and shelves of small jade bottles. A small paved stone entrance and two compact decorative shrubs. No people, no animals, no writing or symbols on signs. Detailed pixel-art illustration with crisp dark outlines and deliberate textured pixel clusters, cozy 16-bit RPG environment feeling, not vector and not photorealistic. Low front three-quarter view, consistent architectural perspective. Entire building visible, centered in landscape 1536x1024 canvas with wide empty margins. Silhouette occupies central 80 percent width and 85 percent height. Genuine transparent alpha background, no checkerboard drawing, no gradient, no large landscape, no UI frame, no border, no text, no watermark. A reusable isolated building sprite, with all supporting props attached to the immediate stone footing. Readable when reduced to 250 pixels tall.

## 未采用的透明编辑

输入为洞府原图，要求只去除背景、保留六栋建筑与排列、输出真实 alpha。工具返回 24-bit RGB 的棋盘背景图；经文件格式与像素检查确认不是透明，已拒绝作为运行资产。
