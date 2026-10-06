# Reference 驻留卸载与返回渲染工程合同 v1

日期：2026-10-06。状态：r3两个固定目标的有限工程实证完成；r4有限画面见证真实失败已保留，Clean CPU重传的World双配置和r6同帧观察／有限返回恢复实证完成；r4／r5真实失败保留，普通输入与设置重启分别记账。本合同补充完整参考画质 Goal 第5节的“卸载返回”工程证据；不关闭第6节普通输入退出条件4，不替代设置切档重启。原13阶段生命周期、四次读回编辑合同与其证据保持各自版本和范围。

## 默认关闭与自有目录

唯一正例入口为 `HELLOMINE3D_REFERENCE_RESIDENCY_PROBE=1`，输出 `HELLOMINE3D_REFERENCE_RESIDENCY_DIR=<session>/residency`。无 probe/fault 时立即返回，不创建对象、订阅事件、复制网格、查询GL或改变正常模拟。故障仅接受伴随合法 probe 的 `HELLOMINE3D_REFERENCE_RESIDENCY_FAULT=skip-target-retirement`；未知值或无 probe 的 fault 明确拒绝。

session 内仅使用独立 `Runtime.app/Contents/Resources`、`save`、新 `catalogue` 与新 `residency`。所有路径为绝对规范路径，子目录、marker、world.meta、chunks 不得是符号链接，类型严格检查。marker 为 `.hellomine3d-reference-residency-owned`，内容精确为 `HelloMine3D owned reference residency session v1` 加 LF。输入包与真实模板先核源码、资源、二进制和全文件身份，独立复制，退出后原包与模板逐文件保持。三个阶段使用同一份真实保存目录，不能在返回前重拷模板。

仅允许隐藏、noActivate、HDR4 的严格独立工程配置；主 capture 为1、定时60000ms、自动退出0，超出本探针内部上限，不启动性能采样。拒绝其它生命周期、编辑、fixture、镜头路径、readback、fallback、资源覆盖、validate-only 与 manifest 输出的混用。普通 OIS／系统输入不注入，所有输入回调数0。默认 runner 拒绝已有游戏；显式 busy-host 工程运行保留完整周期进程清单、实际自有 PID 和 loaded executable，不能宣称性能隔离或普通交互。

## 三阶段与生产路径

三阶段固定为 `resident-a` → `departed-b` → `returned-a`。一个实际 Root／Scene／Window／World 持续存活，world_id、save_directory 和保存身份保持。经正常 `WorldManager::teleportPlayer` 移动真实 Player，更新逻辑相机，并调用原 `SandboxRuntime::update` → `World::update` → demand／有界卸载／正常同步上传。禁止仅移动 render camera、调用 ChunkManager 强制卸载、直接删 Ogre 节点或重放测试 renderer。

A 机位和朝向在入场配置中预先声明；A 检查点保存实际稳定 Player／逻辑相机位置。B 为原点 X 方向正12个区块，真实 Player 与逻辑相机共同离开目标的 near／resident demand；返回精确使用 A 已观察的 Player 位置和朝向。真实模拟 delta、固定 tick、世界时间、AI／actors、光照与动画继续正常推进，不冻结时间以获得 A/A′ 像素等价。返回要求实际 X/Z、朝向及逻辑相机与 A 相符；合法地面物理沉降通过请求位置与实际值分别记录，不伪造相机。

两个逻辑移动记录为depart／return；实际使用三次 `teleportPlayer` API：depart先到远处同高度以正常预载，再依据远处已驻留SurfaceMap高度落到安全Y，return一次回到原位。不是两次API调用；不使用render-only camera证明World卸载。

固定监测区块 `(12,-12)`、`(12,-13)`，目标 section 为 `(12,4,-12)`、`(12,4,-13)`。本次 r9 实存模板目标为：

| 位置 | BlockId／metadata |
| --- | --- |
| (201,69,-186) | Lantern44／0 |
| (194,66,-183) | Water7／0 |
| (199,68,-196) | StoneBrick35／0 |
| (196,66,-184) | Step33／2 |

