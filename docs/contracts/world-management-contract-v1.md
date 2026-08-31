# 世界管理合同 v1

> Architecture Lab 迁移说明（2026-08-31）：本合同冻结历史工程证据；文中 R3、真人和延期
> 描述保留当时语境，当前退出模型见 `docs/current/ai-assisted-gameplay-acceptance-v1.md`。

本文固定 K4 的世界管理、恢复和应用状态边界。它建立在
`world-catalogue-contract-v1.md`、`storage-transaction-contract-v1.md` 和
`world-backup-contract-v1.md` 之上，不重新定义存档格式。

## 所有权边界

- `WorldManagementService` 是创建、打开前检查、重命名、可恢复删除、恢复、永久清理
  和备份恢复的唯一命令入口。
- Ogre/ImGui 只提交命令和展示结构化结果，不拼接或移动世界目录。
- `WorldCatalogue` 继续保持只读；元数据发布使用 `WorldSave`/`StorageTransaction`，
  备份恢复委托给 `WorldBackup`。
- 正常世界目录位于 catalogue root；删除后的世界位于其同级
  `<catalogue>.recovery`。默认最多保留 3 个可恢复世界，按删除时间从旧到新清理。

## 身份与命令语义

- 创建为每个世界生成不可变的规范 world id，并使用该 id 作为目录名。同名显示名合法，
  但不会共享 id 或目录。
- 重命名只修改 `world_name`；world id 和目录保持不变。版本 1/2 世界在第一次写命令时
  升级为当前元数据版本，但目录不迁移。
- 打开前重新枚举目录、校验 id，并事务更新最近游玩时间。UI 不能绕过该步骤直开路径。
- 删除先把完整世界目录移动到恢复区，再发布带原目录名和删除时间的 `recovery.meta`。
  清单写入失败时必须尝试把世界移回原位。
- 恢复要求活动目录中不存在相同 world id，且原目录名仍可用。永久清理只能作用于恢复区。
- 备份列表和恢复通过现有 K3 清单与回滚合同执行；中断恢复不得替换最后一个有效活动世界。

所有命令返回 `Success`、`InvalidArgument`、`NotFound`、`Conflict`、
`CatalogueInvalid` 或 `StorageFailure`，并携带可展示信息。调用方不得从异常文本反推状态。

## 应用状态

普通启动采用以下状态路径：

`MainMenu -> WorldList -> Loading -> Playing -> Paused`

- 世界列表操作和暂停菜单捕获键盘、鼠标，不能向世界泄漏移动、破坏或放置动作。
- `Playing` 按 Escape 进入 `Paused`；暂停期间不推进固定 tick、世界时间或玩家输入。
- 从暂停菜单返回主菜单时先保存并释放活动世界，再清除渲染对象和玩家上下文。
- 自动化可通过 `HELLOMINE3D_SAVE_DIR` 或 `HELLOMINE3D_SKIP_MAIN_MENU=1`
  沿用直进世界路径；这不改变普通玩家启动语义。
- `HELLOMINE3D_CATALOGUE_DIR` 仅用于隔离测试 catalogue root。

## 验证合同

`HelloMine3DWorldCatalogueSmoke` 在原 K1 覆盖上增加 K4 断言，至少覆盖：

- 缺失/空目录不被只读查询创建；损坏目录返回结构化失败；
- 非法名称和路径逃逸被拒绝；同名世界拥有不同的稳定 id/目录；
- 重命名、最近游玩更新和旧版升级不改变 id/目录；
- 可恢复删除、恢复、永久清理和恢复数量上限；
- 备份列表、成功恢复和发布中断后的活动元数据回滚；
- 主菜单、世界列表、加载、游玩、暂停以及非法状态迁移。

每个配置还要通过主程序编译、隐藏后台主菜单启动和隐藏后台直进世界启动。正式真人键鼠、
焦点恢复与完整交互记录继续归入后置的 R3/阶段 8 综合验收，不阻塞 K4 实现关闭。
