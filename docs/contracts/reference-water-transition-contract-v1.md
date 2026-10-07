# 有界生产水面跨越观察 v1

状态：当前 r8 Debug／Release 已实际构建，Root 串行执行的两个原生正例各通过176项独立检查；真实 retain-water-binding 负控自然退出1并被拒绝。校准从真实正例制作的24份故障副本全部拒绝，聚合审计119项通过；另有三项实际入口拒绝及72项独立审计。执行身份、原失败和证据路径见[本轮记录](../reports/reference-visual-goal-execution-2026-10-06.md)。工程观察不替代普通输入游泳／跨水面路线，不将四阶段截图视为完整60Hz观察。

本观察弥补同一个 Root 中 `World` 真实相机介质 → `PlanarWaterReflection::render` → `bindWaterPass` → 主 Water 实际 driver draw 的跨越／恢复缺证。它不改 World、区块算法、水格式、shader、普通配置或旧 lifecycle/edit/residency 协议；普通启动不创建探针／listener，不新增 World 查询或 GL 观察。组件正常路径只多记成功 RTT 完成的 `lastRenderedFrame` 数字，不能把最新输入的 `frameSerial` 当成 inactive RTT 已更新。

## 准入与拥有边界

在 ResourcePaths、manifest writer、World 构造前检查。仅存在 `HELLOMINE3D_REFERENCE_WATER_PROBE`、`HELLOMINE3D_REFERENCE_WATER_FAULT` 或 `HELLOMINE3D_REFERENCE_WATER_DIR` 才进入检查；正式 opt-in 必须为 `PROBE=1`。`FAULT` 仅允许空或 `retain-water-binding`。无 opt-in 保留普通路径。

严格拥有布局：session 下 `Runtime.app/Contents/Resources`、`save`、新 `catalogue`、新 `water`（同时为主 capture 输出）。marker `.hellomine3d-reference-water-transition-owned` 必须是普通文件，内容精确为 `HelloMine3D owned reference water transition session v1\n`；marker、world.meta、chunks 与各路径祖先不得符号链接。Runtime／save 必须是真实隔离副本，不写源 app、模板或既有输出。

仅允许合同列出的 ROOT、SAVE、CATALOGUE、WINDOW_HIDDEN、capture、MSAA、初始 seed/time/pose 和本探针变量；其他 HELLOMINE3D_/HELLO_RENDER_/HELLO_PERF_/HELLO_VISUAL_ 入口（含 validate-only／manifest／perf／其他 probe／强制 fallback／fixture）早拒。Root 外部 runner 另负责源／模板／保护 app 哈希、实际加载可执行文件证书、仅自有进程清理与总 deadline，不能用内部帧检查中断同步保存。

固定初始 pose `195.5 68.02 -176.2 / 5 45 0`、world time 6000、seed 42，实际配置 windowed 1280×720 point、RD3、standard/linear-HDR、MSAA4、medium shadow、post off、first person；第一 checkpoint 必须实测窗口 2560×1440 及 HDR4 真实颜色／depth 附件，不能从配置推导实际 Retina 或 sample storage。Capture=1、MS=60000、EXIT=0、MAX_DELTA_MS=5000，仅用于允许本探针显式 readback，普通 timer 在内部 30 秒界限之外。

## 四阶段与生产操作

阶段固定 `above-a → collar → below → above-return`。全部同 Root／Scene／Window／World 实例与 world_id。每实际正常帧通过 `WorldManager::teleportPlayer`、逻辑 Camera update、正常 Sandbox update，将 body 请求到水列 x194.5,z−182.5，eye 相对水面均值的请求偏移分别 +1.20、+.14、−.35、+1.20 m；每帧重复生产 teleport 清速度，用于稳定有限工程机位，明确不是普通游泳输入。请求值不能自证实际位置／介质。模拟 delta、World time、Ogre animation time 都正常推进，不冻结 actor／streaming／时间。

样板真实已驻留水列 (194,64..66,−183) 必须 Water7/meta0，下方 y63 必须非 Air；各 checkpoint 用既有 blocking/find-only `World::getBlock` 严格复读三格。选择平面由当前 `observeWaterSurfacePlane(actualEye)` 得到 66.9 m，不硬更改组件选面／世界水位。

成功 arm 前连续 12 个实际满足介质／眼高条件且正常模拟推进的帧。above 两阶段须 World 主眼所在格非 Water 且 eyeY > selectedPlane+.15；collar 须非 Water 且 0 < eyeY−plane <= .15；below 须真实 Water 格且 eyeY < plane。记录实际 main／logic／Player pose、当前格 ID/meta、上邻格、depth/immersion 与实际 linked fogDensity。介质来自真实 World 值，不从请求偏移或 shader flag 推导。

