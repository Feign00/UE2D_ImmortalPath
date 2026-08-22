# 第 34 步：Windows Shipping / Steam 发布包优化

更新时间：2026-08-06

## 本步结果

已使用 UE 5.7 重新完成 `Win64 Shipping` 编译、Cook、IoStore、压缩、归档和运行冒烟测试。当前可分发目录：

```text
D:/UE2D/ImmortalPath/Releases/Windows/ImmortalPath-0.1.0
```

入口程序：

```text
ImmortalPath.exe
```

打包脚本：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File D:\UE2D\ImmortalPath\Build\Windows\PackageSteam.ps1 `
  -EngineRoot E:\UE_5.7 `
  -Version 0.1.0
```

AutomationTool 最终结果为 `BUILD SUCCESSFUL`、`ExitCode=0`。完整日志保存于：

```text
Saved/Logs/Step36_Package_WindowsShipping.log
```

## 发布包审计

- 文件数：25。
- 总大小：259,770,143 bytes，约 247.74 MiB。
- 使用 Shipping 可执行文件、压缩 Pak 和 IoStore 容器。
- 飞升资源已进入 Cook 清单：1 个 Texture、1 个 Flipbook、17 个 Sprite，共 19 个资产。
- `DirectML.dll`：0。
- `onnxruntime.dll`：0。
- `.pdb`：0。
- 松散 `.png`：0。
- 松散 `.uasset`：0。
- `steam_appid.txt`：0；开发 App ID 文件不会被带入公开发布包。

关键文件 SHA-256：

| 文件 | SHA-256 |
|---|---|
| `ImmortalPath.exe` | `DC46EAF19F2A8ACA07B2AF80217E0D6428E753F96A43B62A01989C4ADB2D3896` |
| `ImmortalPath/Binaries/Win64/ImmortalPath-Win64-Shipping.exe` | `E088C314B5FE044C5A215ACF5012005411E188923F3B6B251AF9259377FF4E88` |
| `ImmortalPath/Content/Paks/ImmortalPath-Windows.pak` | `AEA3ACCD31AAD0DDBC7DDBD5BE9256065A71E73261C7048754D2CE1FFAA98B77` |
| `ImmortalPath/Content/Paks/ImmortalPath-Windows.ucas` | `BDE84E0B9C842648C1B7E9C919FC6153E02C9168B70F171872ADD17DB57CE6A5` |
| `ImmortalPath/Content/Paks/ImmortalPath-Windows.utoc` | `8412FF82F3240BD328640ED84745D8A9CF044942CA1BB6C2BBEA5AE48E79CDC8` |

## Shipping 冒烟测试

从归档目录直接启动真正的 Shipping 可执行文件，使用独立 `-UserDir`，保持运行 12 秒后发送正常窗口关闭请求。

验证结果：

- 进程启动后持续运行，没有启动即崩溃。
- `CloseMainWindow` 成功，进程正常退出。
- 独立目录生成 Windows 配置和新存档，证明游戏初始化、存档路径与首轮启动均已执行。
- 原开发存档未被 Shipping 测试修改，SHA-256 仍为 `E3833E79F60F31F93EFCAD69640DE8FC3010971BF23914AE9F29DFD6CF01B86A`。

## Steam 上架前仍需外部信息

当前目录已经适合提交 SteamPipe 的 Windows depot，但正式上架仍需在 Steamworks 后台取得真实 App ID、Depot ID，并配置商店页、成就、云存档和上传脚本。这些账号侧数据不应硬编码进公开仓库或测试包。
