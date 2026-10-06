# HelloMine3D 验证矩阵

本文把改动类型映射到最低必要验证。H2-H3、Q1-Q3、Stage 9/BETA-RC、Stage 10/VISUAL-RC
和 Stage 11/P11F 的 Windows 自动工程范围已经集中封板。项目现采用
`docs/current/ai-assisted-gameplay-acceptance-v1.md`：自动化证明确定性和工程边界，AI/Computer Use
证明真实窗口中的功能可玩性，AI 视觉检查证明可观察缺陷边界；人类乐趣、审美、舒适度和
物理设备手感为 `NOT_CLAIMED`。

R3 v1、Physical Input v2 和人工产品体验 v1 继续作为历史合同保留，但已 `SUPERSEDED` 为
Architecture Lab 的当前退出门槛。它们的模板保持 `NOT_RUN`，不得由 AI 冒充物理操作者填写
PASS。新的 AI 场景尚未执行时只写 `NOT_RUN`，不写成永久 `Deferred`。

历史运行结果和逐项证据保存在 `docs/archive/project-ledger-2026-08-17.md` 与
`docs/current/runtime-validation.md`。

2026-09-05 TODOLIST Goal 经所有者明确调整：本批先执行 macOS 对应构建、自动测试、干净包和
真实窗口场景，Windows 专属验证后置；以下 Windows 路由保留，但不再阻塞本次 macOS 交付。
必须区分 gmake/Apple clang、Xcode、x86_64/arm64 和 Windows 证据，不能互相冒充。
AI 平台对应规则见 [当前验收规范](ai-assisted-gameplay-acceptance-v1.md)。

冒险体验阶段按用户 2026-09-26／27 的调整执行：下表及相关合同中的严格三对性能、
P95／P99 ≤1.10 和细微调参统一为 `DEFERRED_BY_USER`，不再阻塞本阶段功能交付。
原协议及历史 PASS／FAIL 留存，不能把延期记作通过。当前性能阻断限于崩溃、明显卡顿、
无界增长和固定容量／资源预算失控；功能、兼容、正常玩法、动态与实际听感仍按各自要求验证。
恢复精细性能专项时沿用原冻结条件；未恢复前不因下面保留的历史门槛重复采样或调参。
完整依据见[冒险体验 Goal](adventure-experience-goal-prompt-2026-09-22.md)。

2026-09-28 [视觉精修规划](visual-experience-polish-plan-2026-09-25.md)承接阶段放行，保留 V01–V10
范围；该新目标暂不设置人工核验、人工审美确认或独立试玩签字，严格配对性能继续延期。
自动工程检查和 AI／Computer Use 自测仍需按实际变更完成；旧目标缺项保留原结果，不作为新阶段
整体前置，也不记为 PASS。具体退出与受阻处理以该规划第六节及其 Goal 提示词为准。

## 日常开发最低门槛

按实际改动匹配下表，完成对应合同的必需检查。检查通过后，只有新改动、失败或未解决疑点
才重复或扩大验证；不把完整里程碑门禁应用到每次文字提交。执行/恢复方法见
[AI 工作流](agent-workflow.md)，它不替代本矩阵的验收要求。