全探针最多 30 秒／2048 帧；每阶段最多 8 秒／512 帧；journal 最多 32 条；全部输出最多 32 文件／256 MiB。bounds 内未能达到真实条件即失败，不追加宽松重试／门槛。

## 事实与资源域

`water/journal.jsonl` schema `hellomine3d-reference-water-transition-journal-v1`；每阶段恰一次 begin、observation、checkpoint、end，共 16 条。observation 在严格资源／绑定门禁之前落盘，用于保存真实故障。checkpoint 与 observation 的同帧事实必须一致。

`facts.planar` 区分最新 frame/input scene_revision 与仅成功实际 RTT update 才写入的 last_rendered_frame/update_count。frame_input_scene_revision 与稍后 World revision 分记，允许异步背景工作推进全局 revision，不放宽实际输入与本次 view 一致性。

`facts.planar.pass` 域为 `actual-Ogre-Water-pass-parameter-and-TUS`，记录 named enabled float 与名为 `planarReflection` 的真实 TextureUnitState 是否存在、索引、纹理名、是否匹配组件目标。`facts.draw` 域为 `actual-driver-after-native-Water-draw`：现有 NativeDrawObserver 在实际生产 draw 前后查询 GL_PRIMITIVES_GENERATED、实际 GL_CURRENT_PROGRAM/link/attached VS+FS、Water 各 named uniform、真实 sampler unit 与 active attachment/storage、GL blend/depthwrite、query 的状态恢复及 GL error。没有创建替代场景 draw，不把 setter 输入／排队 RenderOperation 当实际 driver uniform。SSO pipeline 不在本机有限观察域，出现即失败。

above／return 必须实际 active、reason rendered、last_rendered_frame=current frame、当阶段 update_count > floor；真实 sampler ID 必须等于组件原生 resolved RGBA16F texture。实际附件 1280×720、single sample、owned depth/pool0，主 HDR4 2560×1440。尺寸／private material≤96／private passes≤384／一 camera／一 RTT／一 depth 等既有预算仍强制。

collar／below 必须 actual inactive、reason underwater-or-surface-crossing、update_count 等于上一阶段 floor，last_rendered_frame < 当前输入帧；真实 Ogre enabled0、reflection TUS 删除、component waterSamplerBound false、实际 linked Water enabled0 与 sampler 不消费。旧 GL texture binding 可以合法存在或别名其他 unit，不能因此强求 texture0；也不能将旧 RTT storage／frame 当本帧更新。至多保留一份合法 RTT/owned depth 与有界私有材料，停更不等于销毁组件。

仅 above-a／above-return 显式调用既有 `captureDiagnostic`，写原生 RGBA float32、camera-preview 和 facts，各一次，共两次；不读取 inactive 的旧 RTT。四阶段各一主窗口 PNG。原四次生命周期 readback 预算不扩；不要求正常时间推进的两份 RTT 全图逐字节相等。独立 oracle 检查完整 PNG CRC/pixel bytes、raw 字节数／finite／真实信号、实际源附件身份与 readback frame；视觉残余由 Root 真正看原图后单独记录。

成功后用真实 production teleport 恢复探针开始的 Player position/rotation，记录 actual/restoration 相等，再正常 World.save。该末端不是普通保存重开证明；组件 teardown 沿正常 shutdown，observer/query 在 World／Scene 销毁前 detach。本探针不重写原 13 阶段 lifecycle 结论。

## 负控与独立 oracle

真实 native fault `retain-water-binding` 只在 collar 跳过实际 `bindWaterPass`，让上一 above 的真 TUS/flag 保留；正常 render 已进入 inactive、不再更新 RTT，主 Water 实际 linked enabled 仍为1。记录这份错误 observation 后真实 strict gate 必须非零退出，不能靠 JSON 改写代替实际故障；异常 cleanup 仍正常解绑／释放。

`tools/tests/reference_water_transition_oracle.py` 只读实际 summary/journal/PNG/raw，外部实际 process exit code 是独立必填，不接受 COMPLETE 掩盖非零退出。成功 schema `hellomine3d-reference-water-transition-summary-v1`、status COMPLETE、completed_phases4、normal_save true/input0。校准只能从真实 COMPLETE 原件制作副本，覆盖错误介质／高度／旧 update／TUS／driver flag／假域／旧 Root／元数据／GL／存储／退出码／PNG/raw 损坏等；不合成正例、不删或改任何历史压力失败。

只有固定样板单水位被本探针覆盖。没有声明可用的第二真实 resident 水位，所以 summary 明确 `other_resident_plane_transition=NOT_RUN_NO_DECLARED_SECOND_RESIDENT_LEVEL`；不伪造两水位实体、不把 GPU fixture 的多水位 branch 当 native 切面验收。
