# 有界真实水位选面观察 v1

状态：macOS Debug／Release 有界工程选面观察已完成，真实负控已被拒绝；普通输入与连续玩法验收仍 **NOT_RUN**。本合同补参考画质完整 Goal D 的实际选面变化缺证，不关闭普通输入、保存重开或第6节五项退出条件。原[跨水面合同](reference-water-transition-contract-v1.md)的四阶段、默认行为、预算及已发生证据保持原范围；本模式使用独立 schema 和独立 oracle，不追认旧单水位结果覆盖多水位。

## 默认关闭与拥有边界

仅在既有 `HELLOMINE3D_REFERENCE_WATER_PROBE=1` 且新增 `HELLOMINE3D_REFERENCE_WATER_SCENARIO=plane-switch-v1` 时启用。scenario 缺席或空值继续原 `above-a/collar/below/above-return` 协议；没有 probe 的非空 scenario、未知非空 scenario、orphan fault 必须在资源／manifest writer、Root、窗口及 World 之前拒绝。`retain-selected-plane` 只允许本模式；原 `retain-water-binding` 仍只属于原模式。普通无 probe/scenario/fault 的路径不创建 listener、观察器、World 查询或额外 readback。

沿用严格 session/Runtime.app/Contents/Resources、save、catalogue、water 布局与原 marker `.hellomine3d-reference-water-transition-owned`，内容精确为 `HelloMine3D owned reference water transition session v1\n`。所有原路径／祖先与 marker、world.meta、chunks 的普通文件／目录和无符号链接检查不降低。输出及 catalogue 必须全新；源包、真实保存模板和包括 v9 在内十一已交付包不能被刷新或改写。

入口负控只在新独立 owned session 执行，不修改旧跨水面原件：从新 runner 的17项合法环境基线生成三份错误环境。`orphan-scenario` 同时删除 Entry 和 WATER_DIR，保留其余自有路径／hidden／capture 安全配置；probe专有触发键仅剩新SCENARIO。这是必须被前置拒绝的最小触发负例，不是完整可运行基线，不能因旧DIR guard也会拒绝就算新SCENARIO已被识别。`unknown-scenario` 只将scenario改为bogus；`fault-scenario-mismatch` 在plane-switch-v1加入旧retain-water-binding。三者均要求实际自然exit1、完整正式异常前缀与对应精确reason、stdout仅三条已验证pre-entry前言，不能出现资源／Root／Cocoa／World日志或新water/catalogue/save输出。外部各15秒own-child上限；短进程未取得lsof时如实NOT_OBTAINED，不假造loaded证书。当前三个实际启动阴性均自然exit1，精确reason和正式前缀已核；结果只证明前置拒绝，详见执行记录。

原严格环境准入只增加 scenario 这个已声明变量；其他诊断、perf、fallback、fixture、validate-only／manifest 输出仍提前拒绝。初始配置和姿态保持原样：seed42、time6000、`195.5 68.02 -176.2 / 5 45 0`，windowed 1280×720 point、RD3、standard/linear-HDR、MSAA4、medium shadow、post off、first person；实际窗口／附件由 native 观察确认。仅显式捕获：Capture=1、MS=60000、EXIT=0、MAX_DELTA_MS=5000。

## 固定真实保存与生产操作

四阶段固定 `high-a → low-b → high-return → low-return`，预声明期望面依次为66.9、64.9、66.9、64.9米。全部同 Root／Scene／Window／World 实例与实际 World identity；同帧身份和 disk metadata 来源分别记录，不将文件中的 ID 当运行时查询。模拟 delta、World time、actors、streaming 及 animation time 正常推进。

只通过生产 `WorldManager::teleportPlayer`、逻辑 Camera update、正常 Sandbox update 建立有限机位。每阶段实际主眼非 Water、eyeY > 实际选面 + .15米；请求 eye 偏移为 +1.20米。固定高柱 `(194,−183)`、请求XZ `194.5/−182.5`；固定低柱 `(176,−176)`、请求XZ `176.5/−175.5`，在本轮新native前预声明，不能根据 B 差图挑选。成功 arm 前连续12个满足实际介质／World 选面及正常模拟条件的帧；请求 pose 不证明实际 pose 或选面。错误 component/driver 面在 observation 后判定，不纳入 warm 条件以致真实负控只超时。