| 改动 | 最低必要验证 |
| ---- | ------------ |
| 所有 C++ 改动 | 受影响目标能够编译；运行对应定向自动测试。 |
| 世界、区块、实体或持久化 | 定向自动测试 + `HelloMine3DWorldRuntimeSmoke`。 |
| 冒险地图区域规划 | `bash scripts/verify_adventure_terrain.sh Debug` / `Release`；`HELLOMINE3D_WORLD_SMOKE_FOCUS=ADVENTURE` 生产区块、水柱、植被、正逆加载与默认版本保存重开；v1–v15 T0 生产摘要对照、完整世界回归、双配置客户端及适用视觉/性能。地区覆盖不代替实机效果，参见[区域骨架合同](../contracts/adventure-terrain-v16-contract-v1.md)。 |
| 冒险地图水系 | `bash scripts/verify_adventure_water.sh Debug` / `Release`；`HELLOMINE3D_WORLD_SMOKE_FOCUS=ADVENTURE_WATER` 检查八 seed 河湖实际块、水柱、岸坡树冠、正逆生成及冻结 v17 保存重开；v1–v16 T0 生产摘要、完整世界回归、双配置客户端与冻结干岸实机。最终配对性能随生态整合执行，见[水系合同](../contracts/adventure-water-v17-contract-v1.md)。 |
| 斜向岸坡网格 | `HELLOMINE3D_WORLD_SMOKE_FOCUS=BANK_MESH`：正负坐标阶梯、AO 开关、实际湖岸三角形平面／朝向与暴露面积；结合固定机位图形对照。几何检查不代替岸坡美术判断。 |
| 水域运动精修 | `tools/tests/water_motion_test.cpp` 双配置检查三seed下游切向／连续性／有界缓存；`WATER_MOTION` 检查四类真实水域、快照查询上限、共享角点、非水位回退及岸线编辑；含速度数值命中植被图集槽的负例。双配置复核 `WATER_DEPTH`、`BANK_MESH`、`V10B3`、MeshDirty和资源，Release完整世界、双配置客户端；v26 T0逐字节不变。实际GLSL验证四档幅度、接缝、下游细纹、远纹滤波、亮度上限与故障负例；四水域昼昏原图、普通出入水／编辑分开记账，见[水域运动合同](../contracts/water-motion-polish-contract-v1.md)。 |
| 冒险生态材料与树种数据 | `HELLOMINE3D_WORLD_SMOKE_FOCUS=ADVENTURE_MATERIAL` 检查末尾追加 ID、工具／掉落、真实采集放置、12 个网格材质槽及 metadata 保存重开；资源包与配方回归、图集校验和标准／兼容材质展台。新生成与生态路线另按 [v18 合同](../contracts/adventure-ecology-v18-contract-v1.md) 验收。 |
| 冒险生态实际生成 | `bash scripts/verify_adventure_ecology.sh Debug` / `Release`；`HELLOMINE3D_WORLD_SMOKE_FOCUS=ADVENTURE_ECOLOGY` 检查八 seed × 九区域实际地表、水柱、树型、地被、正逆生成、碎片 metadata 和默认 v18 保存；v1–v17 T0 摘要对照、完整世界回归及双配置客户端。预冻结机位与连续路线分开验收，最终性能和普通探索仍按 [v18 合同](../contracts/adventure-ecology-v18-contract-v1.md) 与目标执行。 |
| 植被空间与树冠精修 | `VEGETATION_POLISH` 双配置检查 v24 追加、六种树的高矮／冠幅／相连方块、6 米包络、地表水体保护、正负坐标跨块、三冻结种子的地区／坡地／雪线及地被、扩大后的入口／工位来源避让、v23／v24 保存身份；最终候选与修改前 v1–v23 生产 `T0-SURVEY` 两份 CSV 逐字节对照，Release 完整 WorldRuntime 和双配置客户端。当前画面与普通路线分别取证，按[植被精修合同](../contracts/vegetation-polish-v24-contract-v1.md)，不能只凭概率字段或静态图关闭 V02。 |
| 局部地形精修 | `tools/tests/local_relief_test.cpp` 双配置检查三冻结seed的位移／坡差／水体保护、连续材料、极值与线程缓存；`LOCAL_RELIEF` 双配置核对真实区块、反序生成、支撑、出生及改块保存重开。修改前v1–v24 `T0-SURVEY`摘要对照、Release完整世界、双配置客户端，重点复核 `ADVENTURE_EXPLORE`／`ADVENTURE_SURVIVAL`，不能牺牲开局木材和种子。实景固定机位与普通路线分开，按[局部地形合同](../contracts/local-relief-polish-v25-contract-v1.md)。 |
| 河湖岸形精修 | `tools/tests/waterbank_polish_test.cpp` 双配置：三冻结seed宏观／出生／边界列、连续真实河道中心线及横断面、旧湿润主槽连通、宽窄／非对称岸／滩台、极值和跨版本缓存。`WATERBANK_POLISH` 检查真实水床／材料／水柱、反序生成、岸线改块保存和默认新世界出生；复核 `LOCAL_RELIEF`、`WATER_DEPTH`、`BANK_MESH`、`ADVENTURE_EXPLORE`／`ADVENTURE_SURVIVAL`。v1–v25修改前后T0摘要、Release完整世界、双配置客户端；固定机位与普通入水／岸线编辑分开，见[河湖岸形合同](../contracts/waterbank-polish-v26-contract-v1.md)。 |
| 冒险地图探索整合 | `HELLOMINE3D_WORLD_SMOKE_FOCUS=ADVENTURE_EXPLORE`：八 seed 普通出生、实际木石资源、有界陆路连接区域核心／水岸／三类地点、洞口植被避让、雪层保持、自定义 Cross 回退、v19 保存重开；完整世界回归、v1–v18 生产摘要、双配置客户端。陆路查询与正常输入分开，见 [v19 合同](../contracts/adventure-exploration-v19-contract-v1.md)。 |
| 载入/卸载边界方块光优化 | `LIGHT_BOUNDARY` 覆盖光源/遮挡、跨区块编辑、完整邻区块光场在光源卸载后归零、九区块正逆加载及 detached commit 的完整光场、熔炉燃烧状态；完整 WorldRuntime 和双配置客户端。不得减少真实亮度、邻区块范围或世界更新预算。 |
| 封闭网格输入跳过 | `MESH_INPUT` 覆盖完整 halo／编辑 revision、默认完整快照、封闭输入零生态查询、开洞后复用输入与网格重建、同步与异步跳过；完整 WorldRuntime 和双配置客户端。输入省略不能改变任何可见层或状态机。 |
| 冒险地表纯查询缓存 | `ADVENTURE_QUERY`：八 seed／v17–v19 与无缓存规划逐列一致，负坐标／整数极限、超容量替换与交替世界身份、共享生成器并发；完整 WorldRuntime、v17–v19 生产摘要和双配置客户端。不得缓存实际方块或改变生成版本。 |
| 物品、容器、制作、工具或食物 | 状态守恒、容量边界、失败原子性、固定 tick 和保存/重载测试。 |
| 箱子容量与双向转移 | `HELLOMINE3D_WORLD_SMOKE_FOCUS=CHEST_CONTAINER`：实际箱子开启／存取、容量限制下的部分存入与取出、满容量拒绝、其他槽不变、事件实际增量及双方库存保存重开。聚焦入口不替代界面反馈与普通采集验收。 |
| UI 或输入 | 动作仲裁、焦点隔离、映射/冲突、设置迁移自动测试；macOS Cocoa 改动在已构建对应配置后运行 `bash scripts/verify_cocoa_input.sh Debug` / `Release`（非可见自动回归）；适用时运行 `AI-01..AI-04`。OS 焦点、Alt+Tab、最小化和窗口关闭只能由 Computer Use 关闭功能范围。 |
| V06c 水岸编辑原上传与地图像素 | 正常 Debug／Release 客户端构建与独立源码复核；以全新隔离目录执行 `python3 -B tools/capture_visual_macos.py --app <独立候选.app> --output <新目录> --shore-edit --shore-edit-site river --shore-edit-target "220 64 -204" --scene shore --seed 42 --time 7000 --render-distance 1 --visual-detail standard --launch-method direct --perspective first --minimap-range 256 --pixel-ratio 2 --width 1280 --height 720 --ui-scale 1 --locale zh-CN`，compatibility 使用另一新目录。冻结机位的自然目标距真实沙岸3 m，六阶段生产填砂／拆砂与诊断恢复分记，不换点冒称四水域或三种子覆盖。`python3 -B tools/tests/shore_edit_capture_oracle.py --capture <标准/capture.json> --capture <兼容/capture.json> --output <新报告.json>`：各208项核对44 B／u32原上传CPU及原生存储、当前uploaded revision／GpuResident、真实3×3 World四角水深、编辑后HUD256 step4先刷新既有同列2 m细历史，再由RD1 step2平面图显示；捕获帧目标未被重新查询，目标原PNG材料／高度颜色变化、西／北细历史及像素保持。原子World快照、incarnation／ABA、内部VAO取数、地形像素归属及实际鼠标选点不在该PASS内；4 m保存重开另验，2 m历史不落盘。`--fault-suite <另一新目录>`对真实raw／facts副本的NaN、stride32、越界索引、旧水深UV、缺HUD细历史重算外层SHA后要求语义拒绝；V06c当批源码生产者的跳过上传／取消深度更新／旧UI故障为NOT_RUN，后续实际源码校准分记V06d。运行时构造世界前拒绝已有save或共用save-catalogue，两例正常exit1／无signal、哨兵保持；普通菜单隐藏渲染另验。195列／30Hz、地图容量／版本不变；普通出入水、编辑、连续路线、三种子与切世界保留，V06／V10仍Doing。见[执行记录](../reports/visual-experience-polish-execution-2026-09-28.md)。 |
| V06d 实际生产源码故障校准 | 本批冻结命令依次为 `python3 -B build/visual-experience-polish-20260928/v06d/build_producer_faults_r1.py`、`package_producer_faults_r1.py`、`run_producer_faults_r1.py`（后两者位于同目录）；输入 `producer-source-prepare-r1/receipt.json` 与 `inputs-before.json`，实际编译／链接 argv、隔离单TU替换和 nominal reference／effective override 包身份分别见 `producer-root-build-r1.json`／`producer-root-package-r1.json`。这些r1脚本绑定已完成的新目录，不能原位重跑；新增轮次须准备另一独立目录并处理超时子进程收敛，不覆盖旧收据。`producer-source-negatives-r1.json` 与 `final-classification-review-r1.json` 核对5508冻结输入、15包2110文件及用户135管理项保持。HUD省略step4细历史刷新：standard native PID8443正常exit1／无signal／无timeout，13次实际粗Sand64／细Water64错态，三幅原图后被原有界readiness guard拒绝；不完整捕获的独立oracle为NOT_RUN。固定基线水深：standard PID8497／compatibility PID8530均native正常exit0、六幅原图、外层原文件SHA通过，再由未修改的独立oracle正常exit1在 `phase1/actual-World-four-corner-water-depth` 拒绝应为1.75而原CPU／GPU仍为2.0；不把子进程0当故障未检出，也不把HUD guard拒绝称oracle拒绝。skip-upload源码故障／内部VAO／incarnation及ABA仍OPEN；普通输入与连续路线未验。 |
| V06d 实际倍率1匹配正控与失败边界 | 使用正常Release独立副本：`python3 -B tools/capture_visual_macos.py --app <正常独立候选.app> --output <全新目录> --shore-edit --shore-edit-site river --shore-edit-target '220 64 -204' --scene shore --seed 42 --time 7000 --render-distance 1 --visual-detail standard --launch-method direct --perspective first --minimap-range 256 --pixel-ratio 1 --width 1280 --height 720 --ui-scale 1 --locale zh-CN`，compatibility另用新目录；`python3 -B tools/tests/shore_edit_capture_oracle.py --capture <standard/capture.json> --capture <compatibility/capture.json> --output <新报告.json>`。本批 `v06d/control-{standard,compatibility}-scale1-r1/capture.json` native PID7887／8266均exit0，六阶段各208检查通过，实际原图1280×720，外层SHA及5508输入保持，终态见 `control-dual-scale1-oracle-r1.json`。`control-standard-r1` 默认OpenGL初始化不支持3.0／native正常exit1的FAIL，以及 `control-standard-r2` native0完成六图但请求倍率2／实际倍率1的FAIL完整保留；二者均非源码故障检出。实际倍率1只证明同条件专项校准，不能写当前Retina2、普通交互或总Goal PASS，也不回写V06c历史倍率2结果。 |
| V06e 当前Retina2匹配正控及漏VBO写入源故障 | 正常正式4f6运行时不重编，沿V06c冻结shore命令在全新 `v06e/control-{standard,compatibility}-r1` 输出，请求倍率2／1280×720点，实际两模式2560×1440、GL4.1.0.0、native0；没有额外GL环境覆盖，各原oracle208项通过，独立245项复核。实际单TU源副本只省略VBO writeData，编译／链接argv见 `v06e/build-r2.json`，146对象仅换1个，145对象＋17库及223聚焦输入保持；`package-r2.json`标明nominal4f6与effective override，实际Mach-O为Resources/bin的493339…c13ad，MacOS路径只是launcher，独立源包37项通过。Root实际先执行 `python3 -B build/visual-experience-polish-20260928/v06e/run_gpu_r1.py --stage negatives --round r1 --pixel-ratio 2`：标准PID66170正常exit1，但r1检查器少读生产固定错误前缀，driver FAIL保留且未运行兼容。新 `verify_failure_r2.py --capture <原capture.json> --mode <standard或compatibility> --binary-sha256 493339000cd424e7e0ae6ef55952f1ab0d4a6278ec6d6bea83eee04c443c13ad --output <新报告.json>`仅更正该字面预期，复用标准原件；随后同冻结命令仅补兼容PID67106，正常exit1。两模式各26项、独立98项确认首solid操作真实VBO≠CPU／IBO=CPU、44B／u32、current revision、GL0和状态恢复。完整六阶段故障oracle为NOT_RUN_INCOMPLETE_CAPTURE；新分配未写入不证明旧revision复用、Water操作、内部VAO取数、ABA、地形像素或普通输入。准备和复核r1失败原件保留，新收据不覆盖原件；绑定完成输出的脚本不得原位重跑。终态及边界见[执行记录](../reports/visual-experience-polish-execution-2026-09-28.md) V06e及 `source-upload-calibration-result-r1.json`。 |
| V06f 实际绘制观察 | 按[合同](../contracts/shore-native-draw-observation-contract-v1.md)在V06c冻结命令上显式增加`--shore-native-draw`，standard／compatibility使用全新隔离目录；`shore_native_draw_capture_oracle.py --capture <标准/capture.json> --capture <兼容/capture.json> --output <新报告> --fault-suite <新目录>`先保留原208项，再核实际暖VAO／活跃float44B输入／真实VSFS与EBO／完整primitive query及post原缓冲。当前r3双模式各595项通过，9元数据/raw副本故障原208通过后被新语义拒绝；独立实物1114项、12阶段38操作176属性通过。关闭新观察的当前二进制双模式各原208项通过；正常Debug／Release修后构建0，主菜单渲染两幅2560×1440原图通过。初次GL4.1错误查询4.3 ARRAY_LONG的失败保留；修后4.1以实测FLOAT排除DOUBLE并明示推导，4.3直接query。获准沙箱外确认桌面／显示器正常并恢复GL；默认沙箱CG0不能代表物理显示器0。公开CUA仍报服务启动失败，普通输入未验，首解绑／非主相机／像素归因／World原子快照／ABA保持OPEN。恢复与当前包身份见执行报告V06f。 |
| V06g 水面几何／投影对照与generic生命周期 | 固定当前V06f冻结包cd47／7b，同机位seed42、player21270-192、旋转10/90、time7000、RD8、FOV90、1280×720点／Retina2、5/6/7/8秒；新隔离副本geometry-zero仅两animatedVertex波幅置0，保normal／fragment；公共viewProj另一副本保全波浪。原ROI max139，geometry-zero10/9/10且>25=0，公共投影仍139，后者不能作为修复。所有direct捕获记录工具SHA及真实PID／退出码／signal／timeout／reaped；实际PID39079／49669均正常0、无signal／timeout、已回收，四帧各2560×1440；generic成功已实跑，timeout／信号清理未校准。独立准备52／52、原件／ROI60／60。5→6加载调度与World时刻不同、相位／实际camera未冻结；CaveBoundary首条0不代表全程0。只限几何参与定位，不声明World面归因、跨chunk持续稳定或普通输入；生产shader／C++／正式包保持，下一步实际相邻原Water属性与uniform。CUA仅只读重检超时、桌面由用户确认正常；不新增人工gate。详见执行报告V06g。 |
| V06h 启动完整邻域网格 | 实际World／ChunkManager存储加载保留旧CPU mesh反例122项、零加载guard与原split worker恢复118项，非海平面回退用例与自然像素归因分开；raw消费者96项检查重复角点及旧96内部侧三角形。生产Bootstrap仅完整3×3ready时启动建网格，缺邻保持Dirty；loader独立于初始solid数量。正常Debug／Release编译，实际MMD及仅Bootstrap对象变化；生产Water shader、存档格式与预算保持。当前Release标准／兼容seed42同RD8机位各5／6／7／8秒原图，正常退出；固定原ROI重算，标准139→10／9／11、>25全0，限本机位。当前标准RD1原水岸六阶段595项全部通过；零solid启动、独立复核及正式包终态见执行报告V06h。普通输入和全场景连续无闪烁仍开放，不放宽旧门槛或用关闭波浪完成精修。 |
| V06d 普通菜单地图与切世界 | 普通配置独立工作副本、公开CUA实际输入；本次PID14432／主窗口3968，仅一个进程依次进入42、42、20260807、42。正常创建两个terrain30世界，42平面选点添加基地X36Z54／追踪，保存返回后重进保持；20260807不同地图0/64，返回42原基地及追踪1/64。原图命令 `python3 -B tools/macos_window_evidence.py --app <普通工作副本.app> --configuration Release --window-id <已核对主窗口> --output <新原图.png> --wait-seconds 2 --note <实际观察>`；本次四张2560×1496完整原图及实际输入记录见 `v06d/ordinary-ui-r1/session-r1.json`。`independent-review-r1.json` 52项通过，18保存文件live／copy／SHA一致、8地图文件完整解析与FNV校验，42为9tiles／5329已知4m格、Home1／tracked1，20260807为9tiles／5280格、markers0／tracked0。原图独立确认平面、380及594视野，475→380过程／局部切换仅Root现场记录。首次未指定主窗口截图FAIL、坐标半缩点击未命中、指针模式下短按无移动均保留；后续S未形成可靠位移，不记连续路线。普通菜单退出有完整Ogre关闭且随后pgrep无PID，native退出码不可得；独立ps默认sandbox拒绝126也保留，不造exit0。V06d首会话中跨进程重开、第三种子、右拖平移、关闭—移动—重开及水岸编辑待验；前两项后续见V10k，不关闭完整V10。 |
| V10k 普通跨进程地图与第三世界 | 在首会话普通菜单Exit且无PID后，公开CUA重新启动同一普通工作包，不用诊断save／位置／库存；actual native31861／主4213与旧14432／3968不同。普通Continue42核对基地名称、X36Z54、基地／追踪1/64；实际Create239701883／terrain30→Enter→小地图Local→Flat个人页不同地图0/64，再SaveReturn→42 Enter恢复原基地与追踪1/64。三次准确窗口原图沿用 `tools/macos_window_evidence.py --window-id <实际主窗口> --output <新PNG>`，本次各2560×1496，证据 `v06d/ordinary-ui-r2/`。独立只读69项通过，24保存文件live／copy／记录一致、8份地图完整解码／FNV有效；42原hmap／5329已知4m格及Home1／tracked1保持，20260807六文件字节保持，第三世界marker0／tracked0；首会话原件及未进入20260807保存保持。数值paste未生效后公开typeText确认239701883才Create，不造错误世界。普通SaveAndExit有08:05:32完整Ogre关闭，pgrep query1／空，native自身退出码不可得。只证明42跨进程4m归档／标记持久化及结合前会话的三个世界标记隔离；无游戏移动输入、普通编辑、右拖或三种子完整路线，不承诺2m细历史跨重启，完整V10与Goal仍Doing。 |
| V10m 普通紧凑双语空随身合成 | 当前正式4f6d0c55／Release `10c392d3…e6356`的独立普通副本，精确复制旧163文件与正常存档，不用诊断位置／库存／时间。Root唯一公开CUA输入，实际三进程36854设置、37162紧凑双语、39159恢复重启；公开设置640×480点／1.25后Continue42，以E打开空2×2随身合成。中英文手册0/10展开提示可读，五空材料槽经滚动可达，网格／产物／操作区保持可见；英文折回已观察，不称全部材料始终同屏。空配方制作按钮禁用点击未新增产物，合成期间F5的两语言配置各自字节保持；第一次Esc回世界、第二次进入暂停。原705B配置通过公开设置逐字节恢复，重启宽屏中文主菜单确认；12准确窗口原图、24保存文件、六配置与三日志见 `v10m/session-r1.json`。001–011实际37162／4668原图1280×1016，012实际39159／4848原图2560×1496，均含标题栏；三日志完整关闭，pgrep query1／空，native自身退出码不可得。Root保护50项通过，精选46轮186图、旧45轮183图保持；[独立原图／存档复核](../../build/visual-experience-polish-20260928/v10m/independent-review-r1.json)221／221通过：12原图实际查看，空库存与原位姿／标记保持，16主／备地图独立FNV有效。只补普通空随身合成可达性及输入归属，未验证已发现配方／Load、制作commit拒绝路径、3×3手册／实际制作、普通采集制作、完整UI矩阵、连续路线或音频；完整V10与Goal仍Doing，正常源码未改、未重建。 |
| V10n 普通设置取消／应用／跨进程保留 | 复用同一4f6d0c55普通副本，无位置／库存／时间注入或重建。用户解锁后Root重新观察同一41853／4883窗口再Continue42，旧锁屏收据保留。草稿960×540／FOV60／第三人称在Cancel后重开恢复1280×720／90／first，草稿及Cancel后配置字节保持；单变量FOV Apply仅改fov，原图观察投影收窄；随后Apply第三人称与窗口尺寸，角色可见而当前窗口不改，真实新进程42949／5011确认960×540内容尺寸及60／third保留。初始化日志仍1280×720，实际尺寸以原生query／PNG为准，不推断差异原因。公开设置完全还原原705B配置，43278／5052最终重启宽屏中文菜单确认。九完整窗口原PNG、七配置、三日志及前／两次保存后各24文件见 `v10n/session-r1.json`，三个会话完整关闭且pgrep query1／空，游戏自身退出码UNAVAILABLE。[独立复核](../../build/visual-experience-polish-20260928/v10n/independent-review-r1.json)122项：121 PASS／1错误预期FAIL，产品失败0，设置限定验收PASS；新backup8/9独立FNV有效，两个未进入世界保存字节保持。42位姿／health20／五空槽保持，第二次保存actor_count 0→5，不能称存档完全不变或证明生物动作。Root聚焦保护35项通过，复用上批完整保护；精选47轮189图，旧46轮186图保持。旧4.2米是spawn与保存字段之差，无W前基线，不能量化W或断言自动出生修正，独立因果复核29项通过。单次同点drag及S短按未证明采集／移动；近墙相机、连续路线、普通采集制作、完整矩阵／音频仍OPEN，完整V10与Goal仍Doing。 |
| V10l 普通按钮平移与无移动重开 | 普通工作包沿用正式4f6d0c55／Release `10c392d3…e6356`，不改源码或重建；本次actual PID34184／主窗口4425，Root唯一公开CUA输入。Continue42→Local→Flat，正式加号固定475×282 m→帮助弹层 `>`／`^` 各一次→关闭弹层，记录地形／玩家／基地向左下移动，范围／北向／X36Z54基地及追踪1/64保持；Esc关闭→无游戏移动→Tab加小地图重开，Flat、475×282 m和平移视口保持；“回到玩家”恢复居中与594×353 m，对照同会话002居中基线。准确窗口原图沿用 `tools/macos_window_evidence.py --window-id <实际主窗口> --output <新PNG>`，五张2560×1496原图、24存档、原日志与Root输入记录见 `v06d/ordinary-ui-r3/session-r1.json`；[独立只读复核](../../build/visual-experience-polish-20260928/v06d/ordinary-ui-r3/independent-review-r1.json)86／86通过：003／004与002／005地图内区[196,370,1790,1315]各1,506,330个RGB像素完全相同；001→003有限内区最佳位移[-108,+108]px、相等98.2606%／通道MAE0.31065，不称全图相同，两图非相邻抓图且侧栏细微RGB差异保留。八地图FNV有效，42原hmap／5329格／Home1／tracked1与位姿保持，meta仅时间字段变化；另两世界各六文件字节保持，24存档live／copy／记录、r2冻结40、普通初始137及五源码身份保持。保存并退出有08:24:54完整Ogre关闭，Root pgrep query1／空，native自身退出码不可得。只关闭正式按钮pan、同世界同会话无移动zoom／pan重开及center复位；右拖、关闭—移动—重开、连续路线、水岸编辑、切世界／跨进程视口保持不在该证明内，完整V10与Goal仍Doing。 |
| V06c 恢复态实际存档重开 | 已完成对应配置正常构建后，`HELLOMINE3D_SHORE_REOPEN_CAPTURE=<已完成新capture.json> python3 -B tools/tests/prepare_fern_world_export.py --source tools/tests/shore_world_reopen_test.cpp --configuration Release --output <全新helper目录> --run --timeout 60`；仅打开该capture自己的save，不使用用户或旧存档。标准／兼容两套各58检查、2次公开World构造，production codecs先严格核对已改目标chunk／Water64与63 metadata／库存及持久canonical历史，移除强制状态后公开World重开→save→再重开；observe前归档正确，400格不增。目标保存文件必须存在；未改procedural邻块可生成，运行时仍9 resident。首轮误要求9保存文件的正常exit1失败保留，非丢存档。无Ogre窗口／更新tick／命令／新改块，不能代称编辑态保存、普通UI或GPU重开。 |
| V10j 小地图粗观察同列刷新 | 正常 Debug／Release 客户端构建、当前 UI SHA 及独立源码复核；64／128／256 m 对应 step1／2／4，仅 step4 刷新既有精确同列，回复长度匹配后才索引。既有 V10i HUD44／地图88仅按未变的 header／test／脚本 SHA 复用，不冒称新 UI 或真实 World 编辑测试。独立新 Release 的隐藏 HUD256／平面页各一原图、direct 正常退出，证明当前渲染启动；普通采细列→实际编辑→256 m 重观察→重开选点及邻列对照仍待验。预算、容量和存档版本不变。见[执行记录](../reports/visual-experience-polish-execution-2026-09-28.md)。 |
| V10i 平面粗层同列刷新细历史 | 正常 Debug／Release 客户端及 `bash scripts/verify_hud_interaction.sh Debug <新目录>`／Release：HUD 各44、地图各88。实际生产 helper 队列配合合成地表回复：旧190检查／同列冲突／正常 exit3，提案193／无冲突／exit0；提案纯回归86与最终正式88分记。检查精确同列、相邻细节、禁止新增槽／页、未知保持、已知空列、无变化回复、锁忙、负坐标、容量0和LRU；不扩大195列／30Hz预算。Xcode工具链、SDKROOT和CPLUS_INCLUDE_PATH使用匹配SDK；最初缺标准头失败保留。普通菜单创建／保存的seed42世界在新Release中真实重开，基地名称、坐标、追踪原图通过；人工操作介入后的HUD截图不证明平移重开，连续移动、真实改块像素复现、切世界和step4小地图当批待验，step4后续接线见V10j。HUD辅助UI哈希的显式括号差异由身份桥接说明，最终UI由正常构建覆盖。见[执行记录](../reports/visual-experience-polish-execution-2026-09-28.md)。 |
| V10h 暂停长通知的新反馈滚动 | 正常双配置客户端及 `bash scripts/verify_hud_interaction.sh Debug <新目录>`／Release：HUD各44、地图各74通过。`python3 -B tools/capture_visual_macos.py --app <独立候选.app> --output <新目录> --pause-notifications --render-distance 1 --visual-detail standard --launch-method direct --scene forest --perspective first --pixel-ratio 2 --width 640 --height 480 --ui-scale 1.25 --locale zh-CN`，英文换en-US及另一新目录；仅隐藏全新save／catalogue／output、无其他夹具。十二实际后端帧／三状态事件，检查新消息首帧、下一帧、同文本重发、手动滚动、字幕提交／刷新／到期、四按钮及实际时钟。`python3 -B tools/tests/pause_notification_capture_oracle.py <pause-notifications目录> --output <新报告>`：旧Before414检查／57行为FAIL／18旧回调归属OPEN；After双语各648／0FAIL／窄OPEN0、117输入各保持。CPU正反例25／0；原字形位置／UV／纹理绑定、完整clip及原PNG未遮挡前景通过，不能代称独立字体Alpha采样、OCR或普通输入。既有假存档正常拒绝exit1／无signal，主菜单两图与前版相同；宽范围scope_open四项保留，完整V10仍Doing。见[执行记录](../reports/visual-experience-polish-execution-2026-09-28.md)。 |
| 第一人称动作、命中/受击、粒子或镜头反馈 | 判定时刻与表现解耦测试、数量/持续时间上限、关闭回退、HUD/准星截图和 AI 多帧/视频观察；镜头效果必须可调或可关，人类舒适度不声明。 |
| 第一人称手臂与抓握表现 | `bash scripts/verify_player_hand_presentation.sh Debug` / `Release`；受影响客户端双配置、P11A/P11B/ITEM_VISUAL 定向回归、空手/图标/方块及昼夜/窄窗口/面板隐藏截图、三档反馈连续帧和相关三轮性能。诊断采集夹具不替代正常输入。 |
| 玩家工具阶段与朝向精修 | 双配置 `verify_player_hand_presentation.sh`／`verify_player_avatar_presentation.sh`：全动作连续性、预备／下挥、使用／进食区分、前向手掌、刚性握持、Off及Reduced缩放，反向工作臂／合并动作／取消预备故障负例；P11A／P11B／ITEM_VISUAL、受影响双配置客户端及Release完整世界。实际诊断采挖多帧与普通输入分开，不改变攻击和消耗时序，见[玩家合同](../contracts/adventure-player-avatar-contract-v1.md)。 |
| 玩家四向步态混合 | 双配置 `verify_player_avatar_presentation.sh`：四向脚端／旋转等价、对角与速度归一化、无效速度、档位及工具全轴抑制；独立盒角点验证步态／换向／停止后的脚底和腿分离，30／60 Hz 收敛；全部前进、取消脚底修正、只抑制X故障负例。双配置客户端及P11A／P11B／ITEM_VISUAL、Release完整世界；隐藏四向原图单列为诊断，真实连续输入／坡面／近墙仍须正常操作证明。 |
| 可持久化玩家视角设置 | Debug／Release `HELLOMINE3D_WORLD_SMOKE_FOCUS=CAMERA_SETTINGS`；检查 settings v11 默认、v0–v10 迁移、`first|third` 往返、缺失／未知／重复／旧版本越界拒绝、设置会话取消／默认／无需重启、双语键一致性；受影响客户端双配置编译。F5 原子保存、第一／第三人称实际切换及近墙回退另随 B8 运行时接线验收。 |
| V10p 设置失败与制作结果反馈 | macOS x86_64 正常客户端 Debug／Release 构建通过；`CAMERA_SETTINGS` 各95／0、`P11A` 各159／0，覆盖结构化问题／旧诊断及顺序、失败计划与Cancel、28错误键双语守卫及原子保存。ResourcePackSmoke 两配置各201／0，资产检查单次91／0；完整WorldRuntime仅Release2896／0。`bash scripts/verify_crafting_result_feedback.sh Debug`／`Release` 各80／0，使用真实配方／库存／制作会话核对实时preview与4秒独立结果、失败原子性及暂停／清空。正常双语反馈滚动、固定按钮和制作点击验收单独记账，见[执行记录](../reports/visual-experience-polish-execution-2026-09-28.md)V10p。 |
| V06i 水面共享边连续观察 | 正常 Debug／Release构建；显式新隐藏seed42／terrain31，原两Water至少10秒，每帧native绑定／六linked uniform／ROI及三raw checkpoint。最终容量修复后标准351帧、兼容361帧的独立消费各1439／1479 PASS，包含真实World四列水深；原序列362／350帧的深度消费者r2为1483／1435保持；五实际副本均先通过重算SHA再按共享UV／VAO／丢帧／WVP／共同错depth拒绝或OPEN。复制前packed2MiB门槛的真实方法抽取CPU probe20项／入口独审16项通过，原r1报告1480／1432保持，边界14项和CLI40项仅结构／mock范围。旧native各595／默认关闭各208实际回归；显式shore版本31只断言新保存，省略仍期待30。41组独立实物复核，查看6全窗／32ROI，无明显裂缝或回绕跳变，微亮点和浓雾保留。菜单启动正常0退出，不计普通输入；原shader／生成／保存保持，完整无闪点、四水域、GPU位移输出、逐对象像素和普通涉水仍OPEN。见[合同](../contracts/water-seam-continuity-observation-contract-v1.md)及执行记录V06i。 |
| V07g 反馈关闭时的玩家基础动作 | 正常 Debug／Release 客户端与 `bash scripts/verify_player_avatar_presentation.sh <配置> <新目录>`，各501/501；保留原显式静态Off／Reduced／Full检查，新增正常反馈关闭的八向完整纯值周期、独立腿盒支撑／分离、反馈污染、上升／下落、停止／暂停、30/120Hz及快照保持。隔离头文件仅将LocomotionOnly幅度置零，编译0后501/9、exit1，生产文件保持。正常配置Off经Bootstrap映射LocomotionOnly；标准前进与兼容横移实际隐藏渲染均Off／third／FOV120，旧包并腿站姿→新包迈腿摆臂；每组6PNG、4distinct，重复flush及非完整周期如实保留。渲染快照夹具不移动Player，不关闭普通连续输入／跳跃／坡面／近墙。见[合同](../contracts/adventure-player-avatar-contract-v1.md)及[执行记录](../reports/visual-experience-polish-execution-2026-09-28.md)。 |
| V07h 交替脚步与移速步频 | 正常双配置客户端及上述avatar脚本各802/802。原501项保持，新增独立八角点的八向交替离地／支撑／分离、横移相位、档位、暂停／停止／换向、30/120Hz及有界时钟；零lift／旧线性慢速clock／旧8.2周期／侧步错误cos负例编译0后均正常exit1。标准前进与兼容横移使用Off、third、FOV90和此前冻结陆地机位，各八原PNG；仅标准前进有本批同条件旧包对照。普通持续输入、坡面与世界脚掌无滑移不由快照渲染证明。见玩家合同第11节及执行记录V07h。 |
| V11d 普通第三人称单跳 | 复用f924／Release6cf正常包，无fixture，公开菜单创建seed42／terrain31、F5第三人称／反馈Off。实际Space wrapper UTC区间由80帧原窗口序列覆盖，独立查看起跳／臂腿轮廓变化／落地，12项及168PNG完整性、SHA／CRC／时间链通过，限定一次原地跳跃PASS。W单次与30次短按有回执但原图及前后保存未观察位移；无hold／dwell证据，连续行走／完整周期／坡面／近墙仍OPEN，不从短按推定产品输入缺陷。本次仅新增验收与文档，不重复未改运行时构建。 |
| 生物关节与步态表现 | `bash scripts/verify_enemy_articulation.sh Debug` / `Release`；方向/传送/固定关节/头口连接、颈部截面体积、30/60/120 fps 姿态过渡、暂停/死亡复位、真实展示比例/时序与故障负例，客户端双配置和 P11E 回归，静态前后图、具有不同时间点的连续行走/战斗姿态、相关三轮性能。新增部件还需检查 GPU 角色编号、昼夜及回退。诊断展示不代替正常战斗。 |
| 生物表面与面部表现 | `tools/validate_actor_shader_macos.cpp` 运行生产普通/阴影 shader，检查原型分区、局部坐标、夜间提示、雾遮挡和关闭回退；旧 shader 负例、双配置客户端/资源检查、昼夜/中远距离连续画面及受影响三轮性能。正常战斗与人类审美单独记录。 |
| 投射物模型与朝向 | `bash scripts/verify_projectile_presentation.sh Debug` / `Release` 检查封闭外向网格、体积边界、速度正交基和非法半径；双配置客户端与 `PROJECTILE` 定向行为回归、生产生物 shader GPU 检查及旧实现负例，近/中/远、昼夜、关闭回退和连续帧，相关三轮性能。诊断快照不替代正常战斗，未改 World 步进时不声明运动插值改善。 |
| 方块选中表面、裂纹与世界碎屑 | P11B 定向与完整 WorldRuntime、模型/metadata/tile 映射、GPU 透明遮罩/阶段/遮挡检查、标准与兼容干净包多帧、取消与 Off 回退、相关性能；诊断夹具与正常输入分开记录。见[补充合同](../contracts/block-feedback-contract-v2.md)。 |
| 湿地草丛区域模型 | `bash scripts/verify_wetland_grass.sh Debug` / `Release`；双配置客户端、`WETLAND_GRASS` / `P11B`、WorldRuntime 与 MeshDirty，检查真实区块和反馈几何/材质/风摆一致、成熟状态、单格边界及自定义资源形状回退。`tools/validate_flora_shader_macos.cpp` 检查生产 GPU 的根部、茎穗接点、普通/阴影一致及故障负例；多 seed 昼夜/兼容画面、可见连续风摆和 Off/High 常驻/流送三轮性能。GPU 编译需 `-Isrc/external/glm`，完整命令见[本批记录](../reports/wetland-grass-presentation-r29-2026-09-19.md)。 |
| 目标、配方发现、探索奖励或资源经济 | 主线可达、输入输出守恒、重复奖励/一次性领取、保存重载和全部受影响迁移；脚本化 AI 记录可执行流程，无上下文 AI 盲玩只提供可理解性代理。 |
| 冒险前期目标定义迁移 | `ADVENTURE_PROGRESS` 双配置：早期食物事件、无重开熔炉、可选历史、Alpha 十位兼容、v1／v2／v3 完成／进度／配方发现迁移；资源解析与 Release 完整 WorldRuntime。B2b 同入口补当前库存／生命／冷却、三项优先级、世界时钟及保存恢复、双语改键／只读查询；双配置客户端编译。普通体验、三字号／紧凑窗口和同机三对性能在 B2 整合检查，不将每个定义用例拆成客户端启动。见[前期引导合同](../contracts/adventure-onboarding-contract-v1.md)。 |
| 资源、配方、声音或 shader | 资源清单/解析验证；缺失和非法引用必须明确失败。水波坐标变化用 `tools/validate_water_shader_macos.cpp` 执行实际 GLSL 的相邻区块位置/法线检查，命令见[水面修复记录](../reports/water-seam-fix-2026-09-12.md)。 |
| V03b 稀疏岩柱记路地形 | `bash scripts/verify_rock_landmark_planner.sh Debug`／`Release`：旧v1–v29冻结字段、三seed密集列、细湿地／沙丘和水影响、负坐标／极值、缓存与并行顺序、移除岩柱负例。双配置`ROCK_LANDMARK`检查完整跨块实际Stone连地、两格净空绕行、植物／树支撑、新世界出生资源、v29保存重开后首次生成未知远块、六套地点及三地下计划的真实通路／资源／反序字节，逐矿Air邻面／非矿支撑／六邻连地及全区煤铁数量，低头／封口／缺块／旧悬空／移除支撑负例；保留ADVENTURE_EXPLORE／ADVENTURE_SURVIVAL、Release完整世界和双配置客户端。生产v1–v29每版地表及chunk CSV与修改前逐字节比较；三seed真实高度前后原图和普通菜单出生／重进分记，普通连续路线不由诊断代替。见[合同](../contracts/rock-landmark-v30-contract-v1.md)。 |
| terrain atlas 或 HUD/手持图标 | `tools\validate_terrain_atlas.ps1`（暖野 macOS 使用 `python3 tools/validate_warm_texture_atlas.py`，含分面/图标映射）、确定性重建、Alpha/空 tile、block 分面、Material 坐标、资源包与隐藏固定截图。 |
| V01e 煤铁矿共享材质 | 图集／数组确定性重建、132语义／124空槽和83槽同物身份；独立正式源采样、固定矿槽13／14、不透明及来源检查。与冻结前版逐层比较全部7级mip，仅煤／铁两槽改变，其余254层保持；合法FNV／SHA重算后的颜色、末mip Alpha、来源／源哈希、等数量语义替换与图集／数组同步污染负例必须拒绝。双配置资源解析顺序执行，避免共用`bin/validation_runs/resource_packs`相互覆盖。标准／兼容和铁矿详情前后原图与普通采矿／手持／掉落／连续移动分记；纯资产变化复用源码身份一致的客户端，不冒充C++重建或世界全量重跑。见[材质源](../art-sources/visual-polish-20260928/README.md)与[执行记录](../reports/visual-experience-polish-execution-2026-09-28.md)。 |
| V01h 圆石／橡木板共享材质 | 保留旧83槽完整身份、来源与9源SHA，再精确加入`oak_planks`层21／`cobblestone`层23成为85槽、11源；正式原PNG独立16格采样、固定槽／不透明与4单图全7mip参考。`python3 tools/tests/visual_built_materials_test.py --baseline-facts <旧基线.json> --before-array <旧.hmt> --output <新报告.json>`独立核对两槽改变、其他254层所有mip及130未改语义记录保持，不import生产builder。图集／数组／报告重建字节一致，旧煤铁及新两材质的合法FNV／SHA污染、同数量替换、来源交换、atlas-array同步污染负例须拒绝；原5图集负例保持。双配置资源解析串行、资产清单；源码不改时复用身份一致的正式客户端，并分别拍摄圆石／木板冻结机位标准前后和兼容原图。固定日照诊断不关闭普通使用、完整远近／背光／夜间或连续移动。见[材质源](../art-sources/visual-polish-20260928/README.md)与[执行记录](../reports/visual-experience-polish-execution-2026-09-28.md)。 |
| V01f 手持／掉落动态图集路由 | 对应正常客户端构建后运行`bash scripts/verify_item_material_binding.sh Debug <仓库根> <新输出目录>`及Release；实际resolver／profile／BlockDatabase冻结、生产Player held／Actor drop与软件VBO，默认v2／合法v1 32列／23列各独立进程63／0，核对四选材＋OakBark分面、实际float floor格身份、重复／光照／root0及有效PNG Alpha挤出轮廓。原r2默认51／0与32列51／14保留；r3 Before默认63／12、32列63／26、23列63／20各含12条新增半像素工程断言，默认原语义失败为0，不称默认旧路由缺陷。导出原VBO／IBO byte及声明／stride／indexType／counts／world transform，编译`tools/validate_item_material_binding_macos.cpp`后以`<native helper> <仓库根> <软件模式输出目录> <新GPU目录>`执行生产terrain GLSL：Before默认480／0、32列240／240、23列240／144→After480／0＋240／0＋240／0，GL0／inputs保持；240是每面四像素的断言数，不是完整身份链。诊断上传／nearest基级不证明真实Ogre bootstrap TUS、ImGui／地图、图标GPU轮廓或普通使用。双配置资源201／0；同owner完整v2非16列probe由CONTRACT_FAIL exit3改为specific RESOURCE_REJECTED exit1，两正控0／705输入保持，合法v1不受拒绝。正常客户端first-party warning0、世界生成与actor领域／正式纹理／shader保持；实际原图、保护、包身份和日志version字段歧义另见[执行记录](../reports/visual-experience-polish-execution-2026-09-28.md)。完整身份链后续实际捕获见V01g；普通输入仍开放。 |
| V01g 实际客户端材质身份 | 正常Debug／Release构建后，用`python3 tools/capture_visual_macos.py --app <独立候选.app> --output <新目录> --material-identity --render-distance 1 --visual-detail standard --launch-method direct --scene forest --position "976.5 86 -3.5" --rotation "12 0 0" --pixel-ratio 2 --shadow off --post off`获取实际九帧；compatibility使用另一新目录。`python3 tools/tests/material_identity_capture_oracle.py --capture <standard/material-identity/index.json> --capture <compatibility/material-identity/index.json> --output <新报告.json>`核对四材料×世界／第一手持／第三手持／真实drop／快捷栏库存／立体地图×两模式，固定48去重；OakBark分面单列。实际resolver资源、bootstrap原生纹理各mip、pass/TUS、原缓冲中性重放及真实ImGui backend framebuffer；42独立源mip参考，不从生产builder导入。World必须实际uploaded revision与live／GpuResident匹配；每条fixture列的map query／height／block一致，正确记录不能掩盖错误记录。实际GL状态恢复与零GL错误、输入SHA、512MiB不可变观察载荷／九帧有界（index重写与Root原图另计）；单TU隔离UV错格／上传层交换负例须编译链接成功后由真实捕获oracle拒绝。纯CPU同列旧高度／错材质、部分像素／裁剪UV对抗独立记录。诊断冻结模拟和库存／方块注入不关闭普通输入、连续动态、远mip采样、Alpha轮廓或正常光照，完整Goal仍Doing。 |
| 岩层与沙面地质着色 | 当前冒险地貌去条带使用 `tools/validate_block_feedback_shader_macos.cpp <root> <新目录> <冻结旧条带资源根> --irregular-geology`，检查旧条带负例、相邻像素对比下降但宽变化保留；其余历史模式保留原资源版本归属。 `tools/validate_block_feedback_shader_macos.cpp` 以可选旧包资源根参数比较实际 GLSL，检查 atlas/array、普通/阴影、昼夜、时间不变、区块原点、负坐标接缝、关闭回退、远处细节衰减及旧版/移除衰减负例；双配置资源检查、多 seed 原图与相机连续帧、相关三轮常驻/流送性能。仅改 fragment 时可复用源码身份一致的客户端，不冒充 C++ 重建或正常行进。命令及范围见[地质材质记录](../reports/geology-materials-r30-2026-09-19.md)。 |
| 生态植被颜色过渡 | `bash scripts/verify_terrain_ecology_colour.sh Debug` / `Release` 检查有界查询、跨区块/负坐标连续和极值；`V10B3` 真实快照及 greedy 内部颜色重建、`WETLAND_GRASS` 反馈/风摆和完整 WorldRuntime；双配置客户端/资源、生产 GPU 四路径及旧 shader/取消滤波/错误合并负例，配对原图、连续帧和相关三轮帧耗/网格规模。见[本批记录](../reports/ecology-colour-transitions-r33-2026-09-20.md)。 |
| V09c 地下有限可见边界背景 | 为低RD洞室天空缺口接入背景，仅对Near外边界owned `Air && sunlight==0`置位，天光口／内侧Near洞口排除，普通地形和实体覆盖背景。缓存最多2048面，首快照每次扫描最多8面／2048格，GPU每帧最多激活8面；确认快照与renderer均传`false`，不重复采样或上传新面，dirty删除退化写入另计。revision＋incarnation失效、零面缓存、重载／reset／切RD及容量由双配置`CAVE_BOUNDARY`各35/35验证；双配置构建通过，资源各198项、Release完整2709项通过。六个隔离production故障均成功编译／链接，再由真实35项检查拒绝：忽略incarnation／revision／天光各双配置一例。生产GLSL19项通过，renderer最终双配置各53分项PASS／1旧Ogre启动基线FAIL，背景阶段GL0；九组最终前后原图已查。V11b以新生产库另验双配置各54／0、GL0，不回写V09c历史FAIL；普通路线`NOT_RUN`，V09／Goal保持`Doing`。见[合同](../contracts/cave-boundary-background-contract-v1.md)及[执行记录](../reports/visual-experience-polish-execution-2026-09-28.md)。 |
| V11b GL3Plus纹理格式／alpha传输 | 先正常构建本配置生产客户端／Engine，再运行`bash scripts/verify_gl3plus_texture_macos.sh Debug <仓库根> <新Debug输出目录> --run`及对应Release命令；不带`--run`仅编译／链接。实际手动纹理分配、完整／裁剪／pitch／缩放／mip／texture blit／自动mip／3D单层，经独立RGBA32F shader核对A8 alpha、typed565实际精度及八种有符号SNORM。双配置纹理各68／0、GL0，原严格启动各1／0、生产洞穴各54／0、GL0；冻结旧源码fresh baseline1／1、GL0x500及V09c历史FAIL保持。隔离Release归档24步exit0、198inputs保持，每故障仅改1代码对象：B5 68／1、无swizzle68／9、A8错标L8 68／22且texture GL2、raw prefix68／8；316旧header warning保留。选定TextureBuffer TU＋同fixture的ASan正控68／0，旧源depth故障READ32／16B堆源：r1 SIGABRT／系统异常框保留，r2同二进制`abort_on_error=0:exitcode=1:halt_on_error=1`正常exit1仍检出；不声明NEDPOOL内部staging边界或全Engine内存安全。提交后包135管理项／2953源码（GL3Plus71）逐项哈希，三组原图与普通输入分记；CUA r2 UNKNOWN／NOT_RUN，不关闭普通路线或整项Goal。见[执行记录](../reports/visual-experience-polish-execution-2026-09-28.md)。 |
| V09b 地下形态与铺地 | 双配置 `UNDERGROUND_POLISH` 检查实际生成的路线、拱顶、木架面连接、连续湿地、九矿外露、资源不变和保存重开；`ADVENTURE_UNDERGROUND`／`LANDMARK_POLISH`／容器回归，Release完整世界及双配置客户端。`UNDERGROUND_POLISH_GEOMETRY` 用实际section快照比较v27/v28网格；旧v1–v27生产T0保持，v28地表采样不变。固定三seed当前计划和同条件前后原图，普通探索另记，见[合同](../contracts/underground-polish-v28-contract-v1.md)。 |
| V09a 地下局部光源 | 双配置 `LOCAL_LIGHT_RENDER`、既有顶点／光源／网格／水域／地下回归、MeshDirty、玩家手持表现和资源；生产 terrain／flora／water GPU 昼夜／遮挡／来源正反例；冻结三种子几何及实际缓冲增量、固定地下／洞口／户外原图。V09a的40字节格式及后续V02b的44字节格式必须按实际版本计入指标；严格性能延期，普通探索不由诊断替代。见[合同](../contracts/local-light-render-polish-contract-v1.md)。 |
| 顶点格式、网格、光照或 AO | 确定性角落/边界夹具、MeshDirty、隐藏固定截图、既有 schema 3 顶点/索引/构建/驻留字段的补充比较和相关 Q1。仅改每顶点值时不得误报为顶点格式升级。 |
| 同列 section 合并绘制 | `RENDER_BATCH` 双配置聚焦：全部属性／世界坐标／索引保持、负坐标与整数极值、非法和空数据、替换／移除、透明／多 pass／自定义程序回退；完整 WorldRuntime、双配置客户端、真实改块／水岸／林地／雪山／阴影及兼容路径多帧，三轮静止与流送性能。合并不改变实际几何量，驻留对象数不能当作 draw 次数。 |
| 雾、天空、云、阴影或后处理 | shader 正反例、关闭回退、固定昼夜截图、窗口缩放/切世界清理、各图形档性能和 AI/开发者视觉检查；动态项必须用多帧、视频或连续窗口。 |
| V02b 有限视距与自然树退场 | 双配置TREE_ROOT_TAG／MESH_INPUT／RENDER_BATCH／LOCAL_LIGHT_RENDER／V10B3／BANK_MESH／WATER_DEPTH／WATER_MOTION及MeshDirty、客户端；Release完整世界和v1–v29地表／块摘要保持。生产terrain／flora／caster GPU检查根一致、近面、连续性、逻辑中心／地下、RD1交互保护及故障负例；actor／water／PCF旧回归、资源接口与资产。`tools/tests/manual_mesh_root_binding_test.cpp`双配置以真实Ogre软件VBO核对手工mesh显式root0、原UV／光照及rebuild，绑定模型不冒充GL draw。固定快照8KiB、44字节顶点及实际几何增幅实测；原／近机位、RD1／8／12、兼容、地下和扫视原图，公开切档与保存重进分记。见[合同](../contracts/view-distance-transition-contract-v1.md)。 |
| V02c Fern 原网格与实际客户端程序 | 正常两配置后运行 `python3 -B tools/tests/prepare_fern_world_export.py --configuration Debug --output <新目录> --run` 及 Release；默认仅编译并准备命令。真实 World／mesher／公开软件缓冲各991／0，新实例布局须重编 fixture。冻结原44B/u32生产GLSL重放两模式各162项／4严格候选FAIL，其余0FAIL／GL0；保留 repeatV1 新候选失败，不改UV或放宽既有根位移1e-5门槛。实际最大根2.861e-6、叶片0.008636；错误包裹时钟编译链接成功后增加14行为FAIL、正常exit1。`capture_visual_macos.py --app <独立候选.app> --output <新目录> --fern-wind --render-distance 1 --visual-detail standard --launch-method direct --scene forest --perspective first --pixel-ratio 2`，compatibility另一新目录。固定自然源、四阶段 Off0／Off1／High0／High1，实际全原缓冲／attached program／native uniforms／primitive query／RGBA／PNG；`tools/tests/flora_wind_capture_oracle.py --capture <standard/fern-wind/index.json> --capture <compatibility/fern-wind/index.json> --world-reference <natural-locate.json> --output <新报告>` 最终13／0，303输入保持，独立另46检查。每源24V／36I、六根及GrassTop独立重建；每全操作192V／288I、query96。首次4.1非法indexed sampler查询exit1／GL1280保留，改active-unit GetIntegerv后八帧GL0；既有目录拒绝正常exit1／marker保持。incarnation、内部VAO取数、像素归因、连续动态／普通输入保持OPEN，不关闭完整V02。见[合同](../contracts/fern-wind-client-observation-contract-v1.md)。 |
| V05 接触阴影 | `tools/validate_shadow_filter_macos.cpp` 执行terrain／actor生产PCF，覆盖两档8厘米遮挡、斜坡跨纹素移动、移除接收平面修正负例及原连续滤波／回退；V05b补低太阳角度小投影、远原点与倾斜足迹48项，冻结旧shader须失败；完整actor／terrain GPU、资源双配置（每个阴影program缺绑定／错矩阵／错索引及旧fragment负例）、双配置客户端、雪地／林地／岩坡标准与兼容实景。AO、世界和档位语义未改时不重跑完整世界门禁；普通动态和其他场景仍按Goal补齐。 |
| 冒险前期危险与种植 | `ADVENTURE_SURVIVAL` 双配置：夜间／跨日、候选距离、实际光照与树顶拒绝、加载数不变、三种子自然种子与土层、无战斗领域种植；D3／D5／D6／G6、阶段指标、难度与食物，Release 完整世界，双配置客户端／资源。正常输入及同负荷性能在 B2 整合，详见[前期引导合同](../contracts/adventure-onboarding-contract-v1.md)。 |
| 冒险动物生态 | `ADVENTURE_WILDLIFE` 双配置：三生态自然出现、真实移动／停留／觅食／受惊、各角列真实干燥非叶支持、最高实际接触面／一级混合支持差、避水与查询预算（不要求四足同时贴平面）、原敌人 D3、区块卸载／重开再生、击杀无掉落且不计敌人目标；Release 完整世界、双配置客户端。B3b 同入口检查三类有界模型与状态动作，`tools/validate_actor_shader_macos.cpp` 验证生产普通／阴影 GPU、像素标记、关闭回退、雾和原生物／投射物回归，双配置资源。三生态普通画面、密集流送与配对性能集中实机验收，详见[冒险动物合同](../contracts/adventure-wildlife-contract-v1.md)。 |
| 动物动作精修 | `bash scripts/verify_wildlife_presentation.sh Debug`／`Release` 检查最短转向、30／120Hz恒定目标、1200帧活动切换、关节／耳根连接、足盒角点支撑、停走、暂停、传送、强度关闭与群体错相；故障版本检出瞬切／腿根断开／同步，修复前脚尖回归保留。复核双配置 `ADVENTURE_WILDLIFE`、Release完整世界与双配置客户端；实际标准／兼容、动画关闭及自然栖息画面分开记账，不以展示模型代替普通遭遇，见[精修合同](../contracts/wildlife-motion-polish-contract-v1.md)。 |
| V07d 动物真实移动段与 Ogre 变换 | 双配置纯动作各74/74；`ADVENTURE_WILDLIFE`用可选环境变量`HELLOMINE3D_WILDLIFE_VISUAL_ORACLE=<新文件>`导出真实World／Actor快照及最近8段，含导出检查各141/141；Release完整世界2719/2719。正常客户端构建后运行`bash scripts/verify_wildlife_renderer.sh Debug <仓库根> <Debug导出文件> <新输出目录>`（Release替换配置及导出文件），链接生产对象／库，双配置各500/500：三物种八方向及最短转向、兔升／降台阶与羊五段垂直下落在三档强度／四活动／30与120Hz的连续路径、暂停及实际软件VBO八角点；双精度15轴SAT比较当前真实段同姿态端点包络，允许已有模型外扩。两个隔离Release生产源码故障均编译／链接成功，再分别由朝向和真实台阶路径／新增穿入断言拒绝原yaw符号与旧直线插值。正常20Hz跨一格台阶仍受步长／体宽限制，待V07e修复；当前0.20秒领域台阶、无窗口变换与gallery均不计普通路线或实际台阶GPU验收。见[合同](../contracts/wildlife-motion-polish-contract-v1.md)及[执行记录](../reports/visual-experience-polish-execution-2026-09-28.md)。 |
| V07e 正常小步跨阶与局部退让 | 原正常Xcode客户端／World双配置构建无first-party警告；最终`ADVENTURE_WILDLIFE`同时传`HELLOMINE3D_WILDLIFE_VISUAL_ORACLE=<新文件>`和`HELLOMINE3D_WILDLIFE_CADENCE_ORACLE=<新文件>`各252/252（含4导出），六条真实三物种0.05秒游荡／逃离上下阶，以及缺角／水叶／低顶／未知列／负区块／真实墙角与Wander恢复、12／24只公平／48查询。Release完整2824/2824为最终运行时，之后仅新增4条Wander测试并聚焦重验，不称执行2828完整项。`bash scripts/verify_wildlife_renderer.sh <Debug或Release> <仓库根> <原导出> <新输出目录> <cadence导出>`双配置各830/830；真实到达流在30/120Hz×三档动画检查独立L折线、发布范围、历史淘汰、暂停及实际软件VBO/15轴SAT。三个生产源码故障编译／链接后分别拒绝旧同高支撑、缺角放行和固定同侧退让；前两以冻结r2正控237/2保留已知历史失败，仅称指定PASS→FAIL有效。真实World自然草甸／湿地及兼容原图是诊断回归，森林未确认兔；不替代正常输入或连续GPU跨阶，CUA当批超时NOT_RUN。见[合同](../contracts/adventure-wildlife-contract-v1.md)及[执行记录](../reports/visual-experience-polish-execution-2026-09-28.md)。 |
| V07f 真实近墙／低顶相机 | 正常macOS x64同配置客户端构建后，`python3 -B tools/tests/prepare_camera_world_oracle.py --configuration Debug --output <新目录> --run`及Release；默认只编译并准备命令。实际World／Player／逻辑相机、生产OgreCamera／Rig／软件PlayerRenderer，两配置各5979／0、208样本，独立双精度13轴SAT核对整个near quad、PlayerBox、缓存／可见性、权威与修订保持；nominal与render分离、dt0即时缩小、开放／第三恢复、nominal0.2不反馈支持、128额外／2048总预算。实际GL3Plus未初始化仅投影／能力枚举，无context或draw。`python3 tools/capture_visual_macos.py --app <独立候选.app> --output <新目录> --camera-diagnostics --render-distance 1 --visual-detail standard --launch-method direct --fov 120 --width 1920 --height 640 --pixel-ratio 2 --perspective third`，compatibility另一新目录。六阶段actualWorld current/upload/GpuResident后采集，实际六GL0和3840×1280PNG；`python3 -B tools/tests/camera_capture_oracle.py --capture <camera-diagnostics/index.json> --console-log <原client.log> --require-nominal --report <新报告.json>`各123／0。独立inverse(P_RS*view)／GLz=-1与实际corners、两三角形SAT／真实完整驻留包络、九对象队列与prebind WVP、PNGCRC／SHA、nominal恢复；旧Before缺新字段仅OPEN，真实111／2SATFAIL/exit1保持，不能删负例或宽放容差。空RenderSystem独立测试SIGSEGV及早期夹具45秒exit1保留。新入口拒绝既有假存档exit1／无信号／字节保持；普通主菜单四PNG相同。原GPU几何、手部像素、atomic／ABA、实际bound uniforms、普通输入／连续舒适度仍OPEN，V07整体Doing；证据见[执行记录](../reports/visual-experience-polish-execution-2026-09-28.md)。 |
| 冒险目的地布局 | `ADVENTURE_LANDMARK` 双配置：terrain v20 六布局在实际种子选中、v19 候选及奖励不变、生产区块逐格与蓝图一致、正反区块顺序、入口与矿物／核心／箱子、箱子 payload、v20 创建编辑重开；同入口保留 E8 v14 回归。Release 完整世界、v1–v19 保存／生成兼容及客户端双配置；三类普通进入、接近线索、区域建造材料和三对性能在 B4 整合。见[目的地合同](../contracts/adventure-landmarks-v20-contract-v1.md)。 |
| 冒险区域建造配方 | `ADVENTURE_MATERIAL` 双配置验证区域地表采集／放置及林地储物、河岸窑炉、高地破碎三套真实工作台制作／放置／保存重开；`HelloMine3DRecipeSmoke` 复核旧配方、经济守恒及新配方，双配置资源包与 `check_assets.sh`。普通地区采集、配方发现、地点建筑示例及带回基地使用在 B4 集中实机验收，见[区域建造配方合同](../contracts/adventure-regional-build-recipes-contract-v1.md)。 |
| 冒险目的地接近线索 | `ADVENTURE_APPROACH` 双配置检查 terrain v21 在冻结种子真实林地／河岸／高地地点的地面、净空、侧边标记、正反区块顺序、v20 建筑与奖励不变、v21 编辑保存重开；`ADVENTURE_LANDMARK` 复核旧 v20 身份，Release 完整世界和双配置客户端编译。普通发现／进入、画面及三对性能在 B4 集中实机验收，见[接近线索合同](../contracts/adventure-landmark-approach-v21-contract-v1.md)。 |
| 冒险区域工位 | `ADVENTURE_WORKSHOP` 双配置检查 terrain v22 三个冻结区域的真实地点、3×3 建筑材料、净空、箱子／熔炉／破碎机空实体及使用、v21 建筑与奖励、正反区块顺序、显式 v22 保存兼容及拆除不重生；旧 v21／v20 聚焦和 Release 完整世界回归、双配置客户端编译。普通探索／建造、画面与同机三对性能仍由 B4 集中验收，见[区域工位合同](../contracts/adventure-regional-workshops-v22-contract-v1.md)。 |
| 地点轮廓与内部路线精修 | `LANDMARK_POLISH` 双配置：三seed各六真实布局的区块投影与反序、六边占地与奖励、结构连通及增量预算、两格净空到交互点／四矿／瞭望台、棚内地坪、封入口及低净空负例、真实箱子拆除与玩家改块保存重开。复核 `ADVENTURE_LANDMARK`／`ADVENTURE_APPROACH`／`ADVENTURE_WORKSHOP`／`ADVENTURE_EXPLORE`／`ADVENTURE_SURVIVAL`／`WATERBANK_POLISH`、Release完整世界及双配置客户端；旧v1–v26生产T0逐字节保持、v26地点实际块与payload相同、v27地表列不变。同机位原图、普通发现进入和收获使用分别记账，见[地点精修合同](../contracts/landmark-polish-v27-contract-v1.md)。 |
| 区域工位视线精修 | `WORKSHOP_COURTYARD`双配置检查三类实际工位同址、下层不变／只删八块／上层净空与旧棚顶负例、主建筑及奖励、反序生成、空机器和打开／保存／拆除重开；保留v22专项及地点／接近／地下回归、Release完整世界和双配置客户端。旧v1–v28 T0及v28地点调查保持、v29地表不变；实景前后图与普通玩法分开，见[合同](../contracts/workshop-courtyard-v29-contract-v1.md)。 |
| V08c 高地工位前角遮挡 | `WORKSHOP_SIGHTLINE` Debug／Release各28项：严格默认31／出生／保存重开，双镜像及三实际区域的完整块id／metadata／实体payload仅高地一格Stone→Air，反序一致、真实Crusher加工领取、玩家角位改块及拆机保存不回填、旧v30未知工位仍按30。保留显式22／29工位及冻结30岩柱专项，Release完整2864项和正常双配置客户端；旧v1–v30 T0两CSV相同，新31地表除版本列外保持。当前斜向／正前方原图改善，另一侧仍局部遮挡；固定机位不关闭普通发现／进入／使用。见[合同](../contracts/highland-workshop-sightline-v31-contract-v1.md)。 |
| 冒险地下空间 | `ADVENTURE_UNDERGROUND` 双配置检查 terrain v23 只在 v22 后追加、三组冻结 v22 洞口字段和实际区块哈希、冻结 v23 保存身份、当前默认版本与未知未来版本拒绝；核对完整逐列地表证书、旧入口无条件避让、真实三维冲突仲裁，并覆盖高于 `surface - 5` 的外来 v22 入口已雕刻地板不得被回填；证明生产 Mountain 预筛与无预筛公开计划等价，且公开 sampler 顺序及 v1–v22 路径不变；检查大洞室／裂隙／浅水池／两类目的地的生产方块、跨区块正逆序与独立并行、缓存容量压力、写入与水体预算、三格干路往返、真实箱子 payload／保存／领取不重生、资源数量、卸载重载及入口个人标记保存追踪。v1–v22 全版本另从修改前基线和最终候选运行生产 `T0-SURVEY`，逐字节核对 `samples.csv`／`chunks.csv`。随后复核 `P11-2`、`ADVENTURE_WATER`、`ADVENTURE_ECOLOGY`、`ADVENTURE_EXPLORE`、`ADVENTURE_LANDMARK`、`ADVENTURE_APPROACH`、`ADVENTURE_WORKSHOP`、`EXPLORATION_MAP`、`LIGHT_BOUNDARY`，运行 Release 完整 WorldRuntime 和双配置客户端。三个 seed 的普通进出返程、地下画面、动态流送及同机 v22／v23 三对性能单独取证；工程聚焦通过不代表 B7 完成，见[地下空间合同](../contracts/adventure-underground-contract-v1.md)。 |
| 持久探索地图 | B5a／B5b 运行 `bash scripts/verify_exploration_atlas.sh` 双配置，覆盖未知、已知空列、坐标极值、刷新、满额保持历史、文件身份／校验、事务故障和损坏回退；B5c 同脚本检查 UTF-8、稳定 ID、单一基地／追踪、修改删除和容量、北向八方位与极值距离、已知任务锚点阶段门槛，以及文件 v1/v2/v3→v4 读取、标记／发现与任务绑定分别往返、错世界隔离和篡改负例。缩放总览另检查已知格汇总、代表样本坐标、页接缝与边界输入。备份／恢复运行 `HelloMine3DWorldBackupSmoke`，世界运行用 `EXPLORATION_MAP` 双配置检查真实采样、远区拒绝、World 标记命令、真实放置／拆除、多界石导航、远区保存重开、损坏与错世界隔离、隔离失败／保存重试／满额状态，并编译双配置客户端、运行 Release 完整世界回归。平面图与标记页工程检查双配置客户端、资源包、`check_assets.sh` 及双语键引用，不凭无窗口编译宣称界面可用；集中窗口核查平面／立体切换、缩放取样与鼠标标记、目的地真值、双语与两窗口、正常返回路线及同机三对性能。存储接线通过不代表 B5 完成，见[探索地图合同](../contracts/adventure-exploration-map-contract-v1.md)。 |
| 世界列表地形预览 | Debug／Release 运行纯 `WorldPreviewStore` 往返与负例，固定 49×25、8 m／格、4096 B 上限、未知保持未知、身份及 source revision；`HelloMine3DWorldCatalogueSmoke` 证明枚举不读预览且四类坏缓存不破坏目录；`EXPLORATION_MAP` 证明真实 Atlas 保存后生成、重开、软失败与恢复后陈旧失效；`HelloMine3DStorageTransactionSmoke` 覆盖原子发布／旧文件保留／候选隔离。双配置客户端和双语资源通过后，集中窗口核查无预览回退、选中切换、真实地形一致性、640×480／1280×720 与 0.85／1.0／1.25 字号，并以世界数量和最大缓存做菜单选中延迟及同机三对性能；无窗口测试不宣称 UI 可用。 |
| 冒险天空日月与遮挡 | `tools/validate_sky_shader_macos.cpp` 文件头构建后以 `<候选 shader> <冻结旧 shader> <新输出目录> --celestial` 执行生产 GPU：日月特征、背面、正午连续、云遮挡、旧版及移除遮挡负例、FS2 逐像素保持及既有云动态；双配置资源检查、FOV 90／120 昼夜实景、三档各三对性能。B1a 不关闭后续云形／区域氛围／阴影范围，见[冒险天空合同](../contracts/adventure-sky-contract-v1.md)。 |
| 冒险天空云团 | 同一 GPU 工具以 `--cloud-form` 检查 B1b 云形变化、清晰边缘与留白、正负坐标风移不变性及停止风移负例；保留日月／遮挡、60 Hz 漂移、云层穿越和 FS2 原版回退检查。林地／海岸／雪山、两 FOV 日月、连续诊断和适用三对性能，普通输入另记。B1d 用 `--natural-sky` 另检查圆形日月及错误方牌负例。B1e 用 `--atmospheric-clouds` 验证清空／薄缘／浓密层次、硬剪影与整幕雾负例；云高变化补双配置 `V10C`、资源与客户端编译。先用同一 GPU 进程出低地／雪山／高处探针图，再以一个独立工作客户端会话集中检查实景；历史硬边界指标不作为薄云的美术要求。 |
| 阴影稳定性 | `bash scripts/verify_directional_shadow.sh Debug` / `Release` 检查正午参考轴、光源平面纹素锚定与负坐标；生产滤波 `tools/validate_shadow_filter_macos.cpp` 的 GPU 连续性/遮挡/回退和旧版负例，完整 terrain/actor shader 回归、资源接口陈旧覆盖负例、客户端双配置、昼夜/移动多帧与各受影响图形档三轮性能。有限采样不声明全部场景零闪烁。 |
| 构建图或平台代码 | 当前批准平台的工程生成和受影响目标编译；macOS 只有被当批列入范围时才要求新原生证据。 |
| POSIX 渲染计时 | `bash scripts/verify_posix_timer.sh Debug` / `Release` 以实际 Timer 源码隔离注入墙钟回拨、前跳和暂停；不改系统时间。重建受影响 Ogre 依赖及客户端，再核查隐藏客户端的真实帧增量、模拟 tick 和正常配置恢复；短采样不代替三轮性能门槛。 |
| 隐藏诊断相机 | `bash scripts/verify_visual_camera_sweep.sh Debug` / `Release` 检查开关隔离、有限参数、位移/角度/时长边界与连续性、有界中间高程／端点一致／非法路径拒绝；客户端双配置构建、实际启动负例、开关关闭路径及原始连续帧。相机观察不证明玩家移动、碰撞、小地图位置刷新或正常玩法。 |
| 水线表层过渡 | `tools/validate_water_shader_macos.cpp` 执行生产 GLSL，覆盖浅/深水、细节开启/关闭共 8,004 个跨水线采样、合成颜色连续性与近裁剪覆盖；冻结旧 shader 反例、同机位连续帧及相关三轮性能。只改 fragment 时复用身份匹配的客户端，资源/实际 GPU 检查不冒充 C++ 重建或正常游泳验收。 |
| 相机水介质与远景衔接 | `V10C` 定向检查顶层水块内的深度过渡、表面/块边界连续性及昼夜雾色与地平线一致；双配置客户端构建，同机位水下/出水连续帧、空气及大气关闭回退、相关三轮性能。基础水介质独立于增强大气开关，关闭大气时仍检查昼夜水下与干燥岸边，保留云层/水面细节回退。端点亮带消失不能替代穿越过程检查。 |
| 冒险区域氛围 | `tools/validate_regional_atmosphere.cpp` 双配置纯检查采样预算／缓存／坐标／区域连续／权威与水介质，双配置客户端及 `V10C`；八生态与林地海岸雪山四时段、FOV 90／120、关闭回退、动态和生命周期检查，三档常驻／流送三对性能。见[冒险天空合同](../contracts/adventure-sky-contract-v1.md)。 |
| V04 连续宽云带精修 | 天空 GPU 工具 `--polished-clouds` 以当前修改前自然天空为基线，检查固定探针的连通浓云大小、旧宽带负例、日月及关闭回退逐像素保持，沿用遮挡／漂移／跨云层／风移正反例；资源双配置和当前实景。纯 shader 不重编整客户端；完整多生态昼夜矩阵在阶段整合，人工和严格配对性能按当前 Goal 延期。 |
| `AL-A0` 纯文档基线 | 逐项对照实际源码冻结模块/API/ownership/tick/snapshot；`git diff --check`、本地 Markdown 引用、World→Ogre 反向依赖检查和 VS2017 完整门禁。运行时代码/身份未变时引用既有正式 Q1/Q3，不重跑 1800 秒；无 OS Computer Use 时 `AI-08=NOT_RUN`。 |
| `AL-A1` World 责任地图 | `tools\validate_world_responsibility_map.ps1` 必须覆盖全部公开方法、匹配 public-surface hash 且无 stale/重复行；随后运行 VS2017 完整门禁。没有运行时行为变化时引用既有正式 Q1/Q3，`AI-01..AI-08` 保持 `NOT_RUN`。 |
| `AL-A2` Chunk Runtime 边界 | `tools\validate_chunk_runtime_boundary.ps1` + AL-A1 公开面门禁；VS2017/v141 Debug/Release 完整门禁；WorldRuntime 的 S0.5/M2/M6/M7/E5/S2.4 与 loader stress 必须通过。禁止新增 Residency 状态、改变 save/unload 转换或修改既有预算；AI 场景未执行时保持 `NOT_RUN`。 |
| `AL-A3` Simulation Runtime 边界 | `tools\validate_world_simulation_boundary.ps1` + AL-A1/A2 边界门禁；`HELLOMINE3D_WORLD_SMOKE_FOCUS=AL-A3` 和完整 WorldRuntime；VS2017/v141 Debug/Release 完整门禁。必须保持 8 phase 顺序、20 Hz context、caller-owned pause 和确定性；后续 A5 只能增加观察词汇，仍禁止 Scheduler/Registry 和执行行为变化。 |
| `AL-A4` Event / Command / Query 边界 | `tools\validate_event_command_query_boundary.ps1` + AL-A1/A2/A3 边界门禁；`HELLOMINE3D_WORLD_SMOKE_FOCUS=AL-A4` 和完整 WorldRuntime；必须保持 typed command FIFO、immutable fact、订阅者 effect/republish、8 层递归、诊断隔离和查询非 mutation 语义。 |
| `AL-A5` Tick Phase Metrics / Budget 词汇 | `tools\validate_simulation_metrics_boundary.ps1` + AL-A1..A4 边界门禁；`HELLOMINE3D_WORLD_SMOKE_FOCUS=AL-A5` 和完整 WorldRuntime；VS2017/v141 Debug/Release 完整门禁。只允许四个真实 metric phase 的 last-tick elapsed/processed/deferred/budget scope/status；不得改变 hard limit、phase 顺序、Gameplay 或引入 Scheduler/Registry/空系统槽。 |
| Track B Core（B1-B6/B10） | 逐批静态 gate 与聚焦 WorldRuntime；B10 额外运行 `HELLOMINE3D_WORLD_SMOKE_FOCUS=B10`、正式 schedule-v3 1800 秒五阶段压力、同 seed 双确定性探针、未放宽 Q1 fast-streaming 和完整 VS2017/v141/隔离包门禁。Lifecycle 资格必须在 8 项 unload budget 前过滤，取消 reservation 不得保留无界 `Absent` 墓碑。 |
| `C1` Block Capability Model | `tools\validate_block_capability_model.ps1` + `HELLOMINE3D_WORLD_SMOKE_FOCUS=C1-CAP` + 完整 WorldRuntime/VS2017 双配置门禁。保留 Chest/Furnace/Crusher 的既有 provider、UI 能力访问和错配/损坏/陈旧句柄失败关闭；C3 只允许 Crusher 增加具体 `MechanicalPort`，仍禁止 Registry 与 Extended 预注册。 |
| `C2` Machine Runtime v0 | `tools\validate_machine_runtime.ps1` + `HELLOMINE3D_WORLD_SMOKE_FOCUS=C2-MACHINE` + Recipe/Resource Pack/terrain atlas + 完整 WorldRuntime/VS2017 双配置门禁。必须保留五态优先级、Furnace 兼容、Crusher 正常 craft/place/Use/crank、槽位/动力/原子完成、break spill、malformed/stale/mismatch、unload/reload、save/reopen 和 economy v2；C3 拓扑不得改变独立手摇或引入动力传播。 |
| `C3` Mechanical Topology Model v0 | `tools\validate_mechanical_topology.ps1` + `HELLOMINE3D_WORLD_SMOKE_FOCUS=C3-TOPOLOGY` + 完整 WorldRuntime/VS2017 双配置门禁。必须覆盖 Crusher-only 六面端口、确定性 component id/canonical edge、merge/split/no-op、正常 place/break、malformed/stale、Chunk unload/reload、save/reopen 派生重建、正常 UI 与 Debug 观察；禁止持久化 topology、C4 power、通用网络、物流和 C4+。 |
| `D1` Simulation Phase Scheduler v0 | `tools\validate_simulation_phase_scheduler.ps1` + `HELLOMINE3D_WORLD_SMOKE_FOCUS=D1-SCHEDULER` + AL-A5/B6/C2/C3 聚焦回归 + 完整 WorldRuntime/VS2017 双配置门禁。保留三类真实 workload、64/4/32 item budget、稳定集合 round-robin/FIFO、单步无 catch-up、mandatory Player/8 phase barrier、copied diagnostics 和 save v12；D2 进入调查不等于已实现 activation。 |

