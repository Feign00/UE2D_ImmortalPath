# 第 41C 部分：玩家新版待机/移动整理与实机预览

## 本批结果

2026-09-07：在用户明确授权程序整理后，保留原图、按人工锚点修正两套图集，导入独立像素角色目录，并通过 UE 独立运行验证。

- 两套动作各 8 帧，8 FPS；没有增加重复帧或用静态图位移冒充动作。
- 待机原有约 26 px、移动 31 px 的图集排版脚线差均修正为 0 px；全部帧无半透明像素、无越界、无像素重复告警。
- 待机使用整套 0.92 等比缩放匹配移动原图比例，移动整套不缩放；不是每帧独立拉伸。目标 Pivot [176,464]，像素密度 2.56。
- 新建 2 张纹理、16 个 Sprite、2 个 Flipbook，全部使用 Masked 材质。旧人物、怪物、灵宠、飞升与默认运行引用未覆盖。
- 独立预览在固定位置显示待机和移动，并互换动作检查脚线与尺度衔接。**不是正式战斗角色的全套替换，也不是全演员动画最终验收。** 移动抬腿幅度较明显，步态风格与整套攻击动作完成后仍需统一复核。

## 文件

- 源图与整理图：`ArtSource/MortalRealm/DesktopPixelV2/Player/`，完整生成记录和处理方式见该目录 README。
- 可复现处理器：`ArtSource/Tools/align_sprite_atlas.mjs`；输入源图校验值与人工锚点，拒绝裁切和覆盖已有输出。
- 导入清单：`Config/ImportDesktopPixelPlayerPreview.json`。
- UE 资产：`Content/GAME/Asset/Player/desktop_pixel_v2/`。
- 独立预览入口：`Source/ImmortalPath/Characters/ImmortalPixelAnimationPreview.cpp`。只有显式开发参数启用；Shipping 构建为空实现。玩家类只增加一次入口调用，没有改写常规战斗/动画选择。

## 独立测试安全边界

预览必须指定 `Saved/Automation/PixelPreview` 下的独立 `-UserDir`，否则拒绝启用。预览隐藏旧玩家显示、暂停测试角色攻击/移动与刷怪，创建无碰撞的预览组件，4.5 秒后退出。暂停仅属于这个显式开发测试，不影响普通游戏和管理页挂机。

示例参数（启动 UnrealEditor-Cmd.exe 并指定本项目）：

```text
-game -d3d11 -windowed -NoSound -ImmortalTestPixelPlayerPreview
-UserDir=D:/UE2D/ImmortalPath/Saved/Automation/PixelPreview/Run1
```

导入时将整个参数加引号，例如 `'-ImportSettings=Config/ImportDesktopPixelPlayerPreview.json'`。本批第一次导入因 PowerShell 拆分扩展名而失败，修正调用引号后成功，未修改导入器来绕过校验。

## 测试结果

- Editor Win64 Development 构建通过。
- Game Win64 Development 首次遇到构建工具内部 .NET AccessViolation；同样源码重试通过。未修改引擎、删除测试或掩盖失败。
- 完整 `ImmortalPath.` 自动化：46 成功、0 警告、0 失败、0 未运行，报告 `Saved/TestReports/Pixel41C/index.json`。新增测试检查新资产帧数、FPS、单元格、图集局部 Pivot、像素密度与 Masked 材质；旧五动作和原 17 帧飞升测试继续通过。
- 素材工具：9 项测试通过，包括新处理器的只读输入、人工对齐、硬透明、确定性输出、非法配置及裁切拒绝。
- 在 `Saved/ArtReview41C` 从源图重新生成两套图集，PNG SHA256 分别与入库版本完全一致，验证端到端可复现。
- `Saved/Logs/Pixel41CPreview.log`：12 次采样记录两套动画帧号与时间持续前进并循环；2 秒时在固定 Transform 互换 Idle/Move。已检查 `PixelPreview_04.png`、`PixelPreview_08.png`，人物脚底贴合地面、固定位置切换无原先整排抬升现象。
- 上述截图是合成前的 UE 视口，洋红为透明保留色，不能把它当成桌面最终效果。
- `Saved/Logs/Pixel41CDesktop.log`：组合透明测试实际读取原生桌面，`sourceKey=true nativeKey=true desktopVisible=true foregroundVisible=true`；退出日志为 `window hidden before color-key teardown; visible=false`，进程正常退出。该自动检查是透明合成/前景采样验证，不宣称逐像素验证所有演员边缘。
- 两次独立运行都使用不同隔离目录；正式主存档 SHA256 前后仍为 `98DE75CD856463348A4547C4D7F5D202613A0EA984CB41CA675FB6F1FEF61579`。
- 另以 `Pixel41CDefaultSmoke` 隔离目录、不带像素预览参数启动常规游戏：旧五套动作正常加载，刷怪继续，未出现像素预览日志；透明合成四项为 true，正常退出。确认新入口不会自动启用预览。

## 下一批

制作同一角色的攻击、受击、死亡，校准攻击接触帧与伤害时机。五套动作风格与状态切换整体验收后，才替换默认玩家。其后继续怪物、灵宠、Boss 和人界局部树草；本批不扩展新玩法。