这些值必须由输入模板独立解码和实际锁内观察同时证实，不能从旧 r4 Water metadata3 编辑存档混用。模板的两份 chunk 文件及 world.meta 进入 runner 身份收据。A 与返回的实际 ID／metadata 相同。B 禁止读取目标 getBlock 或触发 getChunk/getOrCreate/preload；实际目标缺席由生产卸载后数值事件、resident-only SurfaceMap knownfalse、当前 liveSections 与实际 Ogre对象共同证明，不能拿缺失区块返回的 Air 代替。

## 权威数据与同步观察

权威 World 身份通过新增 `observeWorldIdentity()` 在一次 `m_mainMutex` 锁内按值复制实际已加载的 worldId／seed／terrainGenerationVersion；disk world.meta 为独立身份旁证，不能冒称 actualWorld。该最小 Query／Diagnostics 入口已纳入 architecture 责任图与 public-surface hash，不改每帧debug聚合、World算法、事件或存档格式。

probe 在实际 World 已建且 loader 尚未启动时安装 ObserveOnly 订阅。只监听两个目标的 ChunkSaved／ChunkUnloaded／ChunkLoaded；生产发布点已持 World mutex，回调只复制坐标、present、data_residency、incarnation、needs_save、cells、from_storage 等数值。回调不能再次调用加锁 World API、写 journal、调用GL或保留 Chunk 指针。其它事件仅累计数量。独立 mutex 保护最多32条目标事件 ring，溢出失败，不丢目标事件伪装成功。

probe 与回调保持至正常 World close、loader stop/join、World／Sandbox 销毁完成；draw observer先解除并无 pending query，然后组件及 Scene／Root正常销毁。不按旧无订阅编辑 probe 的析构位置提前释放订阅目标，不在已销毁 event bus 上 unsubscribe。

A 与返回要求两个目标真实 resident、live section 当前、uploadedRevision等于本次 liveRevision、GpuResident 且不在本帧 offered CPU-ready 中。独立 oracle 自行由数值推导 readiness，不能只相信 ready=true；CPU-ready列表是最多8个预算内候选，global total/deferred仅记录，不要求全局背景队列清空。旧 m_materialIdentityMeshRevisions 空间卸载时未清，不能单独证明当前上传。返回需要真实 fromStorage 加载事件、新 incarnation、原ID/meta，以及实际新对象和当前上传；blockRevision跨 incarnation 可重置，不要求与旧实例单调比较。

B 必须两目标生产 ChunkUnloaded 后 present=false，known=false，无当前 near live section、render-state、section visual、terrain batch成员或场景原节点。B的本帧 `RenderObjectListener-before-native-draw` 主／镜像回调对退休目标访问数为0，必须与实际cache缺席及全部旧buffer退休间隙共同证明；该回调数值域本身不冒称B像素或完成GL draw。不能只根据总数量下降或 claimed retired=true。

## 原生 GPU、反射与资源预算

A／返回保存目标生产上传的原 CPU VBO／IBO 和实际 native GL字节，保留真实对象、上传serial、来源sections、linked program、原生primitive、44B VAO属性与主／private参数。独立 oracle读取字节核对 CPU/GPU、有限属性、索引范围、真实GL buffer、current版本、实际main与reflection draw；不通过调用者的SHA或计数单独给PASS。局部光查询仍最多27 sections、8 sources、12m半径，主和private linked参数与其当帧 World snapshot一致，unused项为0。World revision与帧输入／较晚观察分开记录，不将较晚后台修订强贴成本帧输入。

旧目标 VBO／IBO 在正常 destroySectionVisual／flushTerrainBatches 退役后、下一次 allocate 前观察 glIsBuffer=false；记录旧id／size／对象／source section／generation、GL error及原绑定恢复。不得保留 HardwareBufferPtr、TexturePtr、ChunkPtr 来延长寿命。后续合法 GL id／对象名称复用不构成失败；没有真正立即退休间隙的 journal 不足以证明释放。

HDR实际颜色／解算／owned depth附件、完整性、尺寸、samples、GL error由原生查询证实；物理像素最多8,294,400。Planar为主viewport向上取整的一半，最多2,073,600像素、一个颜色／depth目标、96 private materials、384 private passes。B没有 resident water 可inactive／释放目标；有合法远处居民水面时可继续正常更新，private缓存不强求0。禁止反射额外请求或保留旧 World 居民。

