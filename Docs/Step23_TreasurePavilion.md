# 第 23 步：百宝阁交易系统

本步骤把现有灵石、装备、材料、丹药与法宝接入统一商店循环。玩家可以在不打断 TBH 横条窗口后台战斗的情况下购买每日商品、付费换货，并把背包装备或材料出售为灵石。

## 一、百宝阁入口与界面

- 按 `B` 或点击 HUD 的“百宝 [B]”打开百宝阁，再次按 `B` 或点击关闭按钮退出。
- 百宝阁使用 900×600 原生 C++ UI，并与背包、炼丹、炼器、法宝、功法和问道台互斥；打开界面不会暂停自动战斗。
- 左栏显示当日商品与售罄状态，中栏显示商品属性、整组数量、价格、购买和刷新操作，右栏显示可出售的背包装备与材料。
- 顶部实时显示当前灵石和距离下次免费刷新的倒计时；商品、背包、材料或灵石发生变化时界面自动刷新。
- 界面保持与 Taskbar Hero（TBH）运行方式一致：独立游戏仍使用桌面底部横条窗口，900×600 面板由现有视口缩放和居中规则显示。

## 二、商品与库存

百宝阁使用持久化的 `FImmortalShopState` 保存整批商品快照。商品生成时即确定物品 ID、数量、价格和装备属性，购买或重启不会重新随机同一件商品。

完整解锁时每批货物包含：

| 类型 | 数量 | 内容与规则 |
| --- | ---: | --- |
| 装备 | 3 | 按青云山关卡生成等级和最低品质，并保留完整词条、强化状态与流派归属 |
| 材料 | 4 | 从当前关卡已解锁材料中抽取，按整组数量出售 |
| 丹药 | 2 | 从当前境界已解锁丹方中抽取，普通丹按 2 枚成组，极品丹按 1 枚出售 |
| 法宝 | 1 | 从当前关卡已解锁法宝中抽取 |

低关卡或低境界尚未解锁对应内容时，实际商品数可能少于 10 件。每个商品拥有独立 `ListingId`；购买成功后标记“已售罄”，不会从列表中消失，也不能在同一批库存中重复购买。

## 三、每日刷新与付费刷新

- 免费刷新按中国标准时间（CST，UTC+8）的自然日计算，默认在每日 `00:00` 换货。
- 游戏运行中每 60 秒检查一次跨日状态；打开百宝阁时也会补做检查。
- 首次进入、旧档升级或进入新自然日时生成免费库存，并把当日手动刷新次数归零。
- 同一天内重启不会再次免费刷新；检测到系统时间倒退时保留原库存，不会利用改时钟重复领取免费刷新。
- 手动刷新第一次消耗 100 灵石，同一天内依次为 200、300……灵石；当日次数与新库存立即写入存档。
- 刷新先生成完整候选库存，确认有效后才扣款；写盘失败时恢复原灵石和原库存。

## 四、购买规则

所有购买都先检查商品有效性、售罄状态、灵石和接收容量，再一次性完成“加入物品、扣除灵石、标记售罄、保存”。

- 装备购买要求 30 格装备背包至少有 1 个空位。装备先安全进入背包，若符合当前流派且强于同部位装备，再走自动换装；换下的旧装备返回背包。
- 材料购买写入独立材料堆叠，并检查该材料最大堆叠上限。
- 丹药购买写入普通/极品独立丹药堆叠，并检查数量上限。
- 法宝购买生成独立法宝实例并写入法宝库存，不占普通装备背包。
- 购买商品不计入怪物装备掉落计数，也不会伪造拾取奖励。
- 任一物品写入步骤失败时不扣款；最终存档写入失败时恢复交易前的库存、穿戴、生命/灵力、灵石、售罄状态与各类 UI 修订号。

## 五、出售规则与定价

- 右栏只列出背包中的装备和材料；已穿戴装备不会进入出售列表。
- 装备按单件出售，价格由装备等级、品质与强化等级计算，回收价为对应商店估值的 25%。
- 材料可出售 1 个或一次出售当前整组，回收单价低于商店购入单价，不能通过反复买卖套利。
- 灵石增加使用饱和计算，避免接近整数上限时溢出。
- 出售同样采用事务写盘；保存失败时恢复被移除的装备/材料与原灵石数量。

## 六、存档与兼容

SaveGame 已升级到 v11，新增以下持久化内容：

- 当前商店日期键 `RefreshDayKey`；
- 当日库存序号 `RefreshSerial`；
- 当日手动刷新次数 `ManualRefreshCount`；
- 每个商品的类型、稳定 ID、数量、价格、丹药品质、完整装备快照与售罄状态。

v1–v10 存档仍可读取。旧档首次升级时按保存的青云山关卡、境界与当天 CST 日期生成首批库存，再统一写回 v11；原有装备、材料、丹药、法宝、灵根和流派迁移逻辑保持不变。加载时会归一化非法数量、价格和刷新计数，并移除无效或重复商品。