V09d补同对象直接加载动态缺口：仅`CaveBoundarySmokeCases.h`新增8项，使用合法
Resident→EvictRequested→Absent→Requested→Loading前置，不销毁对象、不在加载前collect。
Debug／Release实际`CAVE_BOUNDARY`各43／0；去除`loadBlockData`的incarnation更新的单TU
隔离负控各43项／精确2 FAIL，正常exit1，128原对象及8archives保持。r1非法前置导致的
Debug SIGABRT与Release非法状态日志保留；不是正常loader已复现缺陷，原预算／oracle未放宽。
该测试改动不重建普通客户端，不关闭GPU／ABA、普通地下路线或整项Goal。收据见
[V09d复核](../../build/visual-experience-polish-20260928/v10o/cache-independent-review-r1.json)。

V10o在同4f6普通客户端补1280×720内容、双语×0.85／1.00四个空2×2展开手册单元；
首个误命名0.85配置实际0.84保留，正确0.85证据为`config-wide-zh-085-corrected.txt`。
八原PNG、配置与两次正常输入前后保存及活动时点24文件副本见`v10o/`。
六次W／S短按、Space和drag未证明连续移动或采集；F5角色可见单列。原配置逐字节恢复。
CUA截图超时、kernel reset后服务启动失败，008实际仍空合成；最终正常保存退出OPEN，
日志只是前缀，不声明native退出成功。完整UI矩阵、制作、普通路线及音频／稳定性继续OPEN。