仅A和返回各一次真实线性RTT读回（总2次，不增加既有 lifetime4上限），同时保存facts／preview／main原图。原数据有限、尺寸／格式／预算正确，真实本帧 RTT更新、camera／clip／binder／linkedprivate来源一致。固定 World receiver +Z面(201,68,-192)与 wall +Z面(199,68,-196)由真实机位独立投影，检查有限有效ROI及非clear像素，并结合实际目标draw与原图复审说明可见性边界。正常时间与actors推进，不要求A/A′全图逐字节相等，不把合法变化称为回归；draw/ROI观察不宣称每个原始face的精确遮挡或完整普通视觉验收。

## 协议、结束与失败

journal为 `hellomine3d-reference-residency-journal-v1`，序号从1连续，含event／phase／frame／elapsed_ms／normal_input:false／input_event_count及snapshot。三阶段各一组begin→checkpoint→end；chunk事件、gpu-retired和真实teleport作为有序独立观察。snapshot含同Root／Scene／Window／World身份、player／main-camera／逻辑相机、真实world time／simulation delta、帧输入和later revision、target_chunks／columns／sections／cache／draws／local_lights／HDR／Planar。实际readback与字节文件必须属于本session、有界、不可覆盖。

内部上限45秒、4096帧，每阶段12秒／1024帧，检查点至少12次正常帧；journal最多128条、目标事件ring32、原生跟踪对象64、bindings16、每 armed phase draw facts32、每四字节文件最多16MiB、全诊断最多300个普通文件／256MiB。保留当前一loader、pending jobs≤128、每pass commit≤8、每帧offered上传≤8、每World update卸载≤8、boundary-mask cache≤2048等适用预算。外部 runner 设置自有有界 watchdog，超时只清理自己的child。

尾事件必须实际 normal-save返回true、world-cleared（World/Sandbox无、resident/cache/light零）、components-destroyed（Scene/Root尚活着且本组件owned native无）、root-shutdown。summary为 `hellomine3d-reference-residency-summary-v1`，只有三阶段和正常尾部完成才能COMPLETE／tail_complete=true；runner独立记录自然exit0／loaded executable／原包与模板保护。任何非零退出、超时、缺阶段、重复／乱序、保留目标／旧GL、错误上传、fromStorage／incarnation／ID/meta错误均真实FAIL。

唯一原生负控选首个实际目标并持续跳过该目标正常退役直到B检查点，保留真实旧对象／GL；B的无驻留与退休门槛必须拒绝并自然非零退出。失败后正常cleanup解除fault，保存和销毁实际对象，保留failure snapshot／partial journal。独立校准只从已完成的实际正例复制故障数据，不构造成功journal；覆盖只移rendercamera、假data缺席、残留cache/draw/GL、无退休gap、旧GPU字节／版本、返回非storage或同incarnation、错误World身份、假native附件／尺寸、非有限RTT、尾部缺失、非零退出、超时和假ordinary声明。

## 新增有限画面见证（r3后声明，r4失败后r6有限实证）

r3只监测两个房屋／水体目标，返回检查点虽满足其当前版本，后台仍有176／164项CPU-ready工作；实际两张Release主原图显示右侧白桦树干／大部分树冠未恢复。故r3不证明全视野恢复，也不得追认它满足本节新增门槛。

本节在新case开始前固定两个经r9真实chunk独立解码证实的见证：`(14,4,-11)`、`(14,4,-12)`；实际样点为树干 `(225,67,-175)=OakBark4/meta2` 与叶 `(225,72,-177)=OakLeaf5/meta2`。此前基于图像猜测的 `(13,4,-11)`不进入规范。仅A／返回在实际resident/current已证实后读样点；B不查询或强制驻留这些目标。

使用full xyz数值map记录原生产upload的revision／serial／frame；正常destroy间隙清map，不能用旧诊断修订map或简化坐标覆盖多个见证。return动作前记实际serial截止值与movement frame，并与真实return teleport事件同值；返回必须两个见证都有新post-return upload（serial大于截止值，uploaded frame严格晚于实际movement frame且不晚于该checkpoint），当前liveRevision等于实际uploadedRevision、GpuResident且未在该次snapshot offered CPU-ready；实际known样点ID/meta由独立oracle解码并绑定原模板chunk收据。独立oracle自行推导 `view_witness_sections` 与 `view_witness_ready`，新增缺一个见证、保留旧upload两项故障副本校准。正常renderer决定何时完成，不等全局队列为0；仍至少12帧／12秒／1024帧上限、两次RTT读回、原字节／GL预算不变，不增加World/GPU副本或正常玩法成本。

