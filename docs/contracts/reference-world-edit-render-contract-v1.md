# Reference World 编辑到光照／倒影的工程合同 v1

日期：2026-10-06。状态：r4 有界工程编辑、独立 oracle、故障副本和真实负控已验证；普通输入及重开验收仍 **NOT_RUN**。适用当前参考画质 Goal 的 C/D 整合缺证；不关闭提示词第6节退出条件4。没有新的 shader、World 状态、存档版本或物理语义。

## 范围与隔离

单独、默认关闭的工程入口 `HELLOMINE3D_REFERENCE_EDIT_PROBE=lamp|wall|shore`。每个模式在一个自有隐藏、noActivate 客户端中按 baseline-a → edited-b → restored-a 验证，原有13阶段生命周期入口、895检查 oracle 和旧证据保持原样。禁止同时开启旧生命周期、性能、其它 fixture、capture alias、资源覆盖、validate-only 或 manifest 输出。

自有 session 包含 `Runtime.app/Contents/Resources`、`save`、新的 `catalogue`、新的 `edit`。标记为 `.hellomine3d-reference-world-edit-owned`，内容精确为 `HelloMine3D owned reference world edit session v1` 加 LF。输入包和样板从已核对身份的正式包独立复制，原客户端、用户存档和模板全树受保护。只终止本次自有子进程，默认拒绝已有游戏；显式忙宿主工程案例须记录完整周期进程清单且隐藏 noActivate，不操作其它窗口，不称隔离性能。

内部上限为30秒、2048帧、每阶段8秒、journal最多64条；每阶段至少12次真实正常帧并满足世界／上传 readiness 才能记录检查点。外部 runner 另有有界 watchdog，超时及非零实际退出均失败。工程过程不注入 OIS／系统输入，`normal_input=false`、成功输入数0。

## 权威编辑与阶段

所有变更经过真实 `World::setBlock` 及既有 relight／depth／MeshDirty／后台构网／正常上传确认，不能直接替换 Ogre 节点、伪造 revision 或重放一套测试 renderer。

| 模式 | A：必须实读的原始方块 | B：变更 | A′：恢复 |
| --- | --- | --- | --- |
| lamp | `(201,69,-186)`，Lantern44，原 metadata | Air0 | 原 BlockId 和 metadata |
| wall | x199..200、y68..70、z−196 六格 StoneBrick35，原 metadata | 六格 Air0 | 六格原 BlockId 和 metadata |
| shore | `(194,66,-183)`，Water7，原 metadata | Stone3 | 原 BlockId 和 metadata |

记录编辑前后每格的真实 BlockId／metadata，以及对应 section 的真实 blockRevision、本帧预算内 offered CPU-ready、当前 uploadedRevision、实际 VBO／IBO。B 和 A′须证明 revision 增加，真实 setBlock 后、正常 update/upload 前的 mutation 记录中 liveRevision 领先原 uploadedRevision，再确认正常上传到当前revision、驻留状态可绘制。现公开快照不暴露 ChunkMeshState 的 Dirty 枚举，此处不宣称直接读到了该枚举；setBlock 走既有 markDirty 语义，工程观察的是实际版本滞后和随后当前驻留。`collectSectionMeshSnapshot(false)` 的 CPU-ready 列表仍是本帧预算内上传候选，`cpu_ready` 保留为 `offered_cpu_ready` 的别名，不假称完整 ChunkMeshState 查询。独立 oracle 根据真实 live/upload 版本、upload_known、GPU 驻留及 offered 列表推导 selected_ready；World input invalidation 先递增 blockRevision，未上传新输入不能冒充当前版本。checkpoint／mutation／failure 的 `section_readiness` 保留同一次实际快照的 selected sections、全局CPU-ready total／deferred／offered；独立核对候选计数和别名一致，全局背景队列不要求为0。当前版本不重试失败帧，成功仍为固定14个事件、4份真实RTT；若目标本身在帧内失去当前上传，继续真实FAIL并保留失败快照，不新增未验证attempt协议。仅有 World visualRevision 或 Planar updateCount 增加不充分。模式之间互不混合，未编辑控制格保持原始 ID／metadata。

## GPU 数据与受光

每阶段从实际生产上传引用导出有界 CPU VBO／IBO 字节，并从对应真实 native GL buffer 读取原字节。保留 buffer名称、类型、GLisBuffer、容量、顶点stride、index类型、section到buffer的映射和文件SHA；实际 VAO 的已启用 vertex／uv2 等属性须读同一 VBO，GL_FLOAT、44B stride 与0／12／20／28／40 offsets一致，无normalize／integer／divisor。独立 oracle 检查实际文件内容、大小、边界与CPU/GPU逐字节一致，不能只比较调用者写入的hash或计数。读取只观察原对象，须恢复绑定并记录GL错误；不修改 buffer 或重新draw。