当前 v9 正式保存的独立只读解码给出以下预声明值；这些只是保存候选，不自证运行时驻留。新模板须逐文件身份对应，并在各检查点用既有 blocking/find-only `World::getBlock` 实读两根水柱的水、Air 和 bed：

| 柱 | 必须保持的真实 ID/meta |
| --- | --- |
| 高 `(194,−183)` | y64/65/66 为 Water7/0；y67/68 为 Air0/0；bed y63 为32/0 |
| 低 `(176,−176)` | top y64 及下方固定 y63 为 Water7/0；y65/66 为 Air0/0；bed y34 为 Stone3/0 |

两柱都必须被实际 World 查到上述非 Air 水／bed，不从保存查表、请求值或 shader uniform 派生 World 事实。Air 的 known 不能仅由 find-only Air 返回推导；同柱水／bed 已真实存在及当前驻留观察域须明确。每格保留 position、id、metadata、观察域和 observed_frame。真正 `World::observeWaterSurfacePlane(actualEye)` 是本帧选面权威；不能直接改它、写水方块或固定返回值。

## 同帧绑定与 native 证据

使用独立 `hellomine3d-reference-water-plane-selection-journal-v1`／`hellomine3d-reference-water-plane-selection-summary-v1` schema。每阶段恰一次 begin、observation、checkpoint、end，共16条；sequence从1连续有序。observation 必须在严格绑定门禁前持久化；checkpoint 消费同一份同帧数值事实，不在末端重新采样并混用不同时点。

各 observation 同时记录实际 Player／logic／main pose、正常 delta／World time、两柱实值、World 实际选面、frame_input_scene_revision、later_current_world_visual_revision、Planar selected plane／采样矩阵／lastRenderedFrame／update_count、真实 Ogre Water pass 参数／TUS，以及生产 Water draw 后的实际 linked GL uniform 和 sampler。全局 World revision 可因后台工作继续推进，不能拿 later revision 冒充反射帧输入。

四阶段均须 active、reason rendered，last_rendered_frame=current frame、当阶段 update_count > floor；World 选面、component 面、Ogre pass 面及实际 driver `planarReflectionPlaneY` 均匹配预声明当前阶段，不能只有 updateCount 增长。return 两次须反映新的本帧选面／矩阵，不保留上一阶段采样变换。既有plane容差仍为0.001米；采样矩阵须由实际主camera pose与World面独立推导镜像关系：主view仍按实际pose独立检查；镜像camera经生产 `Camera::setOrientation` 复制姿态后，按当前编译的F32 Norm次序再归一，独立重建该复制camera的view Vcopy。反射矩阵R的对角为(1,−1,1,1)、R[1,3]=2h，实际镜像view应匹配Vcopy·R；再用实际RS-depth投影与记录的RTT方向核VP，与Ogre pass及实际GL矩阵相符，完整16元素及独立XYW检查均保持5e−5。GL column-major仅作存储转换，沿用既有5e−5数值门槛，不据当前case放宽；这不是clip边缘像素因果证明。实际 linked enabled=1，TUS 名称、sampler ID 与真实 resolved RGBA16F 纹理对应；主 HDR4 为2560×1440，反射实际1280×720／single sample，格式／完整性／owned depth／GL0与状态恢复全部沿用原门槛。保留一份合法目标、同generation或同texture可合法复用，不强求选面变化必重新分配。

一镜像 camera、一RTT、一depth，private material≤96、private pass≤384，半物理尺寸和绝对像素预算不放宽；listener／query 不保留 World/Ogre 资源引用。反射仍只绘制真实已驻留居民，仍排除所有 Water、反馈、手臂和 UI，不新增流送或 shader。

各阶段显式既有 `captureDiagnostic` 一次并写一张主窗口 PNG：总共最多4次实际RTT读回和4张main PNG。独立 oracle 核真实原生附件、raw字节数／finite／信号、完整 PNG CRC／像素、捕获帧／camera／矩阵身份；Root 实际查看原图。正常时间／actors 推进，所以不把两次 high 或 low 结果当逐字节图像恢复 oracle，不据跨case差图宣称视觉收益。

本模式证明实际 World→选面→当帧RTT→pass／driver输入联结。其它水层像素是否消费 RTT，若没有独立定位的真实 native 像素归因，只声明既有 CPU 判定和未改生产 Water GPU 分支范围；四份 RTT 或 linked plane 不能冒充其它层像素 fallback 的 native 证明。