这只补所声明的有限可见树木恢复；即使新门槛通过，也不宣称所有远处背景、actors、普通行走或整幅viewport均已验证。新版源码字段冻结及真实原图／oracle证据由新session保留；旧r3实证保持原范围。见证版独立oracle已完成AST／CLI静态检查，SHA `337fed99e5ff7ddc25612b429fc45ab5b53a07eaa333f2f7cc7bfdb07c9d2a48`；新门槛实际r4失败；没有新完整正例，因此36项校准（1真实正例＋原33及新增2负副本）仍NOT_RUN，不由静态检查推出PASS。后续Clean重传的观察字段与校准将另行冻结，不追认r4或旧r3。

## 保留Clean CPU网格的有界重传（r4失败后批准，r6有限实证）

r4返回等待至1024帧上限时，原两个目标已current、叶见证已新上传，但树干见证 `(14,4,-11)` liveRevision415、uploadedRevision为空且GpuResident=false，后台CpuReady总数已经0。正常 renderer 离开Near会退休GPU；数据若仍由其它需求保留，World的已上传网格可以合法保持Clean。现有工作线程只重建Dirty、snapshot只提供CpuReady，缺少再次Near时从保留Clean CPU网格恢复GPU的连接。该缺口由实际失败与同包源码链支持；旧journal没有树体incarnation及其具体数据保留需求，不能将推断写成直接事件观察。

批准最小 `observeRetainedSectionMeshes(missing, cpuReadyUploads)` Query／Streaming：主线程先按当帧实际live版本筛选没有当前GPU表示的Clean候选，提供最多8个fullXYZ＋revision＋incarnation请求。runtime在World mutex内只find当前已有Chunk／Section，要求真正Near、数据Resident、meshState为Clean、revision和incarnation都匹配后按值复制原CPU网格；没有load/getOrCreate、makeMesh、markDirty、版本递增、事件发布或保存动作。返回独立retained replay数组和锁内数值事实，不能把Clean重传列入 `cpuReadySections` 或修改 `cpuReadyTotal/deferred` 的原CpuReady统计域。

当帧原CpuReady snapshot已提供的上传优先，重传复制最多 `8−cpuReadyUploads`，正常与replay总上传不超过8。新后台工作在本次snapshot后完成可等下一帧，不混用较晚checkpoint队列计数自证本帧预算。候选按生产上传优先序处理；不能先截无关Dirty／空候选而长期挤掉Clean恢复。renderer仅对missing-current-GPU请求；成功确认无三角的section可用fullXYZ＋revision＋incarnation数值身份记住，避免同一合法空section反复耗重传槽。该身份没有CPU网格副本，正常Near退出和fullWorldclear清理，版本或incarnation改变后失效。

见证facts必须新增实际当前meshState／incarnation，以及成功生产上传的source（原CpuReady或retained-clean-replay）、捕获源state／revision／incarnation。记录同次production sync的normal offered位置／数、replay requested／copied／uploaded位置与来源、combined上传尝试数，以及ack后的实际accepted记录（当前revision／incarnation／meshState、hasVisual）。每项见证的frame ledger必须在flush／ack／current确认后补齐，并与自身成功上传身份一致；不能只保留ack前复制记录。普通／replay尝试包括合法空网格，不把它们冒称全部完成了GPU资源分配。返回树干须实际retained-clean-replay、捕获源Clean且incarnation同A，才证明保留CPU路径的修复；若完全卸载后仅CpuReady重建，属于另一合法路径。独立oracle据此核真实来源与总预算，不只靠ready布尔、source标签或post-return serial。原树干见证不得借markDirty／强制getBlock加载／重新贴旧诊断map绕过恢复语义；两次读回、45秒／12秒／1024帧上限保持。

