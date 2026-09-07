# 第 41B 部分：动画素材质量检查

## 本批交付与边界

2026-09-07：完成只读图集检查工具和离线逐帧预览页。**没有完成玩家新版待机/移动动画的正式替换**，没有修改 C++、运行资产、导入设置或正式存档。飞升动画不变。

原计划接入待机与移动，但生成样稿未通过脚线与位置连续性检查，因此本批收敛为素材验收工具及失败记录。不得把工具测试通过写成游戏动画验收通过。

- `ArtSource/Tools/audit_sprite_atlas.mjs`：检查网格整除、透明通道、遮罩轮廓越界、底部漂移、重复帧；输出每帧原始及遮罩表现的 SHA256。隐藏 RGB 或透明度差异不会被误算为新动作。
- `ArtSource/Tools/preview_sprite_atlas.html`：浏览器本地打开，手动选择图集；支持播放/暂停、前后帧、滑条、帧率、叠影、脚线、缩放、深浅背景与洋红底。遮罩模拟仅用于显示，不修改或导出图片。
- 不自动居中、缩放单帧、修改 Pivot 或通过插入重复帧掩盖跳动。脚线为人工指定，轮廓最低点不等于解剖学脚底；飞行/死亡等动作应单独设置容差并人工复核。
- 这是独立开发检查页，不是新增游戏功能页，不会暂停挂机。

## 运行方式

运行环境需要 Node.js，图像读取需要 `sharp`；浏览器测试另需 `playwright` 和其浏览器，或通过 `ATLAS_BROWSER_PATH` 指定已有浏览器。模块可由现有开发依赖提供（`NODE_PATH`），没有在游戏中增加 Node 依赖。

```powershell
node ArtSource/Tools/audit_sprite_atlas.mjs <图集.png> 4 2 --strict --threshold 128 --bottom-tolerance 2
node --test --test-concurrency=1 ArtSource/Tools/audit_sprite_atlas.test.mjs ArtSource/Tools/preview_sprite_atlas.test.mjs
```

普通模式只报告；`--strict` 遇到警告返回 2，无警告返回 0，输入/解码错误非零。自动检查无警告仍不代表姿态正确或动作流畅。128 是检查默认值，不宣称等于任何 UE 材质配置；本批另外以 85 检查，两个阈值均拒绝样稿。

## 样稿审核结果

本批使用内置图像生成工具（非 CLI/API 回退），保留输出 alpha，没有程序抠图或重绘。原始生成图及透明提取修订均为审核样稿，未纳入运行引用，也未作为成品上传 GitHub。

| 样稿 | 尺寸/帧数 | 检查结果（阈值 128） | 结论 |
|---|---|---|---|
| 待机初稿 | 1536×1024 / 4×2 | 所有像素不透明，棋盘格被画入背景 | 拒绝直接导入 |
| 待机透明提取 | 1536×1024 / 4×2 | 各帧存在透明区域，但无 alpha=255 像素；轮廓底部差 26 px | 仍未合格 |
| 移动初稿 | 1536×1024 / 4×2 | 所有像素不透明，棋盘格被画入背景 | 拒绝直接导入 |
| 移动透明提取 | 1536×1024 / 4×2 | 各帧存在透明区域，但无 alpha=255 像素；轮廓底部差 31 px | 仍未合格 |

两张提取图的 alpha 大量接近 253；不能仅凭缺少 255 认定人物完全透明，但也不能当作已规范导出的硬边透明资产。遮罩预览能去除多数低透明边缘，不能修复姿态和位置问题。移动第 1/5 帧对照显示第 5 帧整体离开脚线，且列间还有水平位置差异；左右脚完整周期仍需重制后验证。

审核样稿标识（保留本地，不是可移植运行路径）：

- 待机提取 `exec-4fbe0e34-3566-458a-93cb-24bd5253d1a9.png`；SHA256 `C9C46AF24F9C475918ABB154E04C69255A2B04AAE03AE0F204FB592071D8DB9B`。
- 移动提取 `exec-9d1f2b1f-5ab1-44ba-8db7-cc415fca39fa.png`；SHA256 `C59821A75B455FB6F952FFA6F6F66A8D770BCBB6AB2E67BFC4811190E06E5F49`。

## 生成记录

角色参考是前批原创白衣青边、黑发发髻、佩玉、持剑的侧身修士锚点，不复制参考游戏角色。待机/移动初稿要求同角色、4×2 等分图集、脚线 464、右朝向、真实透明背景；实际输出没有满足约束。

本次最后一次透明修订的完整提示词（输入是移动初稿，编辑目标，不是风格参考）：

> Use case: background-extraction. Image 1 is the edit target: an eight-frame sprite atlas. Remove the painted gray-and-white checkerboard completely, including gaps between limbs and sword. Preserve all eight characters exactly, their positions, colors and 1536x1024 dimensions. Deliver a cutout RGBA PNG with a genuinely transparent background (alpha zero), solid opaque character interiors, clean sharp edges, no halo. Do not replace the background with another color or pattern. No pose changes or new drawings.

## 验证记录

- 6 项图集单元测试通过，覆盖非法参数、只读性、透明统计、越界/重复、脚线漂移、遮罩等价和阈值。
- 1 项真实无头浏览器交互测试通过，覆盖本地加载、首尾步进、播放/暂停、遮罩切换恢复原始 alpha、错误网格、坏图片恢复与零 HTTP 请求；总计 7 成功、0 失败、0 跳过。
- 首次浏览器测试因 Playwright 配套浏览器未安装失败；改用本机 Edge。随后一次 Node 原生 V8 异常退出；按顺序重跑完整测试通过，没有删除测试或修改游戏来规避。
- 两份实际提取样稿分别以阈值 85/128 运行严格检查，四次均返回 2，确认拒绝逻辑有效；底部差分别为 25/26 和 31/31 px。
- 已查看 `Saved/ArtReview41B/move-frame1.png` 与 `move-frame5.png`，确认离线预览页排版及脚线差异。截图保留在忽略目录，不上传。
- 本批没有 C++ 或运行资源改动，未重新编译 UE、未重复声称上批 45 项 UE 测试是本批运行结果。
- 正式存档 SHA256 保持 `98DE75CD856463348A4547C4D7F5D202613A0EA984CB41CA675FB6F1FEF61579`。

## 后续

继续修正玩家待机/移动源素材，先统一比例、单元格位置和脚线，再完成攻击/受击/死亡。整套风格一致且通过独立实机验证后再替换默认演员；不能混用新像素待机与旧半写实攻击而声称重制完成。全演员动画、分地图植被和其余人界 UI 美术仍未完成。