局部光包含 World 的真实最多8个 source snapshot：position／radius、线性colour／energy、revision，以及有界查询成本。记录原生生产 draw 之后实际链接 GL program 的 localLightCount、两组8×vec4数组，以及主 production pass 和当帧镜像private pass的material／program／attached shaders／camera／frame身份。由独立 oracle 推导预期数组，unused项为0。lamp的B必须精确移除目标source，A′恢复。额外固定、未编辑的 StoneBrick35/m0 `(201,68,-192)` 的 +Z 面为受光观察面：四角 `(201,68,-191)`、`(202,68,-191)`、`(202,69,-191)`、`(201,69,-191)`；邻接四 Air 样点 ID/meta 不变，分立 World blockLight 应下降并恢复。独立 oracle 从实际 +Z 三角形在面中心作重心插值读取 GPU uv2.y（packed blockSource+1），须下降并恢复；不要求白天可能保持满值的 sky／combined light 下降。GL均匀参数明确标为 actual-linked-GL-program-after-production-draw；不改shader、不另画fixture。

## 可归因的真实像素

请求 pose195.5 68.02 −176.2／rotation5 45 0、FOV90、1280×720 points，实际2560×1440 HDR4；旧191.5机位不符合12m灯索引半径，不修改历史证据。模拟步长为0，world time6000、动画时间4，并记录每阶段实际主相机位置／方向／镜头、演员实际pose、主视图与镜像矩阵、选定planeY、GPU globalTime等冻结事实。冻结只在显式诊断中生效，普通模式保持原行为。阶段frameSerial继续真实推进，正常Planar update每帧最多一次。frame_input_scene_revision严格是原reflection FrameInput latch，planar scene_revision须精确相同；frame_local_light_revision是原query snapshot，later_current_world_visual_revision（亦保留旧world_visual_revision别名）是检查点较晚读取，不假标成原frame输入。后台World可合法前进，要求local≤input≤later，目标真实live/upload/actualdraw仍为同当前版本；mutation的global revision只须不大于原frame input，不能用后来current强制等于旧frame。

RTT读回来自实际生产半尺寸RGBA16F目标的原生FBO，float32文件保持GL bottom-left行序和宿主字节序。记录同frame的native附件／深度／尺寸／samples／budget／GL状态恢复、实际绑定／shader身份、updateCount和sceneRevision。主窗口PNG来自同一正常frameRenderingQueued，不能从RTT preview冒充主帧。其文件存在和实际2560×1440尺寸必须验证；HUD／caption／UI clock可合法变化，不用整PNG字节相等证明场景恢复，不额外冻结产品UI／Audio。既有 `captureDiagnostic` 一生最多4次的预算不放宽。

采用A0无编辑对照、A、B、A′四份真实读回；A0是baseline阶段前一个正常frame，仍只有三组begin/checkpoint/end。冻结条件下A0与A整RTT字节相同，否则先保留FAIL并定位未冻结因素。编辑格在水面裁切平面以上的几何部分由独立CPU根据实际main pose／lens／planeY推导镜像投影，再加固定4px padding得到ROI，不信任运行时给出的ROI。四个先验固定16×16天空控制区统一取 GL底部行序 y=ceil(.01H)，x分别ceil(.01W)、floor(W/3)−8、floor(2W/3)−8、W−ceil(.01W)−16。独立 main pose/lens 构造reflection ray，四域所有角ray必须从reflectionEyeY<clipY出发且World directionY<0，永远不进入保留的World半空间；实际A各域须为恒定天空raw，全部A/B/A′精确相同。此域不与编辑／受光ROI重叠，不在看到B差图后挑位置。

B须在目标ROI有真实改变，A′整RTT恢复A原字节。lamp另须在上述未编辑受光面的独立投影ROI减去emitter投影ROI后的像素上有改变，避免实体移除独自满足照明门槛；World blockLight、实际原VBO uv2和主／private linked参数同时成立。四个天空控制区亦须不与受光ROI相交且保持原字节，不能用任意全帧变化替代。若精确冻结／恢复门槛失败，保留实际失败并解释原因，不临时增加epsilon或删像素门槛。

## 正常保存及独立判定