必要CPU验证须使用真实ChunkRuntime／renderer入口和原CPU字节：Clean retained再次Near的有限重传、非Near或非Resident不复制、stale revision／incarnation不复制、普通CpuReady优先且normal＋replay≤8、空mesh身份不反复占槽、读Query不改变state／revision／save／事件。原生新版再核current CPU/GPU、实际linked draw／RTT、固定树体原图与Clean replay事实。恢复Query／ledger和26条独立smoke已完成r5双配置实际验证：focused各26／0、full各4491／0；r5 Release有限原生实证完成，Debug／负控观察时序失败已保留，最新诊断观察协议已完成r6双配置实际有限验证；原失败与不同版本范围保留。不可将设计或静态审查记为PASS。当前World责任图100项（59 Query／38 Command／3 Tick），公开面SHA `9FF359DBFE6DB884939272104EDFACAAE782F2A5CE8FFBE23497896B4F2F50CC`；旧99／95／78历史保持。新增纯数字empty-section-upload身份在正常fullclear须为0，与其它cache一并核对。最新独立恢复oracle SHA `71f8a76d556e3e439231530a411d711e393715cc74e7493ae63159b02e3cc3fa`，AST／CLI检查已过；旧r5冻结7cd工具真实Release327／327和40项校准保留。新协议42项校准＝旧40项＋观察来源假造／列观察帧陈旧两项，r6真实Release正例派生校准42／42＝1实际＋41故障拒绝，Debug不重复同源校准。

## 同帧观察与 unavailable 域

`World.observeSurfaceMap` 是UI最佳努力路径：ChunkManager以try-lock取主锁，锁忙返回空batch；空batch不得冒称resident数据缺席。每A／return阶段在arm前取得一次完整size2目标列batch，保存每列 `observation=World.observeSurfaceMap-try-lock-complete-batch`、`observed_frame`，`column_observation_available=true`；已完成batch中的A／return known须全true，B须全false。不可在draw／原生读回之后二次观察并把TOCTOU结果称原输入。缺batch只在原12秒／1024帧预算内等待，畸形非空非2仍失败；B完整unknownbatch与同份缓存／原生对象事实一同检查。

两固定树体cell使用既有blocking主锁／find-only `World.getBlock` 取实际值；不存在或尚未loaded返回Air，不触发load/getOrCreate。见证均固定非Air，只有实际ID／meta与模板4／2或5／2一致才可推导known=true，记录 `observation=blocking-World.getBlock-find-only-nonAir` 和当前 `observed_frame`。独立oracle仍严格核ID/meta、Clean／incarnation／当前GPU／post-return serial与实际上传来源；每cell／列observed_frame须等checkpoint frame。这只修正观察域，不追认旧失败，也不新增World API或改变普通模拟。A／return实际facts和末端current检查在消耗两个原有读回预算前完成，帧末沿已取得的纯值，不再重复Map或witness读取。

## 当前证据

r3当前实际源码包创建于 `c304e5b7` 加dirty来源，完整src收据2986项，SHA `f2666e0d1c0675b099c81c7c42a065a227e233b212a5def5d6ef2c86a0b863c0`；资源收据 `f859433d9801ca4e41f34fc3c8ed91715d16f00ffb98e970f3b95870b956b91c`。这绑定r3实际二进制，不冒称后续新增见证／settings observer的当前源码。

