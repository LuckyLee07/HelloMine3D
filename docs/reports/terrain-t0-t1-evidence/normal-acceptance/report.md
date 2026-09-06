# T0/T1 独立正常窗口验收 — 部分完成

日期 2026-09-06。总结果 **BLOCKED**：菜单创建、保存重开与正常退出已通过；受公开 CUA 连续输入能力限制，未完成可复核的坡面往返、资源采得及结构进入。不能据此关闭完整正常玩法验收。

| 子项 | T1 v5 | T0 v4 | 证据与范围 |
| --- | --- | --- | --- |
| 正常菜单创建 seed20260807 世界 | PASS | PASS | CUA 连续菜单操作；T0-02 原图、只读存档摘要 |
| 静止地面支撑、可见场景 | PASS（有限） | PASS（有限） | 站立画面与重开稳定；不证明行走碰撞 |
| 受控步行/坡面往返 | BLOCKED | BLOCKED | W/D/S/A 重复短按无可靠位移；T1 Normal 有一次短位移，但未完成路线 |
| 跳跃、坡面通行/碰撞 | BLOCKED | BLOCKED | Space 尝试无足够动态证据；W+Space 被工具明确拒绝 |
| 正常采得木材或高草 | BLOCKED | NOT_RUN | T1 primary drag 触发 Combat hit，未采得；两版库存空 |
| 可见结构接近/进入 | NOT_RUN | NOT_RUN | 观察范围内未确认可进入结构；无法拓展路线 |
| 正常保存、菜单重开、静止位置保留 | PASS | PASS | t1-06/07、t0-04/05；近景位置一致 |
| 状态保存 | PASS（HP8） | PASS（HP20） | 新版受伤后 HP8 保留；旧版 HP20 不变 |
| 采集/地形修改保留 | NOT_RUN | NOT_RUN | 没有成功采集/修改，不扩张保存结论 |
| 正常 Save and Quit | PASS | PASS | 点击后 getAXState 返回 App quit |
| 同 seed 可见地形差异 | PASS | PASS | v5 森林草土阶梯坡；v4 海岸沙滩、海面与远处沙丘草坡 |

视觉比较仅针对各版本同 seed 正常出生范围，出生点本身不同，不是强制相同坐标比较。没有使用固定镜头、传送或日照冻结。初次 T1 Normal 从出生点发生约17.2格水平位移，窗口内同时遭遇攻击；该单次结果也可能受到敌人击退影响，不能将位移归因于可控步行。Casual 两版的按键序列后位置均仍等于出生点，佐证连续路线不能认定通过。

## 环境与隔离

- record_version=1; executor=/root/terrain_normal_acceptance (独立子任务，GPT-6 家族；实现者 /root 未操作窗口)
- evidence_type=AI_INTERACTIVE + AI_VISUAL; scenario=T0/T1 terrain normal acceptance（有限 AI-01 对应项）
- control_method=public mcp__cua_repl App click/pressKey/drag/typeText; normal_input_only=true; debug_or_fixture_used=false
- actual initial/command cwd=/Users/lizi/Desktop/Workspace/HelloMine3D。游戏进程运行 cwd 为各包 Contents/Resources/bin。
- repository_accessible=true; context_isolation=PARTIAL。读取了验收技能、规范、截图工具及发行包 README；未读取游戏源码。
- accessible_roots=文件系统可读 /（含仓库及源代码）；可写仓库、/private/tmp、/private/var/folders/8n/qqg7p5691wq8gmblxz23zrqc0000gn/T、/Users/lizi/.codex/visualizations/2026/09/06/01a0756b-7da7-7203-81e5-83ee22fa09dd；本验收文档仅输出本目录。正常游戏自行写入包内 saves/logs。截图 helper 按父任务修复自动编译于已有 build/macos-window-evidence 缓存。
- strict AI-06=BLOCKED（未 package-only 隔离，且本任务不是盲玩）；functional_playability=NOT_CLAIMED（整体）；human_fun=NOT_CLAIMED; physical_input_feel=NOT_CLAIMED。
- os=macOS15.7.3 arm64 host; packaged architecture=x86_64 Release。GPU/驱动具体版本未单独取证。
- window=1280x720 client, screenshot=1280x748 including titlebar; fullscreen=0; locale=en-US; UI scale=1.00x。
- T1 设置界面读取：render distance8, directional shadows Off, post-processing Off, FOV90, sensitivity0.050, Full action feedback, captions/hints enabled。旧版未改画面设置，1280x720 窗口、英文与相同 UI 外观可见；未独立打开旧版设置逐项核验。
- initial_prompt=[initial-task.md](initial-task.md)；步骤、失败原文与顺序见 [operations.md](operations.md)。

## 包与存档身份

T1: HelloMine3D-T1-v5.app, commit850dd85969dce85fc8c533c451cda6ade12dd7b6, executable SHA256 `221f50c5aa4669ea8a54ab8fa8f121bede39d58b2fa967307bd1224d119997c7`。
T0: HelloMine3D-T0-v4.app, executable SHA256 `c0446b47bbdcf4b4e2ef69248e4d466288db1a10700c821e37ca2fb2e6480424`。各原图 JSON 内包含完整 build-identity 与精确运行 exe/window 匹配信息。

| 世界 | seed | terrain | difficulty | world id | 最后位置/健康 |
| --- | --- | --- | --- | --- | --- |
| T1 Normal 20260807 | 20260807 | 5 | Normal | world-9e31237560c43cd4fd5b990c0b411bf7 | -408.700012 73 -391.700012 /2 |
| T1 Casual 20260807 | 20260807 | 5 | Casual | world-3c68a9bed2ba4787c4f03218a57a0bcc | -391.5 73 -391.5 /8 |
| T0 Casual 20260807 | 20260807 | 4 | Casual | world-9e312375428a5fb4addfaf63727d0c4b | 168.5 71 264.5 /20 |

只读摘要和原 world.meta SHA256 见 [save-summary.json](save-summary.json)。所有验收新世界均空库存。现存 T0 Baseline 未在游戏内打开或修改；一次广泛只读文件枚举意外打印其 metadata，未作为本次证据。

## 证据缺口、失败与停止状态

原图序列及捕获时间/哈希见 [capture-index.json](capture-index.json)。t1-01至04捕获失败全部保留；父任务修复窗口查询的精确 executable 匹配，随后使用经授权桌面访问权限，t1-05起成功。这属于采证工具修复，不改变任何验收游戏包。无删证据、无审批拒绝、无意外对话框。首次 getApp 耗时306.34秒后成功；后续无超时。另有1次明确不支持的多非修饰键组合错误；退出后的 App quit 是预期。

Normal 下 HP20→10→2，Casual 下HP20→10→8，均由窗口可见攻击提示伴随发生。初次应用 Casual 在暂停内仅排队，保存回菜单仍显示 Normal，所以另建 Casual 世界作为一次玩法重试。文本框2次快捷全选失败后以三击正常修正。无法由当前证据确定键盘短按未移动的游戏/工具细分原因，不报告为产品碰撞 FAIL。

所有应用已通过 Save and Quit 正常退出，未遗留按住键、暂停世界或运行窗口。下一步需要支持真实持续按键/组合输入的公开 Computer Use 路径，再补坡面往返、木材/高草采得、修改保存与结构可达项；不得以本报告的静态原图替代这些动态证明。