## 完整验证路由

| 验证 | 命令或目标 | 适用改动 |
| ---- | ---------- | -------- |
| Windows 工程生成 | `tools\premake\premake5.exe --os=windows --file=premake/premake.lua vs2017` | 构建系统或文件布局；当前正式工具链是 VS2017/v141 |
| Windows 全量门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify_build.ps1 -VisualStudioVersion 2017` | 里程碑封板、跨目标源码或链接变化；无活动桌面时显式加 `-SkipRealWindow`，相关真实窗口结果记 `NOT_RUN` |
| World 公开 API 责任门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_world_responsibility_map.ps1` | `World.h` 公开声明或 `docs/current/architecture.md` 责任地图变化；完整 Windows 门禁也会自动运行 |
| Chunk Runtime 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_chunk_runtime_boundary.ps1` | `World` / `ChunkRuntime` / `ChunkManager` 的队列、worker、预算、mesh commit 或 unload 协调变化；完整 Windows 门禁也会自动运行 |
| Simulation Runtime 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_world_simulation_boundary.ps1` | `World::tick`、`WorldSimulation`、phase/context/raw timing、暂停入口或相关 debug snapshot 变化；完整 Windows 门禁也会自动运行 |
| Event / Command / Query 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_event_command_query_boundary.ps1` | command FIFO、EventBus、生产订阅者、查询或未来 Machine/Network 依赖变化；完整 Windows 门禁也会自动运行 |
| Simulation Metrics 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_simulation_metrics_boundary.ps1` | phase metric identity、processed/deferred/budget scope/status、Actor 计数、开发者 Simulation 面板或相关 snapshot 变化；完整 Windows 门禁也会自动运行 |
| Block Capability 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_block_capability_model.ps1` | BlockDefinition capability 声明、Chest/Furnace 访问适配、容器 UI 分派或未来 C2/C3/Extended 边界；完整 Windows 门禁也会自动运行 |
| Machine Runtime 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_machine_runtime.ps1` | MachineRuntime、Furnace/Crusher adapter、processor capability、Crusher payload/recipe/crank、资源经济 schema 或 C3/网络越界；完整 Windows 门禁也会自动运行 |
| Mechanical Topology 边界门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_mechanical_topology.ps1` | C3 Crusher 节点/端口、确定性连通分量、World/Chunk 同步、能力/UI 观察、save 非持久化或 C4/通用网络越界；完整 Windows 门禁也会自动运行 |
| Simulation Phase Scheduler 门禁 | `powershell -NoProfile -ExecutionPolicy Bypass -File tools\validate_simulation_phase_scheduler.ps1` | D1 admission、Actor adapter、Furnace/Crusher 单项执行、调度 snapshot 或 UI；完整 Windows 门禁自动运行 |
| Windows Debug 编译 | `MSBuild build\HelloMine3D.sln /p:Configuration=Debug /p:Platform=x64` | 所有 C++ 改动的主干检查 |
| Windows Release 编译 | 同上，配置改为 `Release` | 里程碑和发行候选 |
| macOS Xcode 门禁 | `bash scripts/verify_xcode.sh` | Xcode 图、macOS 平台或原生封板 |
| macOS gmake 双配置门禁 | `MAKEFLAGS='-j2 -B' bash scripts/verify_build.sh` | 从头编译客户端/依赖和 13 个测试目标，运行 Debug/Release。两配置共用 `bin/` 输出，正式干净门禁保留强制重编译，避免旧配置产物混用；不为普通文字提交触发此门禁。生成 x86_64 macOS 证据，不冒充 Xcode/arm64/Windows。 |
| macOS 启动负例 | `python3 tools/validate_startup_errors_macos.py --output <new-output-dir>` | 与 Windows 共用 10 个缺失和 5 个非法 fixture 定义，要求非零退出、准确资源诊断和 `ui=stderr-only` 报告；Windows MessageBoxW 范围后置 |
| macOS 干净包 | `python3 tools/package_macos_release.py --output <new-app-path>` | 调用者先构建 Release；复制 manifest 资源和 notices、检查动态依赖、记录构建/可执行文件/资源哈希；不会覆盖已有输出，正常窗口验收需另行执行 |
| 十三个 headless 目标 | `scripts\verify_build.ps1` 中列出的测试/Smoke/Soak | 全量里程碑回归 |
| 世界运行冒烟 | `bin\HelloMine3DWorldRuntimeSmoke.exe` | 区块、存档、交互、事件、实体、地形 |
| 渲染截图 | `tools\run_render_capture.ps1` | renderer、shader、texture、mesh、HUD |
| 性能采集 | `tools\run_perf_baseline.ps1`；正式六场景使用 `tools\capture_release_candidate_performance.ps1` | 区块加载、网格、更新、渲染提交 |
| 性能比较 | `tools\compare_perf_baselines.ps1` | 可能改变帧时间或世界驻留的改动 |
| 资产检查 | `bash scripts/check_assets.sh` | 资产和数据 |
| V10B2 图集合同 | `tools\validate_terrain_atlas.ps1` | 37 个语义/双语名、Alpha 边界、分面、HUD/手持一致性与确定性输出 |
| R3 自动预检 | `tools\validate_r3_automated_preflight.ps1 -Configuration Release -Build` | 控制器、交互、容器、战斗、D6 和后台窗口焦点的逻辑回归；只是 AI 交互前置条件。 |
| AI/Computer Use 功能验收 | `docs\current\ai-assisted-gameplay-acceptance-v1.md` 的 `AI-01..AI-08` | 从带哈希的干净 Release 包用正常 OS 输入执行；禁止 fixture、注入、传送、存档编辑和直接 Gameplay API。严格 `AI-06` 还要求仓库不可访问的 package-only 新任务。Windows 首份记录为 `NOT_RUN`；本次 macOS AI-01 为部分通过、总项 `BLOCKED`，见 Goal 记录。 |
| AI 视觉/可读性验收 | 同规范的 `AI-07`，配合原尺寸截图、多帧/视频、连续窗口观察和可访问音频证据 | 可关闭截断、重叠、缺字、破面、闪烁、状态/轮廓可见性和 cue/字幕生命周期；正式证据来自带哈希干净包。`run_render_capture.ps1 -CaptureMs ...` 已支持多帧，但在直接证明发行包可执行文件身份前只作开发预检；不声明人类审美、听感或舒适度。 |
| R3 v1 / Physical Input v2（历史） | `docs\archive\manual-input-acceptance-v1.md`、`docs\archive\physical-input-acceptance-v2.md` 及原校验器 | 历史物理合同 `SUPERSEDED` 为当前门槛，模板保持 `NOT_RUN`；未来自愿运行也必须遵守原物理语义。 |
| 开发者视觉检查（历史/补充） | `docs\archive\manual-product-experience-acceptance-v1.md` A 节与既有 PASS 记录 | 已完成记录继续有效；后续可作为 `DEVELOPER_SELF_TEST` 补充 AI 视觉证据。 |
| 长时间 soak | `tools\run_world_soak.ps1`；正式双 profile 使用 `tools\run_release_candidate_soak.ps1` | 区块/实体生命周期、存档、后台加载 |
| B10 Track B Core 长稳 | `tools\run_large_world_stress_acceptance.ps1 -Formal` + `tools\validate_large_world_stress_acceptance.ps1 -Evidence` | B1-B6 的 demand/job/cancellation/backpressure/spatial-interest 组合变化；精确 36,000 fixed ticks、LW1-LW5、进程/世界界限与确定性摘要 |
| 资源包 | `tools\validate_resource_packs.ps1` | manifest、资源解析、启动预检 |
| 干净发行包 | `tools\package_windows_release.ps1` | 发行包、manifest、资源解析 |
| 世界目录 | `HelloMine3DWorldCatalogueSmoke` | 世界发现、名称、id、目录 |
| 事务保存/恢复 | `HelloMine3DStorageTransactionSmoke`、`HelloMine3DWorldBackupSmoke` | 保存发布、隔离、备份、恢复、格式迁移 |
| 崩溃产物 | `HelloMine3DCrashDiagnosticsSmoke`、`tools\validate_crash_diagnostics.ps1` | 异常处理、dump、sidecar、符号、崩溃 UX |
| 数据竞争 | `bash scripts/verify_tsan.sh` | 后台加载、区块图同步、工作线程调度 |
| 启动/保存/恢复预算 | Q1 schema + Q2 阶段计时 | 目录、启动、进世界、保存、备份、恢复 |
| 内容规模 | `docs/contracts/q3-scale-soak-contract-v1.md` + 正式双 profile soak | 快速移动、人口规模、持久化压力 |
| 完整玩法切片 | G6 自动 fixture + 干净包 `AI-08` | 自动 fixture 证明状态边界，Computer Use 证明正常界面和输入可走通 |

## 验收节奏

| 时机 | 要求 |
| ---- | ---- |
| 功能开发中 | 编译、定向自动测试、数据守恒、存档兼容和必要交互冒烟。 |
| 一个玩法任务完成 | Debug/Release 相关目标、世界冒烟和该任务的失败边界。 |
| G6 集成前 | 清理所有主干构建失败，补齐跨系统回归。 |
| 发布/展示候选 | 当前批准平台门禁、适用 AI 场景、H1-H3、Q1-Q3、长稳、视觉证据和干净包。 |
| Stage 9 单批完成 | 批次合同、定向测试、全部受影响旧版迁移、相关 Q1、必要的 60-120 秒 nominal/stress 和隐藏客户端。 |
| BETA-RC | 历史工程封板保持原证据；当前分类由 AI 验收规范承接，不补写物理/真人 PASS。 |
| Stage 10 单批完成 | 批次合同、VS2017/v141 双配置受影响目标、shader/资源负例、固定 before/after RuntimeReadback、开发者视觉检查和相关 Q1；图形功能必须可关闭。修改 shader/顶点接口时当批补 macOS Release 真实窗口冒烟，否则跨平台状态保持 `Verify`。 |
| Stage 11 单批工程完成 | 历史工程证据保持冻结；适用 AI 场景未执行时写 `Engineering Done / AI NOT_RUN`，主观范围写 `NOT_CLAIMED`。 |
| Architecture Lab Sprint | 受影响自动门禁、数据/迁移、性能、可解释 Debug、游戏内 Demo 和适用 AI 场景定义。 |
| Architecture Lab Track | 完整自动门禁、干净包、新系统压力证据和 `AI-08`；既有主菜单→胜利→保存重开流程必须可完成。 |

## 第 8 阶段批次门禁

| 批次 | 合入前最低证据 | 可后置证据 |
| ---- | -------------- | ---------- |
| `BLD-1` | 重新生成 VS 工程；WorldRuntimeSmoke/Soak 项目同时包含崩溃源和 `dbghelp.lib`；Windows Debug/Release 全量编译及定向目标通过。 | 已于 2026-08-17 通过后台 Release 受控 dump 和干净包。 |
| `K4` | WorldCatalogue、StorageTransaction、WorldBackup 定向测试；创建/重命名/删除恢复的路径与故障边界；一次菜单交互冒烟。 | `AI-01` 正常主菜单与恢复操作。 |
| `G2` | RecipeSmoke；库存/制作状态守恒；预览无副作用；提交失败原子性；关闭、满背包、连点和重载。 | `AI-02/AI-05` UI 与完整流程。 |
| `G3` | 工具表解析；破坏进度状态机；等级/掉落/耐久边界；物品实例存档迁移；固定动作性能采样。 | Q1/Q3 和长稳已完成；功能差异进入 `AI-05`，人类手感不声明。 |
| `G4` | 设置解析/版本/原子写入；应用/取消；暂停模拟；UI 焦点隔离；开发者交互冒烟。 | `AI-01` 窗口、焦点、暂停和设置重启。 |
| `G5` | 音频资产检查；事件去重；音量/pause/mute；dummy backend；无设备和缺失资源降级。 | `AI-07` cue/字幕/生命周期；人类听感 `NOT_CLAIMED`。 |
| `G6` | 干净启动垂直切片 fixture、跨系统保存/恢复、Debug/Release 相关目标、截图和可比较性能采样。 | 干净包 `AI-08` 正常主线。 |
| Alpha 检查点 | 跟踪 v3 迁移 fixture；Debug/Release 世界与崩溃 smoke；双模式隐藏客户端；Alpha 基线/复测；匹配及错误 PDB。 | 后续工程证据已在 RC 关闭；真实窗口功能由 AI 场景承接。 |
| `N3` | 严格食物解析；面包配方；成功/满血/暂停/死亡/冷却/UI 占用的原子语义；生命/冷却保存重载；v5→v6 迁移；目标事件；资源包与启动缺失检查。 | `AI-03/AI-04/AI-08` 证明战斗、恢复和正常旅程；是否引入饥饿仍需独立立项。 |
| 封板 | H1-H3、Q1-Q3、当前批准平台构建、存档故障、资源包、视觉证据、soak、干净包和适用 AI 场景全部通过。 | 人类主观与物理体验保持 `NOT_CLAIMED`，不作为隐藏待办。 |

## Stage 9 / Beta 预排门禁

本节只定义未来批次启动后的最低证据，不表示这些工作已经完成或已经加入当前 77 项总账。

| 批次 | 合入前最低证据 | 阶段结束补充证据 |
| ---- | -------------- | ---------------- |
| `RC0` | `Superseded`：bundle、文档一致性和自动工程基线已由 BETA-RC 覆盖，不再作为活跃批次。 | 历史物理输入合同不再关闭当前功能范围；D2/D4/D6 由 AI 场景承接。 |
| `N7A` | 独立结局状态/奖励 epoch；不以目标耗尽推导胜利；预计 v8→v9 及全部旧版迁移；状态非法值、事务失败、备份恢复；中英文 key/fallback。 | 胜利前中后保存重开，世界列表只从持久状态标记；重复重入不重复发奖。 |
| `N7B` | 激活/守卫状态机；复用现有敌人；actor/事件/驻留上限；死亡、暂停、卸载和遭遇中恢复；相关 Q1 和短时 soak。 | 干净新世界无 debug 注入完成胜利并继续 Playing；进入 `AI-08`。 |
| `N8A` | 前摇/命中/击退/格挡；显式状态转换、目标失效、固定 tick 射线/寻路预算和调试快照。 | 多敌人压力、AI 战斗可读性、内容规模 Q1 和阶段 Q3；人类战斗感不声明。 |
| `N8B` | 战斗档案严格解析；投射物遮挡、寿命/距离/上限，死亡/卸载/保存重载清理；世界冒烟和短时 soak。 | 远程/防御差异清晰，无幽灵投射物、容量突破或重复命中；最终 Q3。 |
| `N9A` | 结构类型/cell/seed/terrain 版本/加载顺序确定性；footprint/间距/覆盖；禁止 `std::rand` 和同步邻区块加载。 | 结构计划快照、跨区块投影、快速流送 Q1 和旧 terrain v1/v2 身份。 |
| `N9B` | 初始战利品快照；箱子库存持久后不再初始化；玩家修改不被覆盖；资源/actor/结构上限。 | 两类地点综合旅程和正式 Q3；若启用 terrain v3，补全新旧世界矩阵。 |
| `N10` | 食物/精确配方/三槽冶炼严格解析；库存守恒；容量/连点/暂停/卸载/保存；主线可达、来源/消耗点和净增益循环检测。 | 综合经济旅程、平衡数据、库存/保存规模比较；饥饿仍需独立评估。 |
| `N11A` | 三档版本化参数；旧世界 Normal；世界保存/metadata 迁移；暂停菜单事务修改并在下个固定 tick 生效；同 seed/难度确定性。 | 三档核心旅程、内容规模/短时 soak、世界列表/备份恢复和 AI 交互确认。 |
| `N11B` | 有界事件状态和奖励去重；胜利前后/重载/备份恢复；不破坏主线结局。 | P2 伸缩项；纳入首个 Beta 才进入 BETA-RC 必过矩阵，否则明确后置。 |
| `N12A` | `en-US`/`zh-CN` key 对齐；fallback、非法 UTF-8、字体、长文本、极端缩放、字幕、Credits 和许可证。自动开发门禁已于 2026-08-26 通过。 | 两种语言核心界面进入 `AI-07`；人类阅读偏好 `NOT_CLAIMED`。 |
| `N12B` | v3 采样定义、9 个 WAV、61 项 manifest、许可证/Credits、缓存/并发上限、缺文件/损坏/设备降级、停止与退出清理均已于 2026-08-26 通过；VS2017/v141 双配置世界 681/681、资源包 34/34，隐藏真实三帧后端无降级。 | cue/字幕/生命周期进入 `AI-07`；人类听感 `NOT_CLAIMED`。 |
| `N12C` | v1 单通道流式定义、20 秒原创 WAV、settings v4、64 项 manifest、许可证/Credits、严格 RIFF/路径/时长、延迟/淡入淡出/低密度间隔、暂停/挂起/静音/音量归零、缺资源/设备、线程和退出清理已于 2026-08-26 通过；VS2017/v141 双配置世界 699/699、资源包 38/38，隐藏真实后端无降级，同身份 Q1 比较通过。 | 暂停/切世界/退出过渡功能进入 `AI-07`；人类音乐偏好 `NOT_CLAIMED`。 |
| `BETA-RC` | 2026-08-26 工程通过：Windows 适用全门禁、v1→v11 迁移、结局/奖励、战斗/投射物、结构/战利品、经济、语言/音频清理、六类 Q1、Q3 双档各 1800 秒、崩溃/符号/84 文件干净包均通过。 | 历史 R3/产品体验状态保持在原报告；当前功能与可观察范围由 AI 规范承接。 |

## Stage 9 回归触发器

| 改动类型 | 必须触发 |
| -------- | -------- |
| 世界、玩家、目标、容器或胜利状态 | 版本/非法值/全部旧版迁移，结局权威，奖励 epoch，遭遇中恢复，保存退出重开，事务失败，备份恢复，幂等和守恒。 |
| 地形、生态、结构或战利品 | terrain 身份，结构类型/cell/seed 与加载顺序确定性，禁止同步邻区块加载，跨区块投影，初始战利品快照，快速流送 Q1，结构规模和 Q3。 |
| actor、AI、攻击或投射物 | 战斗档案解析，固定 tick，状态/目标失效，actor/投射物/事件/射线/寻路容量，遮挡，死亡/卸载/重载清理，掉落去重，内容规模 Q1/Q3。 |
| 食物、配方、冶炼或资源 | 严格解析，输入输出守恒，容量/连点/暂停/卸载，主线可达，来源/消耗点和净增益循环，库存与保存规模。 |
| UI、输入、键位或设置 | 焦点隔离，配置迁移，键位冲突，隐藏客户端，截图和 R3。 |
| 音效、音乐、字体或本地化 | manifest，资源包，语言 key/fallback，字体/长文本，许可证，采样缓存/并发，句柄/线程生命周期，隐藏客户端和听感。 |
| 构建、崩溃或打包 | H2/H3 受控崩溃、符号、隐私、可执行清单、干净包和新 SHA-256。 |

Windows 自动化 EXE 一律隐藏或后台运行。只有用户明确安排的 AI/Computer Use、开发者自测或
历史物理合同运行才可以启动前台窗口；`ShowWindowNoActivate` 只是 best-effort，不能作为不会
抢焦点的保证。Computer Use 记录必须明确标成 `AI_INTERACTIVE`，不得伪装真人/物理证据，也
不得在用户未安排的时段反复启动、抢占当前焦点。

## Stage 10 视觉门禁

| 批次 | 合入前最低证据 | 阶段结束补充证据 |
| ---- | -------------- | ---------------- |
| `V10A` | 顶点 AO 0-3、侧边/对角规则、天空光/方块光、透明/未知邻域、跨 section 一致性；只有合并结果可重建全部内部采样才 greedy 合并，对角线按误差和固定 tie-break 选择；保持 32 字节 stride；Debug/Release 受影响目标。 | FS2 四图加洞穴/树冠/遗迹/营地 before/after；schema 3 既有 geometry/mesh/residency 字段补充比较、快速流送/规模 Q1、短 Q3 和开发者检查。超过 10% 未获批准不得关闭。 |
| `V10B1` | 图集尺寸/分格/坐标和颜色参数严格解析；旧图集像素兼容；不得静默改变 V10A AO/光照曲线；缺失/越界负例。 | shader 负例、Windows 双配置、开发者检查和 macOS Release 真实窗口冒烟。 |
| `V10B2` | 原创 top/side/bottom 资产、生成脚本、来源/许可、manifest、透明边界和世界/HUD/手持一致性。 | 近景/中景截图、资源包、相关 Q1、干净包和开发者检查。 |
| `V10B3`（Done） | 生态 tint 范围、坐标/seed/加载顺序确定性 tile 变体、greedy merge key 和 terrain v3 不变。 | 261 项图集、732 项 Release 世界、五张固定截图、相关 Q1、短 Q3 和 `developer-visual-record-v10b3.txt` 均通过。 |
| `V10C`（Done；Windows） | 定向雾共享参数、云层高度/厚度/速度/颜色边界、帧率无关移动、进入云层与关闭回退。 | 双配置聚焦 21/21、Release 世界 741/741、资源包 65/65、启动负例 14/14、十图多帧开发者视觉 PASS；相关 Q1 保留已批准的快速流送单样本延迟例外。新 macOS 运行 `NOT_RUN`。 |
| `V10D`（Done；Windows） | settings v5、v0-v4→Off、双语 key、Stage 10 shadow 补充身份；Off/Medium/High、距离/纹理/bias/PCF 上限、能力回退和资源清理。 | 双配置聚焦 21/21、资源包 75/75、Release 世界 742/742、74 项 manifest、14 类启动负例、9 项档位合同和强制回退通过；正午/黄昏六图开发者检查 PASS，最终同场景各档 frame P95/P99 无需例外。新 macOS 运行 `NOT_RUN`。 |
| `V10E`（Done；Windows） | settings v6、v0-v5→Off、双语 key、Stage 10 post 补充身份；Off/On tone curve/确定性抖动/八采样极轻 bloom、HUD 排除、缩放、回退和清理。 | 双配置聚焦 22/22、资源包 80/80、Release 世界 743/743、77 项 manifest、15 类启动负例、11 项性能合同和真实支持/强制回退通过；六图开发者检查 PASS，同档性能无需例外。新 macOS 运行 `NOT_RUN`。 |
| `VISUAL-RC`（Done：Windows 工程） | 适用全门禁、完整截图/开发者检查矩阵、资源/许可/Credits、正式相关 Q1、干净包和 bundle。 | Windows 77 项 manifest、743/743 世界、80/80 资源包、六类 Q1、nominal/stress 各 1800 秒 Q3、97 文件包和 239 项 Xcode 工程图静态检查通过；AI 产品表现进入 `AI-07`，主观体验 `NOT_CLAIMED`。 |

Stage 10 的地形 shader、顶点数据、图集、云、阴影或后处理变化都必须保留一个明确的低画质
回退路径。固定截图证明确定性，AI/开发者视觉检查负责可观察缺陷；动态项必须使用多帧、视频或
连续窗口。人类审美和舒适度 `NOT_CLAIMED`，视觉证据也不替代 AI 交互场景。

## Stage 11 可玩性门禁

Stage 11 的工程证据已经冻结。下表不重跑或改写这些 PASS，只定义 Architecture Lab 当前如何
补充真实窗口证据。本次 macOS `AI-01=BLOCKED（部分通过）`、
`AI-06=BLOCKED（隔离环境）`；其余正式场景尚未完成，逐项状态见当前 TODOLIST 和 Goal 记录。

| 批次 | 已有最低工程证据 | 当前 AI 场景 | 不声明范围 |
| ---- | ---------------- | ----------- | ---------- |
| `P11-0` | 火把/材料末尾追加、配方守恒、Rose 修正、metadata 发射、局部重光照、双配置和正式 Q1。 | `AI-05` 制作/放置/洞穴照明；`AI-07` 夜晚/熔炉多帧。 | 人类审美。 |
| `P11A` | 单一动作仲裁、settings v7 迁移、线性相对增量、焦点门、hold/toggle、定向 88/88。 | `AI-01..AI-04` 移动、世界/UI、暂停、Alt+Tab、最小化、回焦和重启。 | 物理鼠标距离、键鼠舒适度。 |
| `P11B` | 判定/表现解耦、数量/持续时间上限、关闭回退、音频并发和 HUD/准星隔离。 | `AI-05` 动态交互；`AI-07` 多帧/视频辨识。 | 人类打击感、眩晕和长期舒适度。 |
| `P11-1` | 身份末尾追加、门保存、木板/圆石守恒、axe/shovel 矩阵、资源和旧存档回归。 | `AI-05` 采集、工具比较、建可关闭落脚点。 | 人类建造乐趣、主观速度感。 |
| `P11C` | 目标 v3、三项并行机会、分支进度、配方发现、迁移、双语与完整世界回归。 | `AI-06` 在仓库不可访问的 package-only 新任务中完成 30 分钟盲玩；只有新工作目录但仍可读仓库时为 `BLOCKED`。 | 人类首次体验、留存和乐趣。 |
| `P11D` | 奖励 v1/v0、save v12、三类能力、持久化和全部迁移/资源门禁。 | `AI-05` 逐结构取得并实际使用能力，记录后续可执行操作变化。 | 奖励吸引力和人类价值感。 |
| `P11-2` | terrain v4、Mountain/洞口、v1-v3 冻结、保存身份、完整回归与正式 Q1。 | `AI-05` 寻找/通行；`AI-07` 轮廓/洞口可见性。 | 人类风景审美。 |
| `P11E` | 多部件轮廓、关键姿态、死亡表现隔离、身份掉落、Waystone 共鸣和完整门禁。 | `AI-05` 战斗/共鸣；`AI-07` 无名称轮廓与状态可辨识。 | 人类危险感、掉落价值感和战斗乐趣。 |
| `P11F` | VS2017/v141 双配置、832/832 世界、六类 Q1、双档 Q3、dump/符号和 104 项干净包。 | `AI-08` 从主菜单经胜利到保存重开；历史报告不回写。 | 外部玩家签字和未批准的新 macOS 运行。 |

AI 记录至少包含 commit、包哈希、执行器/Computer Use 环境、OS/GPU、窗口/图形/语言、seed、
存档身份、重试/超时/意外弹窗、证据路径和声明边界。截图或日志只能支持窗口内的实际步骤；
物理设备手感和人类可玩性不得由 AI 记录推导。

## T0/T1 地形基础（2026-09-06）

本次以 macOS 为必需平台，Windows 专属验证后置。`T0-SURVEY` 生产采样与
`tools/validate_terrain_foundation.py` 检查冻结的统计及 v1–v4 输出；`T1` 聚焦覆盖全 signed int
采样、8 seed 加载顺序/独立并发、出生支撑/资源距离/结构候选及保存修改重开。
完整 Xcode Debug/Release 门禁、干净包三场景固定画面、steady/streaming 三轮成本和正常输入
步行另列，不能用 headless 或固定诊断替代。具体阈值见 [v5 合同](../contracts/terrain-foundation-v5-contract-v1.md)。

E2 terrain v8 按 [E2 合同](../contracts/ecology-surface-coast-v8-e2-contract-v1.md)
单独验收：v1–v7 生产采样与区块指纹、`E2_SURFACE` 聚焦和完整 WorldRuntime、
`E2-SCENES`/`E2-COASTS` 生产原始 CSV 与 `tools/validate_ecology_e2.py` 汇总、
Debug/Release 双配置与资源/存档回归。窗口性能必须在同一 E2 Release `.app` 用
v7/v8 林地和岸边常驻/快速流送各三轮，预热 5 秒记录 30 秒，以
`tools/compare_ecology_e2_performance.py` 汇总原始逐帧记录，P95/P99
三轮中位数比值均不超过 1.10；保留原始逐帧和失败。固定原图及中文菜单正常玩法
分别交付；桌面锁定或输入不可用时按项标 `BLOCKED`，不得以自动化替代。

2026-09-15 用户为 E2 验收追加右上角小地图。最低检查增加：v1–v8 使用当前世界身份的
生产地形规划且不加载邻区块；切换 seed/版本清缓存；中文 1280×720 下纯圆形地图的北向、
玩家方向和约 128 米范围可辨，且无方形底板、坐标栏、版本栏，操作提示不重叠。小地图加入后须重新生成最终二进制、
固定原图、24 次同包性能和两个带哈希验收包；候选 20 只作为追加前历史证据。

2026-09-16 用户要求减少客户端重复启动：允许显式环境变量启用的同进程 E2 批量诊断，
一次启动顺序执行 24 段性能及固定画面，各段仍分别载入测量世界、留 5 秒预热和 30 秒
原始逐帧 CSV。批量证据必须证明同一 `.app`/PID、0..23 相位顺序、每段正确的 seed/terrain
version、存档元数据与世界进入成功；仅首相位需要进程启动成功字段。旧逐启动记录仍要求
每段启动成功。若正式三轮出现 FAIL 或明显波动，保留原件，按合同预定反序另做一次启动的
24 段完整复核，门槛不变；正常中文菜单输入与批量诊断分开。

E2 v7/v8 性能相位必须从同一冻结存档模板复制，除唯一 `world_id` 和
`terrain_generation_version` 外初始元数据一致，并同时固定普通难度与零初始 Actor。批次和逐相位
收据须记录模板/初始元数据哈希；比较器拒绝缺少夹具身份、难度不同或初始 Actor 非零的数据。
此前空 v8 目录与休闲 v7 模板混用的数据保留为协议失败原件，不作为 terrain-only 门槛证据。

2026-09-16 用户允许 E2 自动证据结束后先延期人工项目并启动 E3/R1/R2；该调整仅改变
开发顺序，E2 林地流送 P99 两轮 `FAIL` 不改判。E3 terrain v9 按
[E3 合同](../contracts/ecology-inland-meadow-v9-e3-contract-v1.md) 验证：
`E3_MEADOW` 聚焦及完整 WorldRuntime、由生产测试进程导出的 v1–v9 `T0-SURVEY`
样本/区块 CSV、`tools/validate_ecology_e3.py` 的 v1–v8 原始指纹与 v9 高度/生态/草甸
比例核对、Debug/Release 构建和必要的存档/资源回归。

为弥补常规 32 区块样本不覆盖开窗内域，`E3-MEADOW-SURVEY` 在冻结的 16 个
正/负坐标生产区块导出 Debug/Release 逐列、区块指纹和地点计划，并由
`tools/validate_ecology_e3_meadow_chunks.py` 核对源哈希、完整 XZ、方块与地点覆盖。
地点投影后的 Dirt 地基须保留在原始统计中；只有地点覆盖外的非草甸地表是 E3
地表失败，原误判及修正说明分别存证。该离线调查不替代窗口林缘视觉验收。

窗口性能及可见画面对照必须另记
真实同客户端数据；锁屏下的中文菜单步行/采集/重开按用户顺序延期为 `NOT_RUN`，不能写
`PASS`。E3 开工不关闭 E2，也不放宽 E2 的 1.10 门槛。

E4 terrain v10 按 [E4 合同](../contracts/ecology-inland-relief-v10-e4-contract-v1.md)
验证：`E4_RELIEF` 聚焦、完整 WorldRuntime、由生产进程导出的 v1–v10 `T0-SURVEY`、
`tools/validate_ecology_e4.py` 汇总、Debug/Release 构建和相关存档/资源回归。窗口检查复用
同一个工作 `.app`，在一次启动中完成固定山麓画面和常驻/快速流送采样；版本夹具必须显式
创建 v10 世界，不能用“当前版本”代替冻结身份。

E5 terrain v11 按 [E5 合同](../contracts/ecology-inland-water-v11-e5-contract-v1.md)
验证：`E5_WATER` 聚焦、完整 WorldRuntime、v1–v11 生产 `T0-SURVEY` 与
`tools/validate_ecology_e5.py` 汇总、Debug/Release 构建及 WorldCatalogue、StorageTransaction、
WorldBackup、SaveLoad、ResourcePack 回归。聚焦范围至少覆盖八 seed 正负坐标的连续河谷、
实际水深、生成方块、反序加载、出生资源、洞口、三类地点及 v11 保存重开；通用固定区块不
要求必然命中稀疏水道。正式窗口仍使用唯一工作 `.app`，固定 v10/v11 同 seed/XZ/朝向/时间
画面并各做三轮常驻和快速流送。图形会话锁定时如实记为 `NOT_RUN`，自动证据只能关闭工程
范围，不能代替正常中文菜单步行、采集和保存重开。

E6 terrain v12 按 [E6 合同](../contracts/ecology-vegetation-mosaic-v12-e6-contract-v1.md)
验证：`E6-VEGETATION-SURVEY` 从 16 个固定林区导出 144 个生产区块与逐列原始 CSV，修改前
独立导出两次并逐字节核对；实现后用 `E6_VEGETATION` 聚焦覆盖纯规划、三种树形、树冠边界、
地被地面、反序加载、v9 草甸、v11 水系、资源地点和 v12 保存重开。另须运行 v1–v12
`T0-SURVEY`、Debug/Release 完整 WorldRuntime 及受影响的存档/资源回归。窗口与性能复用唯一
工作 `.app` 并一次启动采集；图形会话不可用时按用户授权延期，不改写为 PASS。

2026-09-17 自动阶段结果：v12 独立生产调查两次逐字节相同，固定样本为 527 个树根、
24,706 个叶块和 1,044 个地被；树群疏密 15/16 林区、地被斑块 12/16 林区满足门槛，三种
树形在八 seed 均出现。v1–v11 共 22 份 `samples.csv`/`chunks.csv` 与 E5 冻结输出逐字节相同；
Debug/Release 聚焦各 `13/13 PASS`，完整 WorldRuntime 各 `1149/1149 PASS`，相关目录、事务、
备份、保存载入和资源包回归通过。Release 完整回归首轮暴露两个依赖出生加载范围的小地图
夹具随机失败，失败日志保留；夹具改用确定的远端区块后双配置通过。完整哈希和待做窗口项见
[E6 执行记录](../reports/ecology-e6-vegetation-v12-execution-2026-09-17.md)。

E6 窗口阶段已用唯一工作 `.app` 的同一 PID `82431` 完成 28/28 个阶段：24 个 v11/v12
三轮性能段和 4 个固定画面段。四组 P95/P99 中位数比分别为正坐标常驻 `1.042×/1.059×`、
正坐标流送 `1.017×/1.029×`、负坐标林缘常驻 `0.986×/1.018×`、负坐标林缘流送
`1.001×/1.015×`，均 `PASS`。首次启动因已跟踪资源清单的字体项排序错误在窗口创建前失败，
失败证据保留，排序修复提交 `cd6304a` 后有效重试一次。Computer Use 在批次运行时再次报告
图形会话锁定，中文菜单正常玩法为 `NOT_RUN`。

### E7 地貌多样化 terrain v13

按 [v13 合同](../contracts/terrain-diversity-v13-contract-v1.md) 和
[执行记录](../reports/terrain-diversity-v13-2026-09-19.md) 检查：

- `bash scripts/verify_terrain_landforms.sh Debug <新目录>` 与 Release：八 seed 覆盖、
  高度/坡差、变化量、极值坐标与纯规划确定性。旧 v1–v12 使用同一最终 WorldRuntime 的
  `T0-SURVEY`，与修改前 `samples.csv` / `chunks.csv` 逐字节核对。
- `HELLOMINE3D_WORLD_SMOKE_FOCUS=E7_LANDFORMS`：48 个选定地点实际生成及反序指纹、
  公开查询、干燥植物、八 seed 资源/出生/洞口/结构接近方向、显式 v13 与改块保存重开。
- `HELLOMINE3D_WORLD_SMOKE_FOCUS=E7_REGIONS` 配合
  `HELLOMINE3D_TERRAIN_SURVEY_DIR=<新目录>`：导出三个 seed 的 18 片 128×128 实际区域，
  明确洞口/结构/矿物例外，检查沙丘/岩台跨度和湿地实际水、干草地。连片选点工具为
  `python3 tools/select_terrain_landform_regions.py <samples.csv> <新输出.json>`。
- 双配置客户端、完整 WorldRuntime、资源、世界目录、事务与备份回归。测试使用隔离根，
  完整套件需要现有 `tools/fixtures`，不读取用户世界；构建和运行命令沿 README。
- 同一工作包固定原图、多 seed / 多视角和连续相机；新地貌 Off / High 都观察。受影响
  常驻/快速流送 v12/v13 各三轮，P95/P99 中位数不超过 1.10，记录网格、驻留和生成成本。
  最后通过正常菜单新建/进入/保存重开；持续输入按用户决定暂缓，不由诊断代替。

### E8 地标建筑 terrain v14

按 [v14 合同](../contracts/landmark-architecture-v14-contract-v1.md) 检查：

- `HELLOMINE3D_WORLD_SMOKE_FOCUS=E8`：八 seed 三类实际地标的基础/连通、两格净空入口、
  核心/矿物/玻璃数量、箱子奖励、跨区块正反生成与树冠避让；默认 v14 改块保存重开，
  真实容器取空和门柱破坏后重开保持。查询 halo 超过 9 格必须拒绝。
  三类陡坡候选一旦已不合格，应提前结束高度查询；R4 同时保留旧规划器触发该工作量断言的负例。
- `HELLOMINE3D_WORLD_SMOKE_FOCUS=LANDMARK-SURVEY` 配合
  `HELLOMINE3D_TERRAIN_SURVEY_VERSION=2..14` 和 `HELLOMINE3D_TERRAIN_SURVEY_DIR=<新目录>`，
  导出 `plans.csv` / `chunks.csv`；v2–v13 与修改前生产基线逐字节比较，v1 由既有完整回归覆盖。
  八 seed 每类至多取两个地点；旧 v2 只有路标。扫描半径上限 32 cells，不能无限搜索。
- 双配置客户端、WorldRuntime、Soak 构建及聚焦/完整 WorldRuntime；相关 nominal/stress 短 Q3。
- 三 seed 地标原图、近入口与夜景，连续诊断保留真实时间点；相同地点与机位进行常驻/快速
  流送各三对性能，P95/P99 中位数比不超过 1.10。记录网格、驻留和生成成本。
  诊断截图不等同正常移动，正常持续输入按用户决定暂缓。最新证据见
  [r31 记录](../reports/landmark-architecture-r31-2026-09-19.md)。

### E9 岩台地表过渡 terrain v15

- `bash scripts/verify_terrain_surface_transitions.sh Debug <新目录>` / Release：八 seed 的高度/生态不变、保护地表、连片性、signed 极值和原土带变化；旧 Foundation 实际负例与旧样本哈希对照。
- `HELLOMINE3D_WORLD_SMOKE_FOCUS=E9`：48 处实际表层与公开规划一致、植被支持、四区块反序指纹和默认 v15 改块保存重开。保留显式 v13/v14 生命周期用例。
- v1/v14 生产 T0 及 v2–v14 地标/区块与冻结基线逐字节一致；v14/v15 只忽略预期版本列比较高度/生态和地标规划。双配置完整 WorldRuntime、短 Q3，生成成本单独记录。
- 三 seed 原图、实际草/沙/林地边缘、有效连续相机、夜间/兼容材质及受影响常驻/流送三轮帧耗；正常 v15 新建/保存重开。倍率/图形能力变化和穿入地形的诊断镜头不得计为同协议通过。
- 本批真实进度与失败保留见[记录](../reports/terrain-surface-transitions-r32-2026-09-20.md)，不以 CPU 成本替代图形门槛。

### HUD 交互

按 [HUD 交互合同](../contracts/hud-interaction-contract-v1.md) 分批检查。`bash scripts/verify_hud_interaction.sh Release|Debug <新输出目录>` 覆盖页面转换、Esc 消耗、输入释放门与失焦；物品详情同时需要真实窗口检查。任务／地图增加对应真值和有界采样检查，完整交付采用双配置客户端、世界与资源回归。

HUD 另覆盖标记跨画布／列表的名称缓冲切换、地图异常信息优先级；`verify_exploration_atlas.sh` 覆盖指针朝向与北向导航一致性。HUD 地图纯检查与输入检查共用 `verify_hud_interaction.sh`，分别覆盖正交方向、真实高差、可见墙面、未知列空白、深度拾取、最大网格、快速拖动释放与平移边界。`capture_visual_macos.py --panel map|journal|pointer` 是显式后台诊断；物品内容图加 `--hud-fixture --inspect-slot 0..4`，不作为普通 hover 的证据。双语小窗口和浮层遮挡应查看原图；关闭 HUD 的性能与暂停页面成本分开比较。

地图增量专项：移动／对角移动／未完成时再移动／范围改变保留精确世界坐标，新增边缘先行；
锁忙和旧中心回复不推进，完成边缘后仍刷新真实编辑。细历史覆盖负坐标、页接缝、容量淘汰、
最大实时窗口不自我淘汰与切世界清理。窗口专项检查关闭→移动→重开、重复开关和手动视图保留；
纯值缓存检查不替代正常行走、编辑后的实机截图及三对性能。