| 实际证据 | 结果与范围 |
| --- | --- |
| Release `reference-residency-native-r1/release-positive`，PID29889，binary `862500d5f41993fc900662fdeee059ea4e3ef99f2076718bff866ea6c00385d0` | 自然exit0；44帧／3607.774875ms，3phase／2真实RTT；独立 `release-positive-oracle-r2.json` 274／274。 |
| Debug `reference-residency-native-r1/debug-positive`，PID30362，binary `380411c1390b844daa86c835802d09dc156e12d7d786f012c415c86d1d61569d` | 自然exit0；44帧／6067.846625ms，3phase／2真实RTT；独立 `debug-positive-oracle-r1.json` 274／274。同源不重复整套fault校准。 |
| `release-positive-calibration-r1/calibration.json` | 34／34＝1实际正例＋33实际数据故障副本，33项均在指定语义门槛被拒；零客户端执行。 |
| `reference-residency-startup-guards-r3/startup-guard-audit.json` | 15真实pre-resource／Root／Window早拒绝，均自然exit1、0.6087–0.6689秒，独立复核stdout／stderr SHA与原因；原source168与template56不变，不称native正例或普通玩法。 |
| Release r4 `reference-residency-native-r2/release-visible-witness`，PID34614，binary `a512c14bd0be79013719260f3686428aa99551cfb93ab06622826d47b3258024` | 自然exit1／nativeFAIL／冻结337 oracleFAIL（1检查／0通过）；2phase／1readback、1056帧／8388.328625ms，返回阶段恰1024帧上限。原两个目标current；叶见证serial3／frame45，树干live415／upload为空／GPUfalse、globalCpuReady0。输入source168／template56不变；旧证据及严格门槛保留。 |
| Release `release-retirement-fault`，PID30194 | 自然exit1／nativeFAIL／独立oracleFAIL；31帧／1627.141125ms，完成1phase／1readback。B两chunk数据已卸载、knownfalse，但`12_4_-13`实际node／两renderables／statekey保留，两个native对象的四个旧buffer仍alive；真实cache门槛拒绝。18项独立负控观察成立，不能称positive PASS；partial失败记录不独立证明每个保留buffer的后续GLdelete。 |

两正例各A的全部6个跟踪native对象在B前6次真正退休间隙内删除（12 buffer）；含正常尾部共12次buffer退休记录，组件旧GL删除观察11项、manager name缺席13项及独立镜像camera销毁。main／private实际draw各阶段共5份，原CPU/GL字节、44B属性、linked灯光及本帧RTT一致；返回两个目标ID/meta、fromStorage新incarnation、同实际World身份与clone保存目标均成立。World time分别 `6007→6014→6015` 和 `6005→6013→6018`，正常模拟未冻结。B背景无水允许Planar目标释放、private cache10条保持有界，不能把这称为所有缓存归0。

首次冻结oracle `babffdb6…`的真实 `release-positive-oracle-r1.json` 保留FAIL（223／224）：误将运行active=true门槛用于已clearSelection、active=false但仍allocated的release.before。经实际源码／事实复核并授权，专用allocated-native checker保持固定尺寸、samples／format、GL0、owned depth、旧精确id/name及删除门槛；未修改native源码或浮点阈值。修后工具SHA `6a6c27d6ce7186203d433f287cbe397bba1dc3df349adbef0965c7a1d1d04506`，历史源码副本 `oracle-source-r3.py` 留在build证据目录，纠正原件 `oracle-observation-domain-correction-r1.json`。

独立聚合：[residency-r3-aggregate-r2.json](../../build/reference-visual-implementation/reference-residency-native-r1/residency-r3-aggregate-r2.json)，SHA `da93e3837b980f269e8e0e3cc0e8d640d4ebbad38ed366960aef9023636b97c8`；r1保留并只纠正未来见证候选坐标，旧548实际检查／34校准／15入口结果不改。独立负控原件 `release-retirement-fault-independent-review-r1.json`，SHA `831f2d8bbab53bfe42bb1cefd1a5820f2d88985301e118fef998d59db28549ab`。

r4失败原件：[r4-failed-oracle-r1.json](../../build/reference-visual-implementation/reference-residency-native-r2/r4-failed-oracle-r1.json)，SHA `e27cac6a0ed41afcb2d8531590731eb9a8272aa4af3afd0675e05c4b0ef18371`；独立事实与源码审查 [r4-failed-independent-review-r1.json](../../build/reference-visual-implementation/reference-residency-native-r2/r4-failed-independent-review-r1.json)，SHA `2f1fada4b359372a8cc2d17682359bbb3a35b106710f0775e0861b6b3f4cb1b4`。冻结337工具另存 `reference-residency-native-r2/oracle-source-r4.py`；未运行校准或新客户端。

截至r4，本段仍是busy-host、隐藏、输入0工程：r3两正例证明两固定目标卸载／返回，不证明全视野、普通设置重启、普通行走／菜单／保存重开或完整Goal五退出。当时新增有限树木见证r4实证 **FAIL**，有界Clean重传修复验收 **NOT_RUN**；后续r5／r6实证另列，普通12路线保持 **NOT_RUN**。

## r5 World与混合原生结果

