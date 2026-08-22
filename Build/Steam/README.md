# Steam 交付说明

当前 Windows Shipping 包可作为 Steam depot 的基础内容，但项目尚未绑定真实
Steamworks App ID，也没有启用登录、成就、云存档或其他在线接口。

获得 Steamworks 应用后：

1. 将 Steamworks SDK 的 redistributable 文件按 Valve 后台要求放入 depot。
2. 在本地开发机复制 `steam_appid.example.txt` 为 `steam_appid.txt`，将内容改为真实
   App ID；不要把 `steam_appid.txt` 放入公开发行 depot。
3. 在 Steamworks 后台配置启动项为 `ImmortalPath.exe`。
4. 用 SteamPipe 上传
   `Releases/Windows/ImmortalPath-<版本>` 中的完整目录。
5. 从 Steam 客户端安装并复验窗口位置、存档、退出和覆盖升级。

离线单机版本不依赖 Steam 客户端即可运行。真实 App ID 与 Steamworks 权限由开发者
账号提供，因此本项目没有使用测试 App ID 冒充正式接入。