## 七、测试与运行时验证

自动化测试入口：

- `ImmortalPath.Shop.GenerationRefreshPricingAndInventory`
- 覆盖 CST 午夜边界、同日/跨日/时钟倒退、四类库存数量、商品 ID 唯一性、付费刷新序号、装备与材料定价、无效/重复库存归一化，以及材料不足时不部分扣除。

建议构建与自动化命令：

```powershell
& 'E:\UE_5.7\Engine\Build\BatchFiles\Build.bat' ImmortalPathEditor Win64 Development 'D:\UE2D\ImmortalPath\ImmortalPath.uproject' -WaitMutex -DisableUnity
& 'E:\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE2D\ImmortalPath\ImmortalPath.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests ImmortalPath.;Quit' -TestExit='Automation Test Queue Empty' -log
& 'E:\UE_5.7\Engine\Build\BatchFiles\Build.bat' ImmortalPath Win64 Development 'D:\UE2D\ImmortalPath\ImmortalPath.uproject' -WaitMutex -DisableUnity
```

非 Shipping 构建提供以下验证参数：

| 参数 | 作用 |
| --- | --- |
| `-ImmortalTestGrantShopResources` | 临时加入灵石、材料和背包装备，并生成四类商品齐全的高进度库存 |
| `-ImmortalTestRunShopTransactions` | 依次购买四类商品、手动刷新、刷新后购买，并出售一件装备和一个材料 |
| `-ImmortalTestFillShopBackpack` | 填满装备背包并审计购买失败时灵石、物品数量与售罄状态均不变 |
| `-ImmortalTestLogShop` | 输出日期、刷新序号、四类数量、售罄数、灵石与各库存数量，供重启审计 |
| `-ImmortalTestOpenShop` | 自动打开百宝阁 |
| `-ImmortalTestScreenshotShop` | 延迟截取 `Saved/Screenshots/ShopTest.png` |
| `-ImmortalTestExitAfterScreenshot` | 截图后自动退出独立运行实例 |

一次完整的临时存档交易与界面验证可使用：

```powershell
& 'E:\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe' 'D:\UE2D\ImmortalPath\ImmortalPath.uproject' -game -windowed -ResX=1920 -ResY=320 -log -ImmortalTestGrantShopResources -ImmortalTestRunShopTransactions -ImmortalTestLogShop -ImmortalTestOpenShop -ImmortalTestScreenshotShop -ImmortalTestExitAfterScreenshot
```

随后用 `-ImmortalTestLogShop` 再次启动，可核对 v11 库存、售罄状态与灵石是否保持。正式验证必须先备份用户原存档，使用临时测试存档完成后再按 SHA-256 恢复。

最终验证结果：

- `ImmortalPathEditor` 与 `ImmortalPath` 的 Win64 Development `-DisableUnity` 构建均为 `Succeeded`；
- 全项目找到 8 项 `ImmortalPath.*` 自动化测试，8/8 全部为 `Success`，百宝阁测试单独通过；
- 临时存档实机依次购买装备、材料、丹药和法宝，四笔均成功；随后完成首次 100 灵石付费刷新、刷新后购买、出售一件背包装备和出售一个材料；
- 重启后审计保持日期 `20260718`、刷新序号 `1`、手动刷新次数 `1`、商品分类 `3/4/2/1` 和售罄数 `1`，SaveGame 版本为 v11；
- 使用现有旧版用户存档做迁移演练，按保存的青云山第 22 关生成 `3/4/2/1` 共 10 件商品并成功写为 v11，原有金丹九层、装备、材料、法宝、功法与灵石均正常载入；
- 填满 30 格装备背包后再次购买装备，结果为失败，灵石不变、背包数量不变且商品仍可购买，验证失败交易零修改；
- TBH 实机截图为 1707×320，百宝阁三栏、HUD“百宝 [B]”、玩家与来怪同时可见，文件位于 `Saved/Screenshots/ShopTest.png`；
- 测试前用户存档 SHA-256 为 `E3833E79F60F31F93EFCAD69640DE8FC3010971BF23914AE9F29DFD6CF01B86A`，测试结束恢复后哈希完全一致。

## 八、素材需求

当前功能不依赖新增素材，现有面板纹理、文字和颜色占位可以完整操作，不需要等待素材。

后续可选的非阻塞美术素材：

- 百宝阁 900×600 面板背景；
- 装备、材料、丹药、法宝四类商品图标，建议 128×128、透明背景、统一仙侠风格；
- “售罄”印章，建议透明背景并保留高对比度。

下一步进入第 24 步“多地图系统”，扩展青云山之外的历练区域、地图解锁条件、关卡范围与掉落表。