A′恢复后调用正常 World.save，记录真实返回及save事件。外部 runner 保存退出后 clone 的最终文件hash和world.meta原文；独立 oracle 直接解析当前二进制chunk v2的目标格、未改受光格／Air格 ID/meta，与原模板及真实baseline交叉核对，并核 world_id 不变。正常保存可能改变元数据／其它合法文件，不要求全save树字节等同模板。此版本不新建运行时World重开尾状态，普通保存重开和隐藏 fresh-World重开均未由本工具证明。

journal和summary记录准确mode、三阶段、same Root／Scene／Window／World、编辑事实、文件证据和最终状态。外部run记录实际进程退出、loaded executable、源码与资源收据、包／模板保护及忙宿主限制。独立 `tools/tests/reference_world_edit_oracle.py` 自己推导geometry ROI、source参数、字节和因果图像，不以runtime_pass、updateCount或summary COMPLETE代替判定。

真实负控 `HELLOMINE3D_REFERENCE_EDIT_FAULT=skip-reflection-update` 只在B阶段抑制正常Planar.render，真实World编辑／正常update及上传仍继续；达到当前上传readiness后记录旧A RTT和B的真实World／GPU数据，再因缺少实际同frame反射draw／更新明确失败。必须自然非零退出或被独立oracle拒绝，保留实际旧RTT。其余校准须先有一份实际完整正例，再复制故障证据，覆盖缺阶段、重复、假freeze、旧revision／上传、假GPU hash、旧主／private灯参数、陈旧RTT、缺控制、错误恢复、GL错误与非零退出。禁止生成一份虚构的成功journal。

## 当前证据

权威聚合为 [r4 independent-aggregate-r1.json](../../build/reference-visual-implementation/reference-world-edit-native-r4/independent-aggregate-r1.json)，从实际run／journal／summary／oracle／loaded executable与文件SHA生成；此审查启动原生进程0次。构建为 reference-world-edit-build-r3 的 Debug／Release，两者真实exit0，7个核心源码的构建前／后及当前SHA逐项相同。D编辑4文件冻结简称bb7e3771，完整7文件map和有明确编码规则的聚合指纹保存在聚合中。包创建身份为 HEAD671aea60dbc45f82f3737e829c45148829d281da 加 tracked dirty diff66397528760bee0f628ddeca1e6de767050c3c8d16abf0626f499dc4523e3511；工程实现当时未提交，不能把创建HEAD单独视为全部实现。5个实际session均核正式全src2985文件收据37c29587d03af3ffc1a223164c3432da75246efedf84979555f2b228fbe51131、源码证书、资源收据及实际loaded binary。Release二进制3c426d5b49d6eeafc82f04b053a8c1408e2e40ee39c5ffce859dbdeb820459b2；Debug83a8916b7d94e889ee839875be687d72a0d716a67c35a2ef65bdadad06768258。

独立oracle最终SHA为e55a28a825d4170c323052fd88dcb622871336e626364b6acd961c6a8450cf2d。结果仅适用本工程范围：

| 实际session | 原生退出／PID | 独立检查 | 校准（实际正例＋故障副本） | 权威证据 |
| --- | --- | --- | --- | --- |
| Release wall | 0／16706 | 265／265，0FAIL | 33／33＝1＋32，32负例全拒 | [当前oracle](../../build/reference-visual-implementation/reference-world-edit-native-r4/release-wall/oracle-calibration-r2/actual-positive.json)、[修正后校准](../../build/reference-visual-implementation/reference-world-edit-native-r4/release-wall/oracle-calibration-r2/calibration.json) |
| Release shore | 0／16892 | 283／283，0FAIL | 33／33＝1＋32，32负例全拒 | [oracle](../../build/reference-visual-implementation/reference-world-edit-native-r4/release-shore/oracle-r1.json)、[校准](../../build/reference-visual-implementation/reference-world-edit-native-r4/release-shore/oracle-calibration-r1/calibration.json) |
| Release lamp | 0／17175 | 294／294，0FAIL | 35／35＝1＋34，34负例全拒 | [oracle](../../build/reference-visual-implementation/reference-world-edit-native-r4/release-lamp/oracle-r1.json)、[校准](../../build/reference-visual-implementation/reference-world-edit-native-r4/release-lamp/oracle-calibration-r1/calibration.json) |
| Debug lamp | 0／17472 | 294／294，0FAIL | 不重复同源Release校准 | [oracle](../../build/reference-visual-implementation/reference-world-edit-native-r4/debug-lamp/oracle-r1.json) |