## 上限、恢复与真实负控

内部总上限30秒／2048帧；每阶段8秒／512帧、连续12个valid正常帧；journal≤32条、输出≤32文件／256MiB，显式RTT读回≤4。外部有界 own-process watchdog 独立记录实际自然退出／超时，只清理自己 child。不存在增加第二纹理、延长预算、宽松重试或隐藏故障的例外。

完整正例最后通过生产 teleport 恢复初始 Player position/rotation，实际核对恢复值并调用正常 World.save，summary COMPLETE／completed_phases4／normal_save=true／input_event_count0。这是 clone 的工程保存；不证明普通保存重开。异常也沿正常组件 cleanup，observer/query 在 World／Scene 销毁前 detach，native非零和未完成阶段保持 FAILED，不补造恢复或保存成功。

唯一真实 fault 为 `retain-selected-plane`：只在 low-b 将传给组件的面保持66.9；真正 World observer 仍返回64.9，World不改。保持错误直到 low-b 满足真实 World warm并留下 observation，严格 World/component/pass/driver 一致性门槛随后自然非零退出；summary FAILED。事实应能确认实际World64.9而组件／driver66.9，不得靠改 JSON 制造负控，不能把 expected native1 叫正例PASS。

独立 `tools/tests/reference_water_plane_selection_oracle.py` 只读实际原件并要求外部真实 process exit code。校准从实际完整正例制作故障副本，至少覆盖错误阶段／schema／身份／介质／真实水Airbed／World选面／旧组件面／旧driver plane／旧采样矩阵／旧RTT帧／sampler／GL或存储／raw或PNG损坏／预算／保存恢复／非零退出／假ordinary声明，每份命中指定门禁。不得合成正例、改阈值、移除旧严格VS16／FS8压力FAIL或改原4phase证据。

## 执行记录

[真实保存候选只读审核](../../build/reference-visual-implementation/reference-water-plane-build-r9/save-candidate-audit-r1.json)保持18项／0失败、0启动的原范围：v9全部56文件前后成员和SHA相同；48个主／备份区块完整v2解码（16主区块），上述两柱Water／Air／第一非Water底床符合固定值。r13 `street-msaa4/save` 的48区块逐成员SHA相同，仅复用保存区块域；不以保存解码代替下面实际World查询。原收据SHA为 `edbd7f2e31a01b8cb186fe2e29fbfac2f8ac09a026c5d0451f0abce67650a116`。

本轮r9双配置实际构建及新168／167资源包完成，正式2989源码逐成员SHA一致（包含 `.inc`），源码manifest为 `60d760b27ca79485228b826f4e820bdc978b9333c67a6478632c2a2541cea0c9`，143资源manifest仍为 `f859433d9801ca4e41f34fc3c8ed91715d16f00ffb98e970f3b95870b956b91c`。Release binary为 `1f23ca5ed547ec2c0221b3dcdc4132dc0530cf0f71413e3a238e5deac1d3309b`，Debug为 `739495700441960dbf5be196fee71025265e80dede6ceb712ca2eec816c51be2`；新工程包记录创建commit `b170d3aa`＋dirty `81763f02…`，不回写v9的 `2ccd0ccd`＋dirty `5a2c…` 创建身份。三个新增正式文件冻结为Header `07eb334a…`、Bootstrap `985fc698…`、Planar.cpp `d0c98ae6…`；完整SHA和实际包身份保存在下述聚合收据。

[当前Release原件](../../build/reference-visual-implementation/reference-water-plane-native-r1/release/run.json) PID88790、[Debug原件](../../build/reference-visual-implementation/reference-water-plane-native-r1/debug/run.json) PID88905均自然exit0，无watchdog／forced cleanup，正常delta／World time推进，四阶段检查点为12／24／36／48帧。实际World选面、组件、Ogre pass、linked GL plane依次为F32(66.9)／F32(64.9)／F32(66.9)／F32(64.9)，各阶段保持11格实际水／Air／bed和same-frame真实RTT／sampler／矩阵联结，4份14,745,600-byte有限raw RTT及4张完整main PNG。每配置真实保存恢复初始Player姿态；退出clone区块中的原11格ID/meta与实际World观察一致，World identity与正常保存一致；这不证明fresh World重开。冻结reader `6c96220d…fff0db` 的新独立读取结果为 [Release292／0](../../build/reference-visual-implementation/reference-water-plane-native-r1/release/independent-oracle-r1.json)、[Debug292／0](../../build/reference-visual-implementation/reference-water-plane-native-r1/debug/independent-oracle-r1.json)，[46种实际正例副本校准](../../build/reference-visual-implementation/reference-water-plane-native-r1/release/calibration-r3.json)全部命中指定拒绝门禁；未重复原严格VS16／FS8压力检查或改其FAIL。

