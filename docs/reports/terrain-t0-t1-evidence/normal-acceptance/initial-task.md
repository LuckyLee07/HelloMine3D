# Initial delegated task

执行 HelloMine3D T0/T1 的独立正常窗口验收。此委派依据项目 hellomine3d-validation 技能及 docs/current/ai-assisted-gameplay-acceptance-v1.md 第4节第8条要求实现者与验收者使用不同上下文；你是唯一窗口操作者，父任务只做证据整理，不会抢焦点。用户已授权本批 macOS 干净包、正常步行、资源/结构可达性、保存重开验收，可自主创建测试世界、正常键鼠移动/跳跃/采集/交互/使用菜单设置并保存退出，不需要逐步确认。不要修改源码、存档或注入物品，不启用强制位置/旋转、传送、fixture、validation-only、时间冻结或直接 Gameplay API。

包路径：新版 /Users/lizi/Desktop/Workspace/HelloMine3D/build/terrain-t0-t1-20260906/HelloMine3D-T1-v5.app，Contents/Resources/bin/HelloMine3D SHA256=221f50c5aa4669ea8a54ab8fa8f121bede39d58b2fa967307bd1224d119997c7，source commit=850dd85969dce85fc8c533c451cda6ade12dd7b6，macOS x86_64 Release；旧版 /Users/lizi/Desktop/Workspace/HelloMine3D/build/terrain-t0-t1-20260906/HelloMine3D-T0-v4.app，exe SHA256=c0446b47bbdcf4b4e2ef69248e4d466288db1a10700c821e37ca2fb2e6480424。两者均可独立运行，不需要源码资源。现在所有诊断采集已经结束，窗口可由你使用。

具体目标：先验证新版，正常主菜单新建 seed 20260807 世界。实际走一段包含地形起伏的路线并回程，记录支撑、碰撞、跳跃/坡面通行、附近主线资源（至少正常采得木材或高草）、可见结构的接近/进入情况（若附近可发现）。正常保存退出，再从菜单重开同一世界验证位置/修改保留。保留动态动作序列与前后原图，不能用截图或源码分析冒充移动。随后旧版以相同 seed 正常创建世界，执行一个可比较的步行/保存重开短路线，说明新旧地形的可见差异。若实际环境或工具阻碍某项，保留尝试和原因并继续可做项，及时告知父任务；不要轻率以“工具可能不支持”放弃，先检查真实 CUA 文档并尝试正常输入。

正常输入只用公开 CUA 接口（mcp__cua_repl）；允许脚本启动包、存原始窗口截图。可以使用已有 /Users/lizi/Desktop/Workspace/HelloMine3D/tools/macos_window_evidence.py --app ABS_APP --configuration Release --launch --output NEW_PNG --note TEXT 保存原图和身份，每次新文件名。可按需读该工具/验证技能/验收规范与发行包控制说明，但无需读游戏源码。运行后只读检查自己的日志/存档用于佐证，不编辑它们。技能若提到 strict AI-06：本任务不是盲玩 AI-06，当前仓库仍可读取，记录 context_isolation=PARTIAL，不能宣称 PACKAGE_ONLY 或 AI-06 blind PASS。独立上下文仍需记录。

所有输出仅写 /Users/lizi/Desktop/Workspace/HelloMine3D/build/terrain-t0-t1-20260906/normal-acceptance/；创建目录并保存 report.md、操作记录、截图、包身份和每项 PASS/FAIL/BLOCKED/NOT_RUN。报告列出实际 cwd、可访问根（仓库可读）、初始任务描述、世界名称/seed/版本/设置、所有失败/重试及停止状态。不要提交 Git 或更新父任务报告/TODOLIST。完成后正常保存退出游戏并回报证据路径。