四正例共1136项检查，0FAIL；校准101项＝3个实际正例＋98个拒绝的故障副本，无虚构成功journal。每正例真实14个事件、39帧、4次RTT读回，三组主阶段与A0control完整；全部正常World.save返回true、输入0、原包和模板全文件保护true。目标section的实际当前live/upload、驻留、原CPU／真实GL字节、VAO44B属性、primitive与GL灯主／private参数均通过，未用summary或计数替代。A0／A／A′整RTT逐字节相同（SHA12b9b12856bbaadf30fc8e6a07c9d7273247f125f52a57e6afa6a7e6416b75ba）；B独立ROI变化为wall1154px、shore6239px、lamp emitter2205px。四个预声明天空控制域始终原字节。灯的未改receiver扣除emitter后993px变化，actualGL uv2.y为1.441666662693→1→原值，World blockLight8／9／7／8→全0→原值，真实主／private source1→0→1。wall／shore／lamp独立解析正常保存clone各11／6／6格原ID/meta与world_id恢复，未新建运行时World重开，也不证明普通保存重开。

背景CPU-ready未错挡目标也未跳过目标版本：Release wall的A全局total51／deferred43／offered8，shore43／35／8，lamp46／38／8；Debug lamp111／103／8，所有selected目标仍实际current／resident／offered false。旧CPU-ready是预算内offered别名，新字段与同一实际快照一致；上传中的mutation记录严格显示live领先旧uploaded，之后实际current正常到达。

[真实skip-reflection-update负控](../../build/reference-visual-implementation/reference-world-edit-native-r4/skipped-reflection/independent-negative-review-r1.json) PID18709自然exit1，native status **FAIL**，独立oracle **FAIL**，不能算正例PASS。负控142项实际观察确认：World灯44→Air0、mutation live819／uploaded817→当前819、主GPU字节与原CPU相同且linked灯count0；B真实frame26／inputrevision428976没有当帧private draw，RTT仍frame13／revision428975／update14，A0＝A＝B全字节陈旧。native在缺实际same-frame main/private PBR draw处失败，summary仅完成1主阶段／3读回，time和失败方块恢复true。退出clone实际解码灯44/meta0恢复；这是cleanup保存旁证，未证明fresh World重开。负控标识为NEGATIVE_CONTROL_REJECTED，同时明确保留原生和oracle失败。

所有session为BUSY_HOST_ENGINEERING，有其它游戏及周期完整ps记录，不称性能隔离、速度增益或连续spawn trace。hidden/noActivate与普通输入0的工程结果不关闭普通编辑、设置重启、保存重开、行走卸载返回或全Goal退出条件。默认关闭路径只读复核未发现新可行动缺陷：无显式probe/fault时不创建对象或observer，不执行冻结／额外draw／读回；析构在resident和Scene之前解除observer，原GL绑定与query及时恢复。此结论不是额外普通运行验收。旧13阶段／895结果和VS16／FS8 strict压力失败均原样保留。

Release wall最初oracle-r1使用21226f38…，实际265／0仍保留；最终校准r2中的actual-positive使用e55当前工具。原wall校准r1仍保留：其保存故障固定灯区块，只触发hash一致性失败，未直接校准wall目标恢复。修正后的r2按实际mode目标、同步故障copy finalhash，真实被独立解码的save/cell(199,68,-196)/normal-save-restored-ID-meta门槛拒绝；不改运行时、门槛或旧记录。

命令：`python3 tools/tests/reference_world_edit_oracle.py --session <actual-session> --output <new-oracle.json>`；`--calibrate` 将 output 作为新的校准目录。无完整实际正例时禁止创建正例PASS；非零native fault仅出FAIL及真实partial记录。

首个Release lamp原生r2自然exit1保留：末端误把原frame42 scene/light revision429034与较晚World429055比较；原版本新合同尚未验收。其四RTT实际A0=A=A′全字节同一SHA12b9b128…，B编辑ROI2205px变化，未改receiver1224px中993变化；原四角中性假设的corner3实为Grass1 `(198,66,−176)` 地面，独立ray交点距灯10.93–10.96m<12，B178px合法受光/A′恢复。新四天空控制域在下一原生case开始前声明，原r2不改成PASS，不通过事后挑不变像素解释生产正确。

原生r3 wall／shore在baseline自然exit1、readbacks0的失败保留。旧失败没有记录末端selected快照，不能断言全由全局背景计数触发；新版本仅移除无关全局队列耗尽门槛，目标版本／上传／GPU／像素门槛不放宽，并在failure保留最近实际section_readiness。原r3 lamp的276检查／31项校准只适用当时冻结版本，不据此宣称新观察字段的完整原生验收。
