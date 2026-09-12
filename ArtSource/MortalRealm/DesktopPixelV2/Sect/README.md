# 四方宗门徽记 v1

2026-09-10，使用内置 image_gen 生成，非 CLI。原创修仙像素插画，没有提取参考游戏素材。首次请求网络失败；重试后生成图保存在本机，经检查后复制入项目。

源文件 `sect-emblems-v1.png`，实际 1254×1254，2×2 排列，每格 627×627。上排为青云宗、天剑宗，下排为万妖谷、魔宗。提示要求 1024×1024，但保留实际输出，不拉伸、不二次抠图。PNG 每隔 4 像素抽样：43,359 点完全透明，55,166 点部分透明，71 点完全不透明。保留原始 alpha；不是二值透明演员图，不用于桌面色键特效。

导入 `Config/ImportDesktopSectArt.json`，资源 `/Game/GAME/Asset/DesktopPixelV2/UI/T_SectAtlas`，无损 RGBA、无 mip、常驻 UMG。代码按宗门 ID 映射取图；未知 ID 或图片缺失不显示错宗徽记，名称仍可读。任务/宝库小图标复用现有代码像素符号，并非此图集内容。

## 完整提示词

Use case: stylized-concept. Asset type: original pixel-art sect emblem texture atlas for a cozy Chinese xianxia idle desktop game. Square 1024x1024 canvas, two columns and two rows, four equal 512x512 cells on genuine transparent alpha. One complete emblem centered within each cell, generous empty margins, no visible grid. All four share the same round carved bronze-and-jade medallion frame and consistent scale. Top left: serene cyan cloud wrapped around a jade mountain peak, for a peaceful cultivation sect. Top right: upright elegant silver Chinese straight sword with pale blue wings of sword energy, for a sword sect. Bottom left: emerald beast mask with small horns and warm golden eyes, for a beast-taming valley. Bottom right: purple thunderbolt within a dark violet flame, for a lightning demonic sect. Clean original 16-bit game pixel art, deliberate square pixel clusters, dark teal outlines, restrained bronze highlights, crisp strong silhouettes readable at 64px; no painterly blur, no photorealism, no gradients or bloom. Only four medallions, isolated, no background environment. No text, letters, numbers, brand logos, watermark, or checkerboard. The outside of every emblem must be actual transparent RGBA.