当前World实际source收据2988项before／after完全一致，Debug／Release RETAINED_MESH focus各26／0；full各4491／0＝旧4465＋26条新增，26个唯一断言名与源码／focus完全一致。Debug full自然PID40476／285.380029s，Release full自然PID41240／38.170468s。新summary工具仅更新计数与说明，SHA `504bd9fadc141a83d517e91c1a744f74cb9b4ac6c563f02b3c26635a5aed6d01`；实际两正例接受、26项parser校准全部符合预期、旧Debug／Release4465各拒绝，旧d45工具冻结副本保留。独立 [summary audit](../../build/reference-visual-implementation/retained-mesh-summary-r1/current-4491-r1/audit.json)，SHA `cb7e1f54c81185e599be56883eaead486ce1c6dd07e0923edb443d610f501fb8`。这些证明World／ChunkRuntime／retained Query／26smoke的实际版本；后续仅Bootstrap默认关闭探针观察修复不冒称这份full重新执行过。

r5 Release PID40465自然exit0，44帧／3550.044333ms、3phase／2RTT；冻结7cd独立oracle327／327、40校准＝1实际＋39故障拒绝。树干14,4,-11当前rev415、incarnation40同A，frame39实际retained-clean-replay／Clean，late ledger普通2＋replay6＝8，acceptedCurrent／hasVisual／当前Clean与incarnation吻合；叶14,4,-12合法新inc235、CpuReady frame44。A／return原主图真实复审，固定树干与冠体存在；不宣称全视野或普通移动恢复。尾清理实际normal save／join returned true、所有cache（含empty upload identities）0、owned组件和Root依次销毁；输入0、busy-host无性能隔离声明。

r5 Debug PID40656自然exit1／51帧，返回raw RTT与主图已生成但未形成返回checkpoint，错误为visible witness column not currently resident；真实失败事实仍显示树干inc40／Clean／replay frame37、叶inc209／CpuReady frame51、两者current GPU。SurfaceMap可能空batch的观察不可用与数据缺席未分开，不能把它判成生产树体再次丢失，也不能追认完整Debug通过。真实retirement fault PID40876自然exit1／31帧，目标两chunk已经Unloaded，12_4_-13 node／2renderables及4旧GLbuffer仍alive；native先触到第二次SurfaceMap目标resident检查，尚未到目标cache硬断言。保留这份实际FAIL／残留观察，新时序协议须另有Debug正例与真正目标cache拒绝负控，不能将旧失败换成PASS。

本轮混合结果独立聚合：[independent-aggregate-r1.json](../../build/reference-visual-implementation/reference-retained-replay-native-r5/independent-aggregate-r1.json)，SHA `7fed3a066e05e0ced23562e460ada39915276ad59510bd3a07415fa7618d4450`；状态INCOMPLETE_MIXED_R5_ENGINEERING_RESULTS，只有Release327／327作为完整正例。冻结r5工具在 `reference-retained-replay-native-r5/oracle-source-r5.py`，两个自然exit1及各1项Oracle FAIL原件保持。最新Bootstrap同帧观察版本 `ccf2f9b3d2385c96416162131a6c1e951d6b03bc912192e333df8996598c1575` 只改默认关闭探针观察时序，World／ChunkRuntime／normal sync／26smoke不变；本段保留r5结束时的待验记录；后续r6实证如下，不追认r5失败。

Summary工具随后只纠正模块说明中的实际函数名 `caseRetainedSectionMeshReplay`，最终SHA `ce2289f52269a7843d6acb699c49e5ab7d82efb944c5f873c17050af70396c6b`；模块docstring以外全部AST节点与已实测504版本完全一致，当前Release完整日志再实际接受，原26项校准／两旧4465拒绝按完全相同parser与常量复用。旧504工具冻结于 `retained-mesh-summary-r1/validator-4491-r1.py`，最终证据 [current-4491-r2/audit.json](../../build/reference-visual-implementation/retained-mesh-summary-r1/current-4491-r2/audit.json)，SHA `2ac578cad2884d6afb2712fb9d7fe2ab6c0e3bead9b996c7604de6550c6d276d`。

## r6当前有限恢复实证