两次reader纠正只修独立数学／物理参考：[最终纠正收据](../../build/reference-visual-implementation/reference-water-plane-selection-math-r3/reader-correction-final-r3.json)SHA `eecbca9443ecd22ada1ec2815640f6efe3c533ed8f39785c2b05147a19d123f0`。镜像camera实际 `setOrientation` 再作一次F32 normalization；当前macOS Debug／Release的真实反汇编确认Norm先rounded x²再三次FMA，独立从主camera四元数重建复制后的姿态，保留完整16元素及独立XYW门禁的5e−5容差。低位正常模拟样本实际Player65.4／velocity−2时，生产20Hz单次gravity tick及previous/requested65.5→current65.4插值给出合法eye区间F32(66.0)至F32(66.1)，main与logic相等，区间端点仍用原1e−5；stationary和非低阶段保持原eye=current+.6门禁。实际scheduler alpha **NOT_OBSERVED**，未推造其值。原Release64项镜像FAIL、86项pose FAIL、完整旧reader及校准未产物的traceback均保留，不能把旧FAIL改成当时PASS。

[真实retain-selected-plane原件](../../build/reference-visual-implementation/reference-water-plane-native-r1/retain-selected-plane/run.json) PID89047自然exit1／summary FAILED，仅完成high-a后于low-b第24帧留下observation：actual World64.9、component／pass／driver66.9；真实inactive、enabled0、TUS移除，错误低位RTT没有被读回或命名为current。独立 [oracle保持FAIL](../../build/reference-visual-implementation/reference-water-plane-native-r1/retain-selected-plane/independent-oracle-r1.json)，另注明 `EXPECTED_NATIVE_FAULT_REJECTED`；normal_save=false、restoration=null，不宣称故障完成保存／恢复。当前r9原模式[单独Release四阶段](../../build/reference-visual-implementation/reference-water-plane-native-r1/original-transition-release/run.json) PID89106自然0、[旧oracle176／0](../../build/reference-visual-implementation/reference-water-plane-native-r1/original-transition-release/oracle-r1.json)，仅证明原协议本轮仍可运行；其旧runner明确保护10包／2197文件，当前额外独立全11包／2423 baseline逐SHA保护另有实证。

[三个实际启动阴性独立审计](../../build/reference-visual-implementation/reference-water-plane-startup-guards-r1/domain-audit-r1.json)100／0、SHA `264a226980fc26eaa83d383276ced8fbf886eeca22d1f11dd4a0c51808678c7f`：PID89218／89234／89238均自然native1，耗时0.418／0.431／0.441秒，只出现精确三条前置日志和正式双前缀reason＋LF；实际lsof取得三个owned executable证书。每Runtime169／save56逐SHA原样，没有water／catalogue／bin saves新产物。

[独立聚合收据r2](../../build/reference-visual-implementation/reference-water-plane-native-r1/independent-aggregate-r2.json)266／0、SHA `a80427a7fa7247297ea264833d00a7a8c20a256f0f68c25d18d9f4cf10a802fa`另外核对实际当前2989源码、所有包／raw／PNG完整CRC及像素、saved cells、正常退出、owned lsof与before／during／after census。新mode的源168／模板56、原Runtime成员和包括v9在内11包／2423文件均逐成员SHA不变。首版聚合r1误要求旧原模式10包字典等于新11包字典，266项／1FAIL原件保留；r2严格按旧run所声明10包域核对，并以当前独立11包完整baseline补充，未放宽任何正式门禁。该聚合本身0native／0build，未声称独立审计者已看图或画质提高；定期census不等同连续宿主隔离。

本轮仅关闭有限的实际选面工程缺证：same Root实际World→current RTT→pass／driver plane、matrix、sampler四阶段联结及正常clone保存。其它水层fallback真实像素因果、clip边缘／水三角画质、普通水路线、连续移动／普通保存重开、12条普通路线及整个Goal五退出条件仍开放，不能据上述工程计数宣称普通验收完成。