当前r6 actual source收据2988项，SHA `a26fbe0ca249dc41cd6fdd08c72b58a5870a502f584b6d44851fff3147d8aec5`；包创建于 `c304e5b7` 加dirty来源，不能把随后本地提交冒称旧包创建版本。独立 [independent-aggregate-r1.json](../../build/reference-visual-implementation/reference-residency-observation-native-r6/independent-aggregate-r1.json)，SHA `d321cf3153d0f5517d5d73fd51d13dcdb4a8fe8d305a1ef86734ee46082f81eb`，仅关闭本合同声明的工程范围，不关闭普通退出条件4或设置重启。

| 实际配置／版本 | 当前有限结果 |
|---|---|
| Release PID43395，binary `b4a166017b3a93717e5e0f13b88017e717bf37913f3bbd30919f2ba32236de2c` | 自然exit0、46帧／3704.568625ms、3phase／2RTT；冻结71f oracle330／330。树干inc40同A、rev415、frame38 actual retained-clean-replay／Clean，late ledger普通0＋replay8＝8；叶合法新inc245、CpuReady frame46。 |
| Debug PID43744，binary `da68ada457f7600a705069084ff2069281ebd37a47c0be1d5dbe37a5aa6110d5` | 自然exit0、48帧／6133.804667ms、3phase／2RTT；冻结71f oracle330／330。树干inc40同A、rev415、frame37 actual retained-clean-replay／Clean，late ledger普通2＋replay6＝8；叶合法新inc209、CpuReady frame48。 |
| Release真实skip-target-retirement，PID43831，同Release binary | 自然exit1／32帧／1559.1235ms，native reason严格为old target visual retained，1phase／1RTT、tail不完整；完整同frame size2 unknown批次与两ChunkUnloaded事实存在，12_4_-13 node1／renderables2／4旧GLbuffer仍alive，直接命中目标cache硬门槛。独立Oracle实际FAIL（1检查／1失败）保持，标NEGATIVE_CONTROL_REJECTED，不称正例通过。 |

两个完整正例合计660项／0失败；Release校准42／42＝1真实正例＋41指定故障副本全部在预期语义门槛拒绝。新观测记录中所有列complete-batch域／observed_frame、树体blocking-find-only派生known域／observed_frame均与真实checkpoint一致。原两个tracked目标真实卸载、存储返回新incarnation／原ID-meta恢复，main／private PBR原生draw、linked灯参数和实际CPU／driver VBO／IBO字节分别验证。原生字节观察仅覆盖原两个selected目标section；两个树体见证证明当前GPU驻留／上传来源／accepted ledger与真实返回原图可见，不冒称为树体额外读取了原生buffer。

每完整正例有6个空间退役间隙／12旧GLbuffer立即dead；含正常尾共12间隙／24旧buffersdead。组件释放实际11旧GL对象dead、13manager names absent和private reflection camera名字absent；World-clear实际normal save／join returned true、所有缓存含empty-section identities为0。场景相机数World-clear→组件销毁→Root关闭为2→1→0，组件销毁时仍有活Root／Scene和主camera，随后World／组件／Root各正常归零。独立审查真正查看当前Release和Debug两份returned-a原主图，均为2560×1440，白桦树干／冠体、建筑／桥和倒影存在；只说明两固定见证及这两个有限静态视图，不证明全视野、连续普通移动、动态AA改善或等时逐像素一致。

真实负控发生异常后组件先释放，此阶段World／Sandbox仍存在；正常shutdown随后销毁自有Sandbox／World并join，再销毁Root，最终snapshot为Root／Scene／World／Sandbox不存在。它没有成功World-clear／normal-save尾，不能将异常清理说成完整普通保存卸载通过。输入全部0、busy-host无性能隔离声明；审查工具未启动任何客户端。

World完整验证按 [world-scoped-reuse-r1.json](../../build/reference-visual-implementation/reference-residency-observation-build-r6/world-scoped-reuse-r1.json) 的exact源范围复用：实际r5收据2988项中2987项与r6完全相同，唯一差异为默认关闭的Bootstrap观察时序；World／ChunkRuntime／normal sync／26smoke源不变。仍引用r5实际双4491／26及对应原日志，不冒称r6重新执行World，也不将旧895生命周期扩大成当前全源码PASS。后续仅SettingsHeader观察修复的版本另记，当前Residency实证效力不关闭其独立设置范围。
