# HelloMine3D 参考画质完整交付执行记录

2026-10-06启动，持续执行至2026-10-07。宿主 Goal **active**（本次恢复审计第2个真实turn），未设置 token 预算。仅当[提示词第6节](../current/reference-visual-goal-prompt-2026-10-06.md)五项退出条件全部成立才 complete。工程与静态画面通过不等于普通输入验收通过。[方案](../current/reference-visual-upgrade-plan-2026-10-06.md) A–E 范围保持；过程中的旧版本和失败见[集成历史](../archive/reference-visual-goal-integration-history-2026-10-06.md)。

## 当前开发验证策略与水面定位

2026-10-07 用户明确要求验证轻量化。后续以受影响编译／定向检查／实际效果为默认，不每轮执行全矩阵或多次独立审计；完整适用验收集中到稳定交付候选。停止未运行的VS bit-exact私有候选，工具已恢复原HEAD，保留全部560旧门槛与旧FAIL；FS私有隔离候选亦未运行／未promote。

仅修测试夹具非二进制步长导致的非零中心误差：真实varying改1/256，原491门槛不变，新增实际一FP16 ULP位移精确校准及拒绝。Root本轮CGL自然1，493／7；中心32F与16F均精确(.25,.125,-.125,1)，两个位移校准PASS，GL0；七个legacy32F零差压力仍FAIL（最大2.98e−8…8.94e−8），原RGBA8／16F比较通过。见`build/reference-visual-goal/resumed-audit-r2/reference-water-dyadic-centre-gpu-r1/baseline-run.json`。生产VS／FS及当前v12未改，不把夹具修复称游戏画面改善。

当前B56整格StoneBrick岸邻面按生产网格规则不发Water侧面；同实际反射frame240推回的主机位估算，水平水面66.9在浅视角投影覆盖当前宽蓝带。这改变下一步：不再按旧全Step实验盲修整条“侧面蓝墙”，优先核实当前水平面及四个保留台阶凹口的局部表现。估算与10s主PNG未绑定同帧，不能作为逐像素归属证明；普通输入／最终动态仍未验。

## 最新r14洞口背景视距修复

洞口背景现复用普通几何的逻辑中心、viewRange、退场强度及fog色，露天零覆盖discard，地下strength0保留原暗色与mask。只改变Ogre背景表示；World遮罩、需求、2048面／696KiB固定预算、八面扫描／更新、碰撞及保存语义保持。真实owned clone接收四个typed参数，反射private pass深拷贝；完整旧接口保留原行为，partial／wrong-type／wrong-size拒绝。见[洞口合同](../contracts/cave-boundary-background-contract-v1.md)。

macOS arm64 Debug／Release客户端构建和两个168／managed167工程包均自然exit0；完整source2990／正式media143对应当前版本。Debug binary `9eb1b370488a133237b08531eacd34db9bca9aaba96cb26d587ec3b030f643aa`，Release `7a723fafeed02b5d90279672868c9b754881f2557d583b9646a54ed6de79a173`。创建身份固定`2a448fdab39523f797f4339f154c824054d720ea`＋dirty `09e288887360eb79599913f6393a478c6474896939f984ba6ec245dab909a98a`；旧13包2875文件与r13双包保持。[独立构建／Renderer核对](../../build/reference-visual-goal/resumed-audit-r2/reference-cave-boundary-range-independent-review-r1/receipt.json)56／0；World＋Tests215逐SHA未变，只复用历史4491×2／26×2该域，不称当前世界全量重跑。

实际Ogre Renderer双配置各90／0，严格编译及启动／背景／结束GL均0，覆盖原mask／预算／清理、owned参数、复制后的独立值及draw、旧接口和错误接口。生产Cave GPU41／0。HDR首次850／7自然exit1原件保留：新增Cave夹具未恢复调用者VAO，使后续caster失败；只修夹具状态隔离后[HDR r2](../../build/reference-visual-goal/resumed-audit-r2/reference-cave-boundary-range-hdr-native-r14-r2/actual-run-r2.json)851／0、自然0，原850项名称／期望不变，新增真实VAO／array-buffer恢复门槛。[独立GPU核对](../../build/reference-visual-goal/resumed-audit-r2/reference-cave-range-r14-gpu-independent-review-r2/receipt.json)保留首次FAIL，不能把CGL结果称普通玩法通过。

[实际主景目录](../../build/reference-visual-goal/resumed-audit-r2/reference-cave-range-native-r14/)包含旧r13地下基线、新r14街道RD1／RD3、水岸RD1、同一地下保存的昼／夜RD1与RD8共七例，另legacy街道r2一例；八例均自然0、stderr0B，每例两张2560×1440原图。条件为medium shadow／post off／MSAA4；HDR例实际linear-hdr／fallback0及4sample存储通过。首legacy25939虽native0，却因要求不适用的Planar诊断完整readback使wrapper FAIL；原件保留，r2去掉该诊断请求后26803完整capture成立。Root已实际查看原图：露天悬空sky黑块消失，地下暗边界与RD8后续岩壁保持；水蓝band仍OPEN，不能判整体街景／Water画质PASS。左墙黑L区只读核对对应保存的Air洞口及洞内岩壁路径，不再称已确认遮盖缺陷；主PNG同帧camera／depth／primitive与actual sunlight未证。机位／time为工程初始请求，同帧main／logic／time仍NOT_OBSERVED，未证明连续普通移动。

普通候选v12／r14已实际创建为`build/reference-visual-goal/HelloMine3D Reference Complete v12.app`，bundle `local.hellomine3d.reference-current-v12`，world `world-9e3124820f269c3627f43438c7d6891b`，prepared spawn `208.5 68 -169.5`；全226／managed167／save56、current2990／formal143已核，创建身份继承上述2a448＋dirty09e288。[创建原件](../../build/reference-visual-goal/resumed-audit-r2/reference-normal-v12-preparation-r14-r2/creation/run.json)SHA `7628dff6ec02c908c1d9a719b0fd40da7344bd47cc794ae73f6c7a46dc4f281d`；fresh作者构场28944自然0、两原图已实际查看，不算普通建造。旧包保留，新增v12后保护基线为14包3101文件；创建后helper追加v12四处保护，四真实CLI前置拒绝。离源码完整临时副本菜单PID30620自然0／stderr0B，实际映像与cwd吻合；Root查看5000ms原图，“继续游戏”列出完整样板，七env／input0／未进入World，保护14包3101与私有保存保持。外围PNG reader漏设tools/tests导入路径首次FAIL保留，[只读恢复](../../build/reference-visual-goal/resumed-audit-r2/reference-normal-v12-post-create-r14-r1/menu-reader-recovery-r2.json)SHA `0ab0209f9f1b21653693005e8ec8f6b1208f0834836e2e54bc6b01a889fa178a`重读原件通过，native未重跑。本批三张精选原字节图见`.local-evidence/reference-visual-20261006/delivery-r14-engineering/index.json`（SHA `1a4f8310c6ca26086418f5a332b0b3cc79ae9dc8b51231881299ed4510746fb1`），[独立主景图审](../../build/reference-visual-goal/resumed-audit-r2/reference-cave-range-r14-native-independent-visual-review-r1/receipt.json)只关闭有限黑块观察域。

下一步按轻量开发策略集中在当前水面实际视觉问题及唯一输入方的普通12路线／编辑保存重开；微小bit压力保留到稳定候选的适用验收，不反复扩展夹具。左墙保存chunk `d74e8525c2985c3272d15117a54add256f27237cb8d58641fd70feedd52ffb55` 在B56／HDR RD3／legacy完全一致；黑区路径更符合既有洞口，精确像素归属未知，不作为已确认破墙修复。原六参考图缺失不变，用户先让另一任务继续使用桌面的指示仍适用；ordinary input0／12 NOT_RUN，Goal active，退出条件未全部成立。

## r13工程、作者保存与实际画面（修复前检查点）

当前生产源码已补Water FS能力准入和80格整岸线，普通交付候选仍为受保护v11／r12。r13双客户端／双包168／167实际自然0；Debug SHA `8694ed40343b79b3f3725bb9f9c908d2bfbf785a206a37b31e06933b5773441b`，Release SHA `c8964d50703da954c5c3c40f01ffb6ce3909d6fc7fffc873e880944acd683dc3`，2990源码清单 `757f2c303cfd6a15e689558d372fc95689dca2652c8b653695ca5195ac7c9daa`。创建身份保持 `e794f7b584e6bdcf757783c691a6c4d0f11cc1e6`＋dirty `d9dfc4f694db16804b5b6964785088f6eb30692b591a98365edd0513566b8a07`；提交或后续修复不得回写包／原件。[独立构建包审计](../../build/reference-visual-implementation/reference-water-legacy-interface-build-r13/r13-package-independent-review-r1/audit-r2.json)71／0（0635cefc…ecd2d）保留首读media143／manifest146域误计的71／2 FAIL；三个bin模板仍逐SHA纳入全168／167。World／Tests215逐SHA未变，只复用4491×2／26×2该域，不能称当前全部World重新运行。

[九次接口／sampler独立恢复](../../build/reference-visual-goal/resumed-audit-r2/r13-native-independent-recovery-r1/receipt.json)SHA `05c7dc556f197812531b251ee10daff9c48520f002a715750c590d874cddb477`核当前src2990／media143／双包及13包2875文件。coherent旧四资源成员实际只三项SHA改变；r12基线8586已自然1，r13 old-hdr9509／old-legacy9574自然0、absent0／0／GL0、无RTT／depth／私材／附加shadow，HDR本身保持active。partial9639、wrong9675、optimized9698原raw FAIL_RETAINED／自然1各命中明确拒绝；一元素数组10140同样原FAIL保留，具体 `linked type/array for planarReflectionPlaneY` 在第一次RTT前拒绝，不将compile失败冒充该守卫证明。[五例审计](../../build/reference-visual-goal/resumed-audit-r2/water-legacy-interface-native-r13/independent-capability-domain-audit-r1.json)141／0，SHA f3e27193…069ba7；两个旧资源正例四原图已查看，画质未因此通过。

Release9726／Debug9810真实四phase各228／0、stderr0B；旧212谓词／37校准／EPS1e−5精确不变，只增每phase四项complete5／5／prepared门槛。Release原37＋新增3故障副本分别命中特定门槛，属于副本校准；真实negative9868自然1／nativeFAILED／oracleFAIL，collar的reflection TUS及driver flag仍1，错态原件先保存、normal_savefalse。它未创建fallback，不能称异常fallback清理已测。此三次加载旧r10保存World，r13新inc不自动改写旧保存；蓝侧带和局部三角／斜线继续开放。详见[能力合同](../contracts/reference-water-shader-capability-contract-v1.md)。

E源 JSON `47087fd4433a078d49da714200d4350f3895677bd712d20c77be5fbc5699be3e`、官方export inc `cef3234638276daea840a04298a13fcb5176763cf01a0cd769a441789c0ed656`只改变80个非入口岸格为StoneBrick，四StoneStep入口／其它作者操作保持。构场10343自然0，实际62967 edits／613regions／72000预算／16chunks／12kits／两布局／2树／normal_world=1／saved=1；[构场原件](../../build/reference-visual-goal/resumed-audit-r2/reference-shore-scene-ab-native-r13/construct-b/run.json)SHA `612c8a34f26c48bb4c9e3dd0d640b1ac7552b99a5e5a68fc948b61c1e6bb2230`。A是v11原作者保存、B是新r13作者保存，各56文件；A/Bstreet12728／13277与water13646／13831均自然0、只加载保存而不重新构场。[主保存与图像独立审计](../../build/reference-visual-goal/resumed-audit-r2/reference-shore-independent-saved-review-r2/receipt.json)67／0，SHA `8c1ee22da4df651cedfd4c07fa03dda34a9bbade78fe3c5e9ffc7b37fb0000ce`；原r1错误期待HDR active=1造成五reader FAIL保留，实际是active=linear-hdr／fallback0。80岸格＋4入口精确；全部16个主chunk实际差异83cells，另三格TallGrass metadata1→0（186,67,-210／213,67,-212／230,67,-211）。B仍有原作者spawn1024/140/1024、名save；普通候选准备须按既有helper只在新副本正规化，不能把A/B保存整体称仅80cell变化。

十张RD1原图实际查看，技术capture成功但画质FAIL：建筑大面积不可见、黑悬空片。源码viewRange的RD1露天end14m／RD3end46m解释楼体越界；两份同r13、同保存、同medium的街道对照只改RD1→3，实际14682／15195自然0且楼体出现；[独立RD3控制审计](../../build/reference-visual-goal/resumed-audit-r2/reference-shore-independent-rd3-control-review-r1/receipt.json)37／0、SHA `8f0635136b06217a4f909666b1e66585a5d4046d01552bee5658c934b9070f57`核actual CLI／settings／finalconfig唯一距离行及全文件保护。输出保存75文件／primary16，不称全部保存仅80变化。原RD1协议／所有图保留，洞口背景无同域fade是黑片候选，尚待生产验证。新control r1仅准备时漏actualsettings／finalconfig两gate且未执行，原件保留；r2新增实际逐字节断言后才运行。反射诊断是首个active且frame≥240的一次采样；主PNG是elapsed5000/10000ms，不宣称同帧main／logic／World time或完整严格画面对照。岸格变更只证明保存与实际几何范围，不能宣布蓝水带／局部三角全部消失。

本批三张精选原字节图保存在 `.local-evidence/reference-visual-20261006/delivery-r13-engineering/index.json`，SHA `9baa51940765f1fe6dc7716f66b6e38dd4a2698afcaf694a8bc42c5c7daf7a3e`：RD3旧／新街道与明确FAIL的RD1新水岸，均含actual配置／来源和哈希，既有精选不覆盖。

本次恢复有真实工程、存档重开与画面新证据，Goal active；普通input0／12 NOT_RUN。用户“先让另一任务继续”仍适用，未竞争桌面或向另一聊天发送协调消息。A–E、连续正常画质、普通编辑后菜单重开与当前正常包退出条件仍未整体成立，不complete。以下保留此前v11／r12及更早事实，不用历史段落覆盖最新范围。

## v11版本和保护（历史检查点）

当前普通候选为 `build/reference-visual-goal/HelloMine3D Reference Complete v11.app`，Release arm64，bundle `local.hellomine3d.reference-current-v11`，世界“临水建筑完整样板”／seed42／`world-9e31248101433662e3db1c8cbfe8d638`，spawn／player `208.5 68 -169.5`。完整226文件、托管167、可写保存56分别核对；加入v11后十三包2875文件逐成员／SHA保持，未知操作者改变的WorkbenchCurrent原样保护。

| 当前v11／r12身份 | SHA-256 |
| --- | --- |
| Release r12游戏 | `b6d2d61371ea6a739a4cba6b532132f4b1abd92b5ec5d719ed57506bba416768` |
| Debug r12游戏 | `954a004b94d47318fe9c4f15dfc71f6a4b462a7c430fb5126216b34c9e2781e0` |
| 当前2990项src清单 | `f127efc8b34e7c43ee6056f3e923ab14b3a7992f3090758d3283c81e257fbe60` |
| 正式资源manifest（未改） | `f859433d9801ca4e41f34fc3c8ed91715d16f00ffb98e970f3b95870b956b91c` |
| v11托管分发清单 | `84d5880033c07e7bb95b6fb3828ca8efa36a550dc8beedbe8df11bd6b6d82bc8` |
| v11全226文件清单 | `99a8a369c034a15102d1a14d423d05855f8a09c612d1a5e821f35905f4bcb3a7` |

创建来源保持真实 `5ce2209e731f370e36b5c9ae32793c2249390f84`＋dirty `914eba9b02f64a9005f2be5e028381aa10c3f4449fe467c8c0dff20145b3cac8`；后续提交不回写包、capture或native身份。`final-workbench-v11-r1/postcheck.json`27／0，SHA `201f17e35cf024caab24bd123091f8077cc5186dee1122b0e4d347637c33cd94`，source168／current2990／resource143／fresh save56全部实核。

当前真实四阶段Release99806／Debug99924自然0，新严格reader各212／0、Debug37故障副本全部命中特定门槛；实际保留绑定负控179自然1／FAILED／oracle整体FAIL。有限legacy三新进程260／270／278自然0、独立52／0。新绑定和读者失败保留细节见本报告末节及[完整sampler合同](../contracts/reference-water-fallback-sampler-contract-v1.md)。正式Water shader未改，宽透明岸带、局部斜边、普通连续观察与VS16／FS8严格压力FAIL仍开放。宿主active／恢复第2真实turn、ordinary12 NOT_RUN，不complete。

v11鲜样板来自 `scene-v2-native-r16/street-msaa4`，Root owned1455自然0、CAPTURED、实际构场62967 edits／两布局／12部件／2树／正常World保存；属于作者构场input0。菜单配置只与实际fresh shadowoff差一个明确的medium值。离源码完整临时副本 `/private/tmp/hellomine3d-reference-goal-menu-v11-334p48sr/HelloMine3D Reference Complete v11.app`，actual2767自然0、七env仅ROOT／hidden／有界capture，没有World／Save／Catalogue覆盖或点击Continue。实际Cocoa1280×720点／2560×1440 backing；两原图均已查看，正常菜单显示样板名，但未进入World。source226／临时save56／十三包2875／src2990全部保持。

准备器创建时96efd，创建后96c08只新增四处v11保护；reverse精确回原字节。真实CLI四负例2682／2683／2685／2689均自然exit2、4／0（`helper-protection-r1/audit.json` SHA `ab0db44426c6f44546a7968bab6cd16bac7371bc511f42ef3add9ee0fd8a68b1`），无native／包创建。当前三张精选原字节图位于 `.local-evidence/reference-visual-20261006/delivery-v11/index.json`，SHA `5990bc44d75b49f70c5771eb9c456628a6efea9d375e45c716879df2d808ef1b`：r12 fresh街道（shadowoff）、v11默认菜单、r12 above-return（工程teleport）。原六参考缺失与后加五张候选均保持，未宣称美术收益、普通输入或人类审美通过。

v11独立创建审核 `final-workbench-v11-r1/creation-independent-review-r2.json`29／0，SHA `9bab4870df662eb12b4137237d102438e87866740fb38f0a6449bfd2eb88cc4a`；原r1错误把142个media payload和manifest自身的143正式总数混淆，29项／1FAIL收据完整保留（SHA `42650942bbe0bb373110844b3c559ef9fe98ea5b4678cc51b9f5eaf34d68d6d0`）。r2只修成员计数域，不改资源、native或其它门槛。菜单独立 `menu-isolation/domain-audit-r1.json`23／0，SHA `8f3c6d3bf99b98f03fd15f36012472727d88880fd6a192d9ac300df653c59bc8`，两原图完整CRC／RGB(A)逐行解码且像素相同、实际查看，七env／Cocoa／lsof txt／source226／temp save56／13包2875／src2990精确。cwd为实际launch参数，未宣称lsof cwd查询；无World绘制/诊断构建记录、没有输入或Continue。

### 当前普通输入与提交状态（公开能力恢复更正）

**后续本Goal实际选择补录**：公开 `getApp` 对 owned ordinary-v11 副本返回 `ReferenceGoalOrdinary`／`HelloMine3D` AX handle，实际耗时992.1562s，虽然请求timeout30000ms，不能称有界返回。随后[实际映像／cwd收据](../../build/reference-visual-goal/ordinary-v11-r1/app-selection-identity-r2/receipt.json)（SHA `64903388a321aacdf30466838e7a21111b9eefab47ceec4acab485b778b95c9f`）核本Goal PID6107在private副本运行、binary b6d2…4768、cwd同bin；另main v10s PID5551原样保留。普通日志当前 INPUT_FOCUS=0／frame0，无Continue／普通动作证据。本Goal应用选择1、native启动1、input0，十二路线仍NOT_RUN；上文及恢复点的App0为此前快照。两个只读进程reader失败保留：默认sandbox ps被拒、第一份lsof reader误对所有库路径stat，撞到analyticsd权限；新r2解析实际fcwd而非猜目录，未发信号／输入。

选择等待期间另一任务开始新的复焦会话。用户随后明确回复“先让另一任务继续”；本Goal暂不操作桌面输入，未向其它聊天发协调消息，继续独立工程。这是输入时段优先级，非整个Goal暂停或普通验收豁免；宿主active、退出4仍 **NOT_RUN_WAITING_FOR_SINGLE_INPUT_OWNER**。owned6107保持未聚焦菜单，尚未执行键盘／点击／拖动，后续只有另一输入会话收尾且核当前窗口归属后才继续。

当前普通输入状态为 **NOT_RUN_WAITING_FOR_SINGLE_INPUT_OWNER**。新的[公开CUA恢复收据](../../build/reference-visual-goal/resumed-audit-r2/cua-public-recovery-evidence-r1/receipt.json)（SHA `a15c17c81d03b4e12d7a9d6c2c4151d0328f208798bba73f2568213b20e38e60`）记录另一主工作区视觉精修任务对 v10s `Workbench.app` 的公开 `getApp` 两次 completed（2893ms／535ms）及后续截图调用；该任务报告点击可用，但较新的等待快照报告复焦后 Escape 无响应，键盘效果仍未闭合。工具调用完成不等于每个按键生效，更不等于本Goal普通路线通过。本Goal CUA应用选择／输入仍0、12路线全部NOT_RUN；目前等待另一普通输入操作方收尾，不竞争输入或操作其窗口。

[普通v11副本准备](../../build/reference-visual-goal/ordinary-v11-r1/copy-receipt.json)（SHA `80ae663a74f49f0e06eb453b6dfc3fe574349a78204c985deffa5378588e3054`）已将226文件／save56及文件模式精确复制到 `/private/tmp/hellomine3d-reference-goal-ordinary-v11-umpqlew2/ReferenceGoalOrdinary.app`，原v11与十三包2875保持，未launch／输入／注入／改档。[普通路线](../../build/reference-visual-goal/ordinary-v11-r1/normal-route.json)（SHA `ee85d8b8542bd13b1bb96bff72d98a362c54db3aa0cd9df08bfe9e0157b91315`）仅PLANNED_NOT_RUN，覆盖正常菜单、行走、12类获取／放置、地图、灯墙岸编辑及保存重开；源包bundle ID和创建身份保留，未来唯一执行者必须按实际clone路径／执行映像核身份，不能仅靠重复bundle ID选择。

本轮已本地提交 `e794f7b584e6bdcf757783c691a6c4d0f11cc1e6`（10文件），v11创建身份仍为5ce＋dirty914eba。[提交后r1](../../build/reference-visual-goal/resumed-audit-r2/water-fallback-local-commit-r1/commit-postcheck.json)和[r2](../../build/reference-visual-goal/resumed-audit-r2/water-fallback-local-commit-r1/commit-postcheck-r2.json)各前23门槛通过、最后全局空闲门槛FAIL：r1子串误匹配自有检查shell3500；r2真实发现另一主工作区v10s游戏PID1847（lsof实际路径及SHA另存），两份24／1原FAIL不改。[新r3](../../build/reference-visual-goal/resumed-audit-r2/water-fallback-local-commit-r1/commit-postcheck-r3.json)11／0（SHA `ad276d3ee460377b7d7700866c57a1cafc573d2e7b5cfc38593072100323b005`）只要求本Goal自有进程已关闭，外部游戏原样保留。v11菜单原 `IDLE_GAME_PREFLIGHT` 标签因args子串未发现 `./HelloMine3D` PID1847而过宽；原capture／FAIL保留，有限菜单图像／源码／保存证据不因此作废，但不作为全局空闲或性能隔离证据。

## v10历史交付身份

以下v10／r9数值为修复前检查点，保留原件与失败，不作为当前v11绑定版本证据。

当前普通候选为 `build/reference-visual-goal/HelloMine3D Reference Complete v10.app`，Release arm64，bundle `local.hellomine3d.reference-current-v10`，世界“临水建筑完整样板”／seed42／`world-9e31248faeb63c1f9a59f368ece99651`。正常配置v12，spawn／player `208.5 68 -169.5`；两栋不同布局建筑来自真实作者构场保存，普通输入建造仍未验。完整226文件分别记托管167和可写保存56；v10加入保护后，十二包共2649文件逐成员／SHA保持，不覆盖未知操作者改变的 `WorkbenchCurrent`。

| 当前v10／r9身份 | SHA-256 |
| --- | --- |
| Release r9游戏 | `1f23ca5ed547ec2c0221b3dcdc4132dc0530cf0f71413e3a238e5deac1d3309b` |
| Debug r9游戏 | `739495700441960dbf5be196fee71025265e80dede6ceb712ca2eec816c51be2` |
| 2989项src清单（含生成inc） | `60d760b27ca79485228b826f4e820bdc978b9333c67a6478632c2a2541cea0c9` |
| 正式资源manifest（未改） | `f859433d9801ca4e41f34fc3c8ed91715d16f00ffb98e970f3b95870b956b91c` |
| v10托管分发清单 | `fb6e524b7fdceb9ace1193a3c5a666277aeeebebeee34104b979c4183dd690f6` |
| v10全226文件清单 | `edb06b03ee6fe02cfb75282768a781cf1a2e02d23bc8250062b39815239beb8b` |

受保护v9创建身份保留实际来源 `2ccd0ccd2e4ea886b8a8633e825f261397e86461`＋tracked dirty `5a2c984086a577ae57ff1a36576a6ce424a6759ccee4711c2591ac868aa9c894`，后续提交不回写创建身份。`final-workbench-v9-r1/postcheck.json` 实际27／0；创建后再加入v9保护，`helper-protection-r1/audit.json` 真实CLI4／0、0游戏启动，十一包2423原样。工程包source168、正常用户包226、模板save56各自记账。

v10创建身份继承真实r9来源 `b170d3aa555acfc5446da3471f63b28322f3db7e`＋dirty `81763f0243a980abe005cf31e014d048c3e70c2ebc4fc4095510eded351e68d6`，当前实现已本地提交 `2c73ec2dc721de0f2127127678f1607bc28a1a3e`，包身份不回写。`final-workbench-v10-r1/postcheck.json` 27／0（SHA `080e5d958c977c66c8dc9e22d1f7e13986a2b4bb522191604459eeff7ad0e4c5`），当前代码/资源及完整包/正常fresh保存核对；准备器在创建后加入v10路径/ID保护。 独立创建24／0、菜单27／0和创建后保护CLI4／4均已完成，十二包2649文件原样；准确收据与首轮reader失败保留见下文。

当前r9实际双客户端构建0、双水位Release／Debug各292／0及真实旧面负控已发生；r8地图1793／0、HDR链接四案例164／0和原跨水面176×2按准确域保留，当前r9原协议另有176／0。`reference-water-plane-build-r9/source-scoped-reuse-r1.json`（SHA `a3552ad262598f9501d6be2e65884737b909c9a560a2bcfc6ace60552bba48cb`）核r8→r9仅三runtime文件改变、World+Tests215／资源143／C GPU输入24逐SHA同；旧4491/26等只按未变域复用，不把驻留330、三PID设置或旧HDR／Bootstrap全流程称为r9重跑。正常材质／WaterVS／WaterFS资源未改，严格VS16／FS8压力FAIL仍保留。新opaque分类depth对照36／0支持局部depth/order贡献，原Alpha单pass depth对照仍有三角与细线；宽透明岸带和画质归因继续OPEN。宿主active、本次恢复第2个真实turn，不blocked、不complete；12普通路线全部NOT_RUN/input0。

## v8历史交付身份

以下为本轮修复前的v8创建身份和当时检查点，保留其原数值、失败和未执行项，不作为当前v9身份。

当前普通候选为 `build/reference-visual-goal/HelloMine3D Reference Complete v8.app`，Release arm64，bundle `local.hellomine3d.reference-current-v8`，世界“临水建筑完整样板”／seed42／`world-9e312494bc82bd30e6e31915b728d461`。正常配置v12，spawn／player `208.5 68 -169.5`；真实作者构场保存，包含两栋不同布局建筑，不是普通输入建造证据。托管167文件、可写保存56文件、完整226文件分别记账。九个旧包1971文件对本次恢复起点逐SHA保护，新增v8使保护对象变为十包2197文件；不覆盖未知操作者已改变的 `WorkbenchCurrent`。创建后才在准备器加入v8保护，真实CLI4个拒绝检查通过、0游戏启动。

| 当前身份 | SHA-256 |
| --- | --- |
| Release r7游戏 | `664dc919bb1d6faa8f449680d8f61823f530e9b62580917852d2b6e235860e06` |
| Debug r7游戏 | `aa67e7a4abea352a78970944db840faad9fa4275a314ff65b8d66f8c44799779` |
| 2988项src清单 | `4bf8f24faa1b7d22aca4545fe6e474b159eab72ecb9ae85fda05e5b12f4f41dd` |
| 正式资源manifest（未改） | `f859433d9801ca4e41f34fc3c8ed91715d16f00ffb98e970f3b95870b956b91c` |
| v8受管理清单 | `b2e0cbdd2d96086ed3280c450938c76dd4ae50e6a28c151c72fd861ae405d89e` |
| v8全226文件清单 | `61077af6b0e797fe175c4992776a40f6d36629506b0a2224a3fa78393f351a49` |

v8创建身份保留实际来源 `c304e5b720e13c5a8a15d42010432680a7dd53cc`＋tracked dirty `8ecfc8755824ad90c6f5a57a93b12e284493b627ee8fec80ef98caa44e1307dc`；本轮后续本地提交不回写该来源身份。当前双客户端、设置重启、probe-off街景及无PNG性能实际执行；World4491与真实驻留返回按精确不变源码域复用，不能称r7重跑所有历史阶段。`final-workbench-v8-r1/postcheck.json`27/0核全部src／资源／包／存档／backups和创建身份。

2026-10-07恢复第2实际turn又发现可独立处理的范围缺口，未宣称工程耗尽。v8仍受保护且是修复前版本：套件ID33–44缺地图配色，真实陶瓦／木梁落入默认灰色；当前仅UI新增13行家族映射，生产函数1793/0和旧源码12负控拒绝，双客户端与新包待统一构建。HDR跨阶段链接负控在冻结r7实际PID76207自然exit0、强制fallback生效，坏resolve接口被隐藏为真实生产缺陷；原FAIL完整保留，正式链接检查正实施。默认关闭的同Root跨水面component证据仍在补齐，普通输入保持12 NOT_RUN。

以下v7段落、数字与原收据均属历史版本；本轮r5–r7差异、实际失败及有限复用另列。

## 历次实现和v7保护证据

实现工作树 `/Users/lizi/.codex/worktrees/reference-visual-v1/HelloMine3D`，分支 `codex/reference-visual-v1`。主工作区 master 和另一视觉 Goal 均未写入。首版提交 `552345675265f77f8a30dbf1ae205a52d1abb64f`；主体实现提交 `2f87fc67`，包含正常世界建筑、资源／制作、表面光照、真实倒影和普通 HDR 首选四倍多采样；水岸几何修复提交 `a7f42d0262bbd7a7d2a4a3361ef263ffb4bad64a`；水平水面反射修复及包保护提交 `ef6d4350c96dadcaede332089beda8c4e3f7d5e3`；正常资源释放、严格诊断入口和 v5 包保护提交 `32cf60a0b23d0e9e165d39b150d584ec46d2b693`；本轮真实世界编辑到原GPU受光／倒影工程证据提交 `671474ad13ff9e2533c9e90e3a38d1d8c3c0ed87`，已交付包／大小写别名保护和普通启动隔离提交 `a08a9d20`。仅本地提交，无推送、发布或标签。

保护首版 `build/reference-visual-v1/HelloMine3D Reference Visual.app`（182文件）、`GoalWorkbench.app`（182文件）、`WorkbenchCurrent.app` 的实际现状（251文件，未知操作者已改变存档，未覆盖），以及 v2／v3／v4／v5／v6 完整包（各226文件）。不刷新或删用户存档。新当前候选包为 `build/reference-visual-goal/HelloMine3D Reference Complete v7.app`，Release arm64，bundle `local.hellomine3d.reference-current-v7`，世界“临水建筑完整样板”，seed42。九包共1971文件均逐SHA保护。包可离开源码运行；普通启动器清除 HELLOMINE3D／HELLO_RENDER／HELLO_PERF／HELLO_VISUAL 四组诊断变量，只设置包自身 ROOT；原始诊断包的 packager wrapper 保持兼容显式捕获，不将普通启动隔离错误扩到诊断工具。托管167文件和初始可写56存档文件分别记账。

| 身份 | SHA-256 |
| --- | --- |
| 当前 Release 游戏 | `3c426d5b49d6eeafc82f04b053a8c1408e2e40ee39c5ffce859dbdeb820459b2` |
| src 源文件清单（含生成 inc） | `37c29587d03af3ffc1a223164c3432da75246efedf84979555f2b228fbe51131` |
| 正式资源 manifest | `f859433d9801ca4e41f34fc3c8ed91715d16f00ffb98e970f3b95870b956b91c` |
| 沿用 r7/v4 冻结 Water.frag | `bda2529e3d2ab22a0280e23356c2e04af752ceaf615cfdf9f40f8b729890b077` |
| 当前冻结 Water.vert | `7d95d3daa8f043b24df6181cd3903c8d1699a620711a7bd0cbe0e4caed0a4df2` |
| v7 受管理包清单 | `fa4f776dd6224b054d6d5f17aa8cca6800e923404af8cffa3de8174c50433851` |
| v7 全226文件清单 | `bcb71db3881beb7ca26648a16506a82972f323c20b075022ce1476dadd639267` |
| 当前 Colour 数组 | `5232b1c87d25a8bef1b26ace87e07bbe7eea9520ed17601ec6446bf4eef36a95` |
| Normal 数组 | `853af45c552f7a603f9452c1ce8ceae6ca12eca4b72cd948668f110184c3ed32` |
| Surface 数组 | `7e8df2db6028b9fe27329df3b189537b66f7f8364e9a8d0f666ddadc11f6463d` |

历史 v3／v4／v5／v6 创建身份与收据原样保留。v6 的 Release `89567b…`、2984项 src `95662cc…`、创建 `c14f327b…`＋dirty `815373ce…`，属于上一轮，不改写为当前身份。其离源码默认菜单 PID7997自然exit0及恢复点仍留在 `final-workbench-v6-r1`／`delivery-v6`。v7 包创建 identity **仍是来源包的** `671aea60dbc45f82f3737e829c45148829d281da`＋tracked dirty `66397528760bee0f628ddeca1e6de767050c3c8d16abf0626f499dc4523e3511`；后来提交只记录实现落地，不回写来源收据。当前2985项src逐文件／成员身份一致，资源未改。v7世界 `world-9e312492d48a8c679e4c1ee2fd4e8be2`，spawn／player `208.5 68 -169.5`。准备器与独立交付核对在 `final-workbench-v7-r1`：27／0，源168文件／r10捕获全树／模板56文件及旧八包1745文件不变；创建后才补v7保护，新增真实CLI4／0，九包1971文件不变，不让固定保护挡住首次创建。

v7完整全树副本在 `/private/tmp/hellomine3d-reference-goal-menu-v7-4c68wp_5/HelloMine3D Reference Complete v7.app`，离源码实际PID21873自然exit0、7.021秒。loaded executable由实际lsof核对；只设置自身ROOT、hidden及有界capture，没有SAVE／CATALOGUE／world override。两张原生2560×1440默认菜单图同原字节SHA `202dd010…`，根代理实际查看5000ms原件，显示“继续游戏／临水建筑完整样板”。源226文件及临时存档56文件均未变；未点击继续、未进入世界、输入0。原 `CAPTURED_PENDING_VISUAL_REVIEW` 收据保留，另有 `menu-isolation/root-image-review.json` 限定审查。构场和编辑probe日志均0；不能替代退出条件4。创建脚本的重复文件guard曾拒绝覆盖已准备脚本，随后唯一native运行使用未改的准备脚本，不记第二次客户端。

样板是作者构场和 starter 赠送库存，**不是普通输入建造证据**；仅准备时 live 名称和安全 spawn 改为街道保存位置，其余原始存档／备份保持。恢复旧备份会恢复其原名称和森林 spawn，交接中已明示。

## A–E 实现与当前验收

| 范围 | 正常运行路径与当前证据 | 尚需验收 |
| --- | --- | --- |
| A HDR | 固定曝光／真实RGBA16F／独立owned depth；r8真实resolve stage-pair先于回退/FBO链接校验，四native和临时GL对象清理通过；历史resize／A→B→A和组件释放按未改源码域限定 | 普通窗口resize、目录UI和设置操作、普通行走及长期释放未验；r6驻留和r7新进程切档不能扩称当前全部生命周期 |
| B 套件 | 12类单格部件，最多8盒、四向；BlockId33–44、Material49–60，11制作+1冶炼；真实mesh/编辑/上传/保存路径；r8补全12ID地图材质族配色，1793／0及旧源码负控 | 普通制作、四向摆放、登阶、窗洞选取、挖掘拾取及地图使用 |
| C 材质与光照 | 四种正式 authored albedo，3×64²×256×7 mip；线性色彩 mip、法线归一化、roughness/metalness/emission；普通/阴影完整 shader；真实最多8灯源有界索引；内外阻光修复 | 普通增删灯／墙后受光、正常连续低太阳／近远；本轮隐藏真实编辑及8帧近远证据另列 |
| D 倒影 | 单选水位、半物理尺寸RGBA16F镜像RTT／真实裁切和驻留世界；当前r12完整sampler四阶段各212／0、Debug37故障校准、真实retain-binding负控与有限legacy52／0；历史r9双水位292×2／46故障和旧面負控按原域保留 | 普通转头、入水出水、拆墙／增灯／改岸、行走卸载返回和长期稳定性仍未验；其它水层像素fallback、岸边三角/透明带与普通水路线仍OPEN |
| E 整合 | 48×48街道／6m水道，两层阳台屋和不同布局L形工坊；2株现行 TreeGenerator 普通种植树 tag0；入口通路清楚；HDR正常首选实际 MSAA4，单HDR空间滤波回退 | 三种AA稀疏连续原图已审查；共面细纹已修并复审8帧；侧面硬切已局部修复；近似水侧着色局限保留，普通完整路线未验 |

新 ID 和合法四向 metadata 追加，世界／设置仍v12，无新增持久字段。单格邻面、封层、AO、Alpha和格级阻光分别判断。套件配方保持成本和物品守恒，12部件在真实World而非Ogre直接摆设；工位、炉子、箱子走正常交互。

正式源在 `media/sources/architectural-kit-v1.json`、`reference-visual-scene-v2.json` 和 `docs/art-sources/reference-visual-v1/`；确定导出到 .block/.shape、SceneData.inc、UI atlas与三个数组，并纳入 manifest。HDR的旧木板layer21和道路layer23分别复用 authored timber147／rough145；旧 Warm array、共享图标和 legacy 配色保持。新资源完整同 owner，完整旧渠道可回旧模式；声明部分、混 owner、坏内容拒绝，高优先旧包不被基础新色吞掉。

实际 World 构场62967／72000写尝试、19200变化、613作者区域、16场地区块；两株普通树127+32最终格，正常编辑、树皮采集、保存重开已专项验证，不伪造 natural-root。第二栋作者／整合及返工成本保存在场景源报告，构场约108ms仅是作者诊断成本。围护修复补全墙体／承接层／顶板、正确瓦阶朝向，492围护格与27瓦接缝核查；三处真实室内天空光0，室外15，本地灯光保留。旧漏空画面原件保留。

## 工程、GPU和资源证据

证据根为 `build/reference-visual-implementation/`，各记录保存准确源码、资源、二进制与命令。下面数字只描述对应范围，不作为未来固定门槛。r6 新增真实 mesh 三角裁剪、冻结启动能力、SectionMeshInput policy 与 WaterVS接触固定；当时重新完成客户端和World双配置。本轮新增默认关闭世界编辑观察入口及Planar纯数值观察，上一轮正常生命周期修复不变，当时双构建与真实编辑另列；旧v7轮World／材质／shader／正式资源未改；本恢复轮World增加只读身份和有界Clean mesh replay，已实跑4491双配置。材质／shader／正式资源仍未改，沿用精确对应范围证据。HDR resolve shader、C材质数组、围护／构场、配方和ResourceContract未变，复用对应同源范围；r8正式改变HdrPipeline链接前置校验并增加默认关闭跨水面观察，不能称当前Bootstrap/HDR整个文件未改。814检查限定旧同一resolve shader/颜色域，不扩称新链接入口或水面全部验收。

| 当前适用检查 | 结果与定位 |
| --- | --- |
| 当前Debug／Release客户端 | r12两配置实际自然exit0；`reference-water-sampler-query-build-r12`，Debug `954a004b…`／Release `b6d2d613…`；完整2990src精确核对，两个独立168文件包。r11/query失败、原Debug reader16／1和旧构建/第三方警告均保留 |
| 完整World双配置（源码域限定） | r5真实各4491／0、RETAINED_MESH聚焦各26／0；`reference-retained-replay-world-validation-r5`。当前r12 scoped-reuse核World+Tests215同SHA，限定复用、不称r12实跑full；旧4465和超时原件保持 |
| 严格 World summary | 4491真实双full通过、26项实际/正负校准通过；旧4465明确拒绝。只更新来自26新测试的期望计数和注释，旧工具与失败原件保留，解析/阈值不放宽 |
| 纯 shape 双配置／生产 recipe 双配置 | 各2495／0；各184／0（含正常成本／发现／守恒） |
| ResourcePack双配置／生成工程 | 各201／0限定unchanged合同；当前r12实际generate/graph及9正/负校准通过，旧图结构结果按原域保留，未手改工程 |
| HDR完整生产GPU | 814／0；`hdr-gpu-r2`对应 resolve 及颜色域覆盖 |
| 当前 C普通／阴影完整GPU | 3502／0、GL0；`reference-surface-gpu/r5`含当前道路和木色、三数组21mips、独立BRDF／法线／四向／退化／远原点与坏输入 |
| 当前 C parser／包优先级 | 37／0；`integration-r1/surface-profile-r6`；确定性 export/check PASS，68保护输入哈希不变 |
| D shader／policy | 45／0、19／0；原生真实 RTT RGBA16F、HDR>1、非有限0；当前原图确见建筑镜像 |
| 场景真实 World／导出 | 9／0＋492围护格；`scene-enclosure-r1`，6真实缺陷负例、10场景功能字段保持 |
| 当前普通包交付／隔离 | v11完整226/托管167/save56，交付27／0、独立创建29／0、菜单23／0、保护CLI4／4；十三包2875原样，creation5ce＋dirty914eba保持。历史包／helper／launcher原件保留，12普通路线仍NOT_RUN |
| 适用沿用原生工程生命周期 | Debug／Release各895／0，Release oracle校准23／23；`render-lifecycle-native-r4`；另有真实 delayed-drain负控自然exit1和严格拒绝 |
| 适用沿用生命周期入口 | 十个真实负例自然exit1＋probe-off CPU validation正控exit0；`render-lifecycle-startup-guards-r2`，独立153／0；非普通输入 |
| 启动坏输入／回退 | 6案例PASS；`native-startup-r3`：能力回退、坏PBR、缺variant、缺真实AA auto、legacy坏normal、旧AA-less HDR兼容 |
| 当前HDR链接前置校验 | r8三合法native0＋真实坏pair自然1、独立164／0；r12对应链接验证函数／资源域保持，限定复用。`hdr-link-current-r8-calibration-r1/current-native-audit-r1.json`，r7坏link被fallback隐藏FAIL保留 |
| 当前跨水面component | r12完整sampler四阶段两配置各212／0、Debug37指定故障副本拒绝、真实retain-binding179自然1/FAILED；有限legacy三进程52／0。历史r9双水位292×2/46副本和旧面负控、r8/9原176按原域保留，不称r12双水位fresh重跑；普通输入0 |
| 跨水面历史启动拒绝 | r9新scenario三actual均自然exit1，独立100／0严格核preRoot拒绝；r8三actual阴性72／0保留原域，旧reader漏前缀FAIL保持 |
| 当前套件地图配色 | 生产共用函数1793／0，旧2ccd函数12个新族负控全部拒绝；当前r12/v11保留该精确函数，未代替普通地图验收 |
| AA完整生产GPU | 空间滤波97case、719工程checks／0功能失败但12质量失败；实际MSAA97case、605工程checks／0工程失败但1质量失败 |

保留失败：初次 World3108／49暴露48方向射线与1邻面遮挡，已修真实实现；3264／36为新GPU来源测试漏既有+1标签，精确修夹具且所有断言保留。初始surface编译误加header-only .cpp、旧包夹具非法脚本覆盖、并行Xcode数据库锁、初始Ogre数组API和4个CGL弃用警告、Retina参数遗漏／反射preview误计主帧均保留原日志并修正复验。不能以删测试或放宽质量门槛换PASS。

## AA取舍、内存与帧时间

空间滤波97case为39改善、46不变、12退化，薄线最坏2.89倍误差；实际四采样为94改善、2不变、1退化，单个 slope1/width1/phase0.5 的面积参考误差提高3.05%。保留真实有限采样限制，选择MSAA4不宣称零质量回退；无历史、无TAA，UI在场景后绘制。正常HDR无诊断env即优先4sample，legacy0；不支持window／真实storage／resolve时完整回退singleHDR；明确env=0供同条件诊断。

当前2560×1440：MSAA scene颜色117964800B、深度58982400B、单sample resolve29491200B，总206438400B；连窗口back/depth实际格式计339148800B。反射1280×720颜色7372800B+depth3686400B，私有≤96材质／384pass；HDR≤8294400物理像素、最多4sample。RSS不是显存，driver front/present和物理VRAM **NOT_MEASURED**。真实灯源最多27section／110592格／8源，512B索引每section；每帧仍有界扫描，不宣称无扫描。

该 AA 比较对应r4同组冻结Release／资源：无PNG读回，唯一游戏进程串行 warmup5s＋采样30s，固定水岸、RD3、medium shadow、Off post。它不是后续 r7 或当前 v7 版本的性能基线；当前测量及隔离限制另列。见 `aa-performance-r1/comparison.json`：

| 模式 | 帧数 | P95 ms | P99 ms | 峰值RSS B | >33ms帧 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 单sample、AA关闭 | 8417 | 5.509 | 7.026 | 275644416 | 0 |
| 单sample、空间滤波 | 7606 | 6.629 | 7.852 | 312852480 | 0 |
| 实际MSAA4 | 4959 | 9.103 | 10.761 | 273465344 | 0 |

MSAA4比关闭P95约1.65倍、P99约1.53倍，成本明确；未见持续明显卡顿。严格同机三对和≤1.10依用户Goal沿用延期，**不能将本结果记为该门槛PASS**。一次30s采样不能代替A资源生命周期循环。

## 图像、普通输入和恢复点

固定实景身份在 `scene-v2-native-r4`；旧／新资产×legacy／HDR四组合在 `asset-comparison-r1/audit.json`（4真实launch／8原图／同保存几何和姿态）。旧套件颜色使用历史材质surrogate，原历史没有该整套kit，不能声称它是历史完整场景。两HDR均实际浮点4sample且无HDR回退；旧profile完整缺席、当前profile完整启用；legacy保持旧水。独立AI实际原图审查确认新颜色统一、道路黑白噪声消失、墙/瓦纹理可辨、HDR真实房屋/桥/云倒影及清楚HUD。最后时间约6135±7，不是冻结逐像素oracle。

几何修复前，三模式连续镜头各8原图在 `aa-moving-native-r1`：实际 render camera 12s移动/转头，无Player输入／流送改变。全部24原图已审查，倒影/HUD可读，无所见整体破图／明显漂移，但近岸仍有细线和三角带。r5新增8帧及旧8帧实际复审见 `shore-filter-native-r1/moving-review.json`，仍为 `RESIDUAL_SHORE_BANDS_NOT_RESOLVED`。扫镜在 frameStarted 累加 event delta，捕获在 frameEnded 单独 armed 后累加；文件目标时间相同不等于实际相机姿态／动画进度精确配对，不能做逐像素改善宣称。稀疏帧不是全60Hz观察，也不替代普通行走。

精选阶段原图持续保存在 `.local-evidence/reference-visual-20261006/phase-2`、`phase-3`（均有版本/条件/哈希/AI原图审查）。phase-2是取光修复前，phase-3是围护修复后、道路别名修复前；后者道路缺项如实登记。phase-4保留街道r5、水岸移动r5、室内r4及原残余缺陷。phase-5已保存／展示r6街道、室内和移动水岸3张原生2560×1440原图，索引含准确二进制、资源、设置、env与实际AI审查；历史原图不删除。phase-6保存／展示r7白天和低太阳水岸两张当前原图，身份为FSbda2529e。

六张参考原图准确会话turn／原路径已恢复，临时原文件不存在且限定本地检索未找到副本，缺项索引 `reference-originals/index.json`。按提示词可观察特征实施，不编造对原图比较。用户随后补充的五张原图已经恢复为本地新参考并实际逐张查看，保存在 `reference-additions-20261006/index.json`，来源/尺寸/原字节SHA完整；原六图缺项未伪造关闭。新参考支持后续城镇建造套件与材质族扩展讨论，已记候选，未将本Goal12类悄然扩成30–40类。Windows当前平台 **NOT_RUN**；人类审美、乐趣、手感 **NOT_CLAIMED**，均不追加签字门槛。

以下公开CUA失败和文档查询为早前未恢复时的历史记录，原文保留；当前能力证据及单一输入操作方等待状态见本报告顶部更正。

公开CUA旧 `getApp(GoalWorkbench绝对路径)`等待4819.2s无结果，无审批拒绝。当前普通包v1正常launcher启动成功，`cua.listApps()`4.7402s返回 `WorkbenchCurrent` 与 isRunning=true；随后 `getApp(bundle ID)`虽指定20s timeout，实际等待1262.4s仍无 App绑定／AX／截图，主代理中止无响应调用。工具原文“aborted by user”是代理中止等待，**不是人类取消Goal**。两次均无可见拒绝原因，不声称已定位服务根因。

普通执行者无click／pressKey／drag或其他输入，成功输入0。v1日志显示16:21:53创建1280×720点／2560×1440窗口，16:25:36正常关闭；其后Player位置／时间已变化，但操作者 UNKNOWN，不能当AI玩法证据。该包及其变化存档保护，另建v2/v3继续后续恢复，不与用户抢窗口。准确checkpoint在 `build/reference-visual-goal/ordinary-current-r1/checkpoint.json`。

只读 `cua.rewriteDocumentation()`随后3.3811s成功。返回的macOS公开API只支持名称／路径／bundle ID选择且自动初始观察；没有PID、免初始snapshot选择、有效snapshot timeout或AppInfo→App快捷绑定。windowId/listWindows方法限定Linux/Windows；emit=false只抑制结果输出，不能绕过getApp。见 `ordinary-current-r1/public-api-options.json`。文档查询成功不证明应用选择恢复，**无新能力证据不再重复失败**。没有合成输入，没有用诊断改档冒充普通编辑。

## r5滤波覆盖与当前r6水岸整合

实际 narrow sin^12 岸纹在所测真实bridge字段中约98.3%亮峰小于一像素，原世界米数visibility不足。HDR分支增加有界像素相位面积平均，保持legacy表达式与实际8／16位输出，无纹理／history／额外RTT。r5冻结完整生产shader CGL为114 checks／7 FAIL：原45及新增语义覆盖共107项通过；7失败全部是额外RGBA32F逐bit压力比较，1–3 float ULP（2.98e−8..8.94e−8），原断言和阈值保留，**不能将整工具报告写PASS**。实际RGBA8／RGBA16F各7组逐bit一致、7真实varying岸场对96² double面积独立参考通过、10 helper/能量/坏输入校准通过；桥两对角误差降低81.28%／81.91%，7组聚合降低94.57%。这是对应信号覆盖，原生剩余条带未因此判PASS。证据 `planar-water/shore-filter-gpu-r3/freeze-summary.json`；r1 85／5和r2 95／5原失败保留。

r5冻结水岸MSAA原生性能（无PNG，单游戏，5s warmup＋30s）5302帧，P95 7.115ms、P99 8.698ms、max13.894ms、0帧>33ms、峰值RSS281296896B；真实四采样/浮点/反射附件复核，见 `shore-filter-native-r1/performance-audit.json`。与前表采样发生时间不同，不宣称shader速度提升，不扩称严格三对性能或资源生命周期通过。

r6通过6个原生消融定位到 Water竖直面与低台阶共面，而非仅窄sin波纹。只删除vertical面的诊断能去hatch但留下洞，已保留为负例，未进入生产。生产实现四侧64bit/8×8边界遮挡裁剪，用原两三角重心保留depth/shore/drift/light与对角；只有实际opaque接触轮廓pin，顶面保持原mesh。冻结startup bool在任何World/worker前由实际VS/FS链接探测，SectionMeshInput快照冻结，完整旧WaterVS精确保留原字节mesh；未新增存档字段。每侧≤128三角/384顶点/384indices，44B stride，四侧+原顶底≤74128B/cell；实际纯mask8332 checks通过。接岸rawshore>0顶角锁定原0.90高，保持比最高1/8切边0.875多0.025，开放水保留旧波纹。

r6冻结完整WaterVS+FS GPU560 checks准确拆为504语义／0 FAIL和56 TFfloat32严格位压力／16 FAIL（总544 PASS/16 FAIL），位差≤9.54e−7，断言与阈值不删除；不能写整工具PASS。之前高切边倒折负例及失败修复过程保留。最终VS仅注释修正，GPU实测6af0…到最终7d95…的语义无变链见 `planar-water/water-clip-vertex-gpu-r4/source-freeze-final.json`。

原生r6：street、water、old-complete-VS、legacy、low-sun、interior各2图，以及12秒sweep8图均CAPTURED/exit0、2560×1440；主HDR实际4sample/RGBA16F，当前启动 available1/link1/clipped，旧完整VS actual owner/c4c181…且available0/link1/legacy-uncut、HDR不回退。当前真实RTT1280×720 raw独立重算nonfinite0/RGB分量>1共171个。`scene-v2-native-r6/current-native-review.json`复审全部8新＋8旧移动、street/water4图、oldVS与RTT等23原件；coplanar细hatch明显降低，无所见新洞／倒折／灾难漂移，HUD/建筑倒影保留。较宽水平露出带有实际Step66.5至water66.9几何依据；另见侧面光学色界，当前planar高度门槛在66.74硬切，后续r7独立修复，r6原件和局限保留，**未记全部画质PASS**。没有逐像素姿态配对、全60Hz或普通行走宣称。

r6当前有界性能：5s warmup＋30s、无PNG，2482帧、P95 18.065ms/P99 22.768ms/max93.3625ms，6帧>33ms、最长连续2帧，RSS281460736B，驻留terrain7776736B。采样前后另一Goal普通客户端PID69309在主工作区，归属/并行限制记在 `scene-v2-native-r6/performance-msaa4/{concurrency-context,performance-audit}.json`；未关闭或竞争该窗口。此数据**不是隔离基线或新旧速度比较**，93ms突发保留，不能归因于某单一渲染变化。当时隔离采样待运行窗口空档；后续r7补证另列，资源生命周期仍未验。

## 当前r7侧面反射增量

仅Water.frag入口9行，以world-position derivatives叉积识别几何水平面；在非均匀return前求导，退化／非有限安全approximate，整片竖面不再在meanY−.16内部切换RTT。主normal/Fresnel/Alpha/几何/预算/CPP/VS均未改，因此保留4465双配置、C/shape/recipe/资源等对应同源覆盖，重验本次完整WaterGPU和原生当前/旧VS/legacy/低太阳/移动。

完整当前GPU491／8（exit1）准确分为483功能／0 FAIL、8严格压力FAIL；原45期望全部通过，7历史RGBA32F压力保留，另1任意非零fixture数学中心精确位压力保留（.25→RGBA32F.2499999851→RGBA16F.2498779297）。旧常量world-position没有有效derivative，改为真实水平varying，并独立确认原radiance origin实际16F精确；没有放开生产退化分支或删除断言。32四侧/两对角/两winding/透视、actual VS+FS pin侧与.875窄条、最高切边、farorigin、topHDR、legacy8/16F、Alpha、finite及upnormal/sign-only/全禁RTT/旧高度分界故障均正确拒绝，GL0/strictcompile0warning；见 `planar-water/side-surface-gpu-r4/freeze-summary.json`。r1/r2/r3和114／7全部保留，不能写整GPU工具PASS。

r7六次视觉运行（street/water/moving/oldVS/legacy/lowSun）全部CAPTURED/exit0。当前实际link1/cap1/clipped、HDR4浮点；完整旧VScap0/link1/legacy-uncut且HDR无回退，owner清单可核；legacy仍legacy。AI实际查看22原件（8新+8r6移动、4static7000、2RTT），记录 `scene-v2-native-r7/current-native-review.json` SHA ec2904bc…：平行／锯齿岸线明显减少，连续露出侧几何与屋墙/灯/桥/云倒影保持；未见新大洞/倒折/主倒影丢失。16/16区块ID/metadata完全一致；两RTT nonfinite0、RGB>1仍171分量，raw数值随时间不同。宽透明层边界、局部三角明暗与近似侧着色仍可见；只声明目标缺陷的有界改善，不称完整水物理／全60Hz／普通移动PASS。

r7先前无PNG的5s warmup＋30s采样4729帧：P95 8.933ms/P99 10.064ms/max23.1246ms，0帧>33ms，RSS274808832B，terrain7779936B。原已知PID69309在采样前后均已不在；后续完整进程preflight找到另一Goal客户端PID92602，核实其实际18:24:29启动晚于18:19:17–18:19:54采样区间，不能倒推它当时已竞争。**采样期间没有完整全游戏进程清单，隔离基线NOT_VERIFIED**，不宣称新旧速度提升或严格三对通过。补隔离采样因当前另一游戏存在在launch前拒绝，诊断新启动0，不关闭他Goal窗口；准确时刻/归属在 `scene-v2-native-r7/performance-isolated-preflight-r1.json`，实际指标在 `performance-msaa4/performance-audit.json`。

另一Goal客户端退出后，r7追加**仅一次**隔离静止水岸采样：5s预热＋30s测量，无PNG／采样读回，5206帧，P95 **7.772ms**、P99 **9.294ms**、max14.627ms，0帧>33ms；峰值RSS303235072B，驻留terrain7782384B全采样恒定。实际HDR颜色／深度／stencil均4sample、fallback0、boundary cap1，2560×1440。启动前两份完整进程清单无游戏，运行中7份每5秒完整清单仅本Goal副本PID95264，实际loaded executable由lsof核对；结束后无游戏、自然exit0，源r7包168文件全部哈希不变，没有关闭其它任务客户端。证据 `scene-v2-native-r7/performance-isolated-r2/performance-audit.json` SHA `b1027f36c36eebbd24808fdd15623fa62057407cdec0a4596caa96f1360b5d30`。这是有完整周期进程证据的本次有界采样，非连续进程spawn跟踪、非全系统空闲；actors4..6、worldtime6104..6704真实推进。RSS含启动峰值，不能据此证明长期释放；不声明新旧速度改善、严格三对性能或普通输入／生命周期通过。先前竞争、不完整隔离及拒绝记录均原样保留。

## 当前 HDR／倒影生命周期与入口隔离

正常路径修复两处延迟释放：HDR removeCompositor 原来只 markDirty，旧 compiled quad material／TUS 保留旧纹理；现在在外层帧／退出边界使用 public `CompositorChain::_compile`，新目标分配前 drain。太阳阴影原来先 Scene NONE／clearUnused，接收材质稍后才解除旧 TexturePtr；现在先解除本应用接收引用，再由 Ogre 按引用计数清理，不全局清缓存或强删其它 Scene。主 camera lastViewport 也在 HDR remove 正常恢复；没有声称原生复现 UAF。

[生命周期合同](../contracts/reference-render-lifecycle-contract-v1.md)定义默认关闭、隐藏不激活、输入0的工程探针。独立完整包＋两份真实存档 A/B，同一 Root／Scene／Window，实际窗口 resize 960×540→1600×900→1280×720 point，实际像素2560×1440→1920×1080→3200×1800→2560×1440；正常 `clearActiveWorld(true)` 保存、World销毁、loader同步join，再 `buildTerrain(true, directory)` A→B→A。13阶段有45秒／4096帧、单阶段8秒／1024帧、128 journal上限，外部90秒 watchdog。观察值不持 Ogre 资源引用；新分配前查询旧 GL alive，五类 Manager 名称／count／memory精确回 warm-menu基线。clone world_id 相同及 malloc 地址复用合法，不冒称普通目录 UI 选择。

最终构建 `render-lifecycle-build-r5`：Debug `95a7f52bda74f48d6f2b111bd11253bec1e779c7d5dd66125fae4bacf58b881d`，Release／v6 `89567b…`，src清单 `95662cc…`。验证时6运行源码＋合同七文件combined `bc5490653f821f0a6a4af2e2b9dff77c6622d524ec4ebfbb03c91e3033315a99`，之后只同步合同结果段，原freeze不改写。新header生成图31工程／396groups／3218memberships／54refs，9正／负例通过，未手改生成工程；未重跑不受影响的4465 World／shape／recipe／资源／shader，复用范围如上。

`render-lifecycle-native-r4/{debug,release}` 真实PID7076／7089自然exit0，各13阶段、53记录、83帧，末时4957.67／1582.94ms；独立oracle各895／895，Release校准为1真实正例＋22故障副本，23／23。HDR成功generation1..4，Planar1..6；RGBA16F resolve实际单采样，HDR colour/depth/stencil实际4sample，Planar单采样，owned DepthBuffer附着且POOL_NO_DEPTH，GL错误0。两次合计76次旧GL死亡观察、80次旧名称删除观察，**是观察次数，不是去重对象数**。三次clear各五Manager完整集合／count／memory相同：texture12／65,998,008B、material39／351,674B、mesh3／13,492B、program41／217,010B、compositor3／0。合法ShadowTextureNull1保留，七个计数缓存0、dynamicShadowOff=true；world-reset的反射target／depth／private资源0，镜像camera保留1，组件销毁后镜像camera0、仅主camera1且主viewport有效，最后Root退出，不在Root销毁后查询GL。

真实 Release `delayed-drain` 负控PID7096自然exit1：首个release中旧HDR两FBO、colour renderbuffer、resolved texture仍alive，depth已删除、drained=false、GL0，独立oracle保持FAIL。后续正常cleanup已观察当前资源删除，**未再次query首次保留的四个旧ID**，不声明它们二次观察全部死亡。当前原生三案均源app168文件、样板模板56文件不变；其它Goal PID1892同时存在，明确为 BUSY_HOST_ENGINEERING，不声明性能隔离或普通输入，不关闭或操作该窗口。

保留中间失败与修复：build-r2 Debug exit65为缺完整OgreDepthBuffer header，修include后双构建通过。native-r2 Debug自然exit1，clearA留下4MiB ShadowTexture0，到loadB才删除，clearB严格Manager名称拒绝，推动正常清理次序修复。其clearB begin的material50是39基线＋10Planar＋1shadow，不能称preclear泄漏。默认runner另游戏在场拒启launch0记录保留；修复后的native-r3旧构建两正例各895／0及真实负控保留准确旧身份，不改写为当前二进制。

严格入口还修复两个真实漏口：旧r4 Release混入 `V10D_SHADOW_FALLBACK=1` 自然exit0／13阶段COMPLETE，违反预期提前拒绝，证据 `render-lifecycle-mixed-guard-r1` 保持FAIL；旧r4加owned `EFFECTIVE_MANIFEST_OUT` sentinel，在后续ownership拒绝前已经被writer覆盖，`render-lifecycle-preflight-leak-r1` 保持FAIL。现将probe admission放在既有try最前、资源／manifest writer前，显式probe才提前拒绝validate-only／输出；无probe/fault立即返回。按真实消费者修正material/pause变量名，补forced-fallback、bool／文本World fixtures、自动设置、readback及历史capture alias的实际启动条件。

最终 `render-lifecycle-startup-guards-r2` 十个实际负例均自然exit1，0.40075..0.50514秒，日志原因准确；没有Root／窗口／World／catalogue／journal或私有save变化，十个sentinel各自前后哈希相同。probe-off正控自然exit0／0.66645秒，生成145条精确base manifest，实际创建Root与CPU World，但无RenderWindow／正常帧／输入／GPU渲染。其owned CPU存档23个路径写入及唯一Runtime新增MineOgre.log如实记录，source168／模板56未变；不把它说成普通保存重开。独立复核153／153，文件 `independent-review-r1.json` SHA `7d512fe61e98cae433d863905aae8c6effda6a8a38bf029d749bc926f0f7232d`。门槛与原失败不删除。

本轮r8绘制／兼容和忙宿主测量对应旧r4 Release `ca771ea…`／src `dbe82285…`：street／water／legacy Off／On均CAPTURED/exit0，water RTT1280×720 nonfinite0、171个RGB分量>1，最大3.861328／2.935547／1.248047。AI看过水岸及两legacy原图；两legacy各一条既有Apple sampler警告保留，不声称无警告。独立265／0。5s预热＋30s无PNG采样4411帧，P95 8.83492／P99 9.14188／max10.2221ms、0帧>33／50ms、地形7,775,984B恒定、reported峰值RSS331,890,688B；全部周期清单有其它Goal1892，非隔离、非严格三对、非速度收益、非GPU子阶段或长期无泄漏。未启用probe的draw／shader／资源相同，该限定证据仍有效，不把旧binary指标写成v6实测。

上一轮v6/r9 Release `89567b…` PID7197街道原图CAPTURED/exit0，source168不变，AI查看07000ms的2560×1440原件，建筑／道路／桥／灯／HUD清楚，未见新增大面积破图。该轮生命周期修复不宣称新的视觉收益；该轮街道和默认菜单原字节分别存于 `.local-evidence/reference-visual-20261006/delivery-v6/`，索引准确保留版本、条件、SHA及AI实际查看范围。普通资源循环、设置重启、行走卸载返回、灯墙岸编辑与保存重开仍NOT_RUN。

## 历史世界编辑、低太阳与v7交付证据（限定范围复用）

新增[默认关闭工程合同](../contracts/reference-world-edit-render-contract-v1.md)，三个独立自有clone分别拆灯、拆六格墙及一格填岸，真实 `World::setBlock` → relight／MeshDirty → 正常上传 → 原主／镜像生产draw → 原RGBA16F RTT读回，再完整恢复ID/meta及正常World.save。未改World、shader、第三方或存档格式；普通路径不冻结模拟。只有显式owned probe冻结模拟0／动画4／worldtime6000，30秒／2048帧／每阶段8秒／journal64；最多四次RTT，不额外放宽现有capture预算。目标revision／实际VAO与GPU bytes／linked灯参数／实际sameframe／独立裁切ROI／四先验sky controls均单独检查，不能用updateCount或summary代替。

`reference-world-edit-build-r1`真实生成及9校准通过（31／396／3219／54），r3仅C++内容变化沿用此成员图，两配置构建实际exit0，7核心源码before／after完全相同；Release `3c426d5b…`／Debug `83a8916b…`，全src2985收据 `37c29587…`。13个实际启动负控在r2 Debug `ab14db…` 上自然exit1、0.606–0.768秒，root/window/resourcepreflight0；source168/template56/各clone managed167/marker/link/sentinel全部不变。最终r3只改帧版本和readiness观察，早期admission函数字节相同；这属于明确沿用入口覆盖，不把r2二进制叫当前。runner大小写别名真实CLI2／0、0启动；第一次src成员排序误判的preflight exit2保留，修为全部成员集合与逐SHA校验，未放松内容校验。

| r4自有案例 | 原生结果／独立当前oracle | 校准与限定 |
| --- | --- | --- |
| Release wall PID16706 | 自然0，39帧，265／0；目标裁切ROI1154像素变化，A0=A=A′整RTT原字节 | 33／33＝1actual＋32故障；第一次弱保存fault校准r1保留，r2按实际wall目标和最终保存收据构造真缺陷 |
| Release shore PID16892 | 自然0，39帧，283／0；ROI6239像素变化，A0=A=A′ | 33／33；目标Water7→Stone3→原ID/meta |
| Release lamp PID17175 | 自然0，39帧，294／0；移除灯ROI2205px、未改receiver993px变化，A0=A=A′ | 35／35；World blockLight、原GPU uv2.y及主／private linked灯1→0→1均成立 |
| Debug lamp PID17472 | 自然0，39帧，294／0；原RTT字节与Release相同 | 同oracle范围，不无因重复Release全部故障校准 |
| Release skip-reflection PID18709 | 自然1，26帧，nativeFAIL／oracleFAIL | 142实际观察；主World／VBO／灯已更新，RTT仍旧frame13／revision428975、B与A原字节同，因缺实际同frame private draw正确拒绝 |

四正例合1136／0；三个校准101／101＝3真实正例＋98故障拒绝。每正例14events／4真实RTT／3阶段；源168及模板56均不变，忙宿主不称性能隔离，普通输入始终0。保存后直接解析clone chunk v2恢复目标／receiver／Air ID/meta及world_id，**没有fresh World重开或普通保存重开证明**。原后台CPU-ready队列在A可达46／51／111，但所选live=uploaded、known／resident／not offered均成立；保留同次快照背景和所选事实，不假称offer子集是完整mesh state查询。聚合 `reference-world-edit-native-r4/independent-aggregate-r1.json` SHA `3134ca7d…` 精确核实际bin、官方src/resource收据、7core及journal/原bytes；根代理实际查看当前lamp A/B、wall B和shore A/B主图，归因仍依独立RTT/GPU检查，不宣称整PNG相等。

中间失败原样保留：r2末端误将原frame输入revision与较晚World比较，自然1；实RTT A0=A=A′不因此改PASS。原四角中性控制之一实为距灯<12m的受光Grass，B改变合法；新四sky控制在下一原生case前声明并独立证明ray不进入保留World半空间，未见B后挑区域。r3wall／shorebaseline自然1、readback0，缺末端target快照，不能断言唯由全局队列导致；只移除无关全局队列耗尽门槛，目标当前上传／GPU／像素严格门槛不变。最后r4成功同时保留真实background队列，未隐藏旧FAIL。

当前默认关闭probe街景 `scene-v2-native-r10/street-msaa4`：owned driver19794/game19800自然0，两原生2560×1440原图，源168／模板56不变，根代理实际查看07000ms。v7来源此新正常可保存作者世界；没有把run fixture当普通输入。低太阳首轮 `low-sun-near-far-r1` 的斜线走入丘体，全8原图审查为 `FAIL_NOT_ESTABLISHED`，真实chunk眼点Stone3证实，不改PASS或误判shader崩溃。只读实际存档按负坐标／跨chunk核54Air扫掠、9逐1m站位／实际脚印支撑，另用恒Z街道195.5→187.5→195.5安全路径。

`low-sun-near-far-r2` owned driver21428/game21433自然0，8原图全部AI实际查看，近／远／返回皆可见两楼、屋瓦、桥栏和真实建筑倒影；camera实际X195.5→187.508→195.5、Y68.6、Z−176.2；Player仅正常重力68.02→68后固定。World时间11500→11741，非冻结A/A配对；8稀疏读回不等于全60Hz或正常流送移动。source168/template56均不变。结论 `BOUNDED_OBSERVATION_COMPLETED_WITH_LIMITATIONS`，原图review SHA `6bd2ff82…`：未见新增明显破材／栏断／倒影丢失，宽透明岸带、局部水三角明暗及近似侧光、细周期纹理仍保留。没有新增已确认需生产修复的缺陷，不据稀疏图关闭连续移动验收。

当前源版本还实际完成一次无PNG／probe-off有界采样 `current-performance-msaa4-r1`：owned driver22506/game22511自然exit0，5秒预热＋30秒测量4621帧，独立CSV重算P95 **9.362ms**／P99 **10.4006ms**／max13.3325ms；>33／50ms均0、最长串0。summary固定3小数与CSV6有效位按各自序列化精度相容，不放宽性能门槛。reported峰值child RSS316620800B，实际terrain计数7522752B全4621帧恒定，44B stride／4B indices逐帧重算一致；非物理VRAM、非逐帧RSS。HDR实际colour/depth/stencil4sample、resolve0多采样＝单sample、反射1280×720／plane66.9，0frame PNG／RTT诊断读回（启动1×1格式probe仍存在）。21section／20480格／1source查询有界；周期Planar日志有额外shadow update及batches，非独立GPU耗时。World6097→6697、actors5→7真实推进，启动393.215ms及入世界159.820ms主线程停顿单列，不被稳定采样覆盖。全部10份完整周期进程记录有其它游戏PID21781，明确忙宿主；不称隔离基线／严格3pairs／新旧改善／长期释放／普通路线。source168／模板56原样。独立审计SHA `b5b86eaf…`，所有本轮own handles已自然关闭。

本轮新精选3原图在 `.local-evidence/reference-visual-20261006/phase-7/`，当前street、lamp B、低太阳far原字节及准确条件／身份／独立审查；`delivery-v7`保留当前街道和离源码默认菜单2原件，index SHA `755599a8…`。原phase2–6、delivery-v6、五张用户补充城市参考和原六图缺项均保留。WindowsNOT_RUN／human审美与乐趣NOT_CLAIMED；城镇30–40材料族仍候选，不悄然扩大本Goal12类。

## 退出条件与恢复

| 提示词第6节 | 当前结论 |
| --- | --- |
| 1 全部正常生产方向 | A–E范围保持；r14补洞口背景视距，r13 Water能力接口／80格整岸线与12套件已接真实路径。历史World／编辑／驻留只按准确身份和不变域复用；普通获取／编辑／移动及本版其它水位完整循环仍未验 |
| 2 画质和动态 | r14实际主图露天悬空sky黑块消失，地下边界／RD8岩壁保持；水蓝band、局部三角／斜边仍OPEN；左墙黑区与保存Air洞口一致，精确像素归属未证。原RD1 FAIL保留，工程机位与稀疏原图不关闭普通连续动态或整体画质 |
| 3 工程和稳定性 | r14双构建／双包0、Renderer90×2／0、Cave41／0、HDR r2 851／0；首次HDR850／7和legacy wrapper FAIL保留。r13 sampler228×2与历史World等只按准确域保留；VS16／FS8压力FAIL及普通编辑／碰撞／地图／保存可靠性仍未闭合 |
| 4 普通输入保存重开 | NOT_RUN_WAITING_FOR_SINGLE_INPUT_OWNER；本Goal getApp已实际返回AX但耗时992s、owned6107路径核对、input0／12路线NOT_RUN；用户指定先让另一任务继续，键盘效果未知。ordinary-v11副本／计划不由菜单状态或工程保存关闭此项 |
| 5 交付和身份 | v12／r14普通候选已实际创建，全226／managed167／save56与当前源码／资源已核，创建2a448＋dirty09e288保持；旧包原样，保护基线14包3101。v12保护四拒绝／离源码菜单已核、普通input0，整体五项退出条件未成立 |

宿主保持active：本次恢复审计为第2个真实turn，旧blocked三turn及未知操作者paused不累计；本turn有地图/HDR/跨水面和完整sampler修复、当前候选及必要交接独立进展，不blocked、不complete，也不把工具调用、跨日、上下文压缩当Goal turn。公开App选择已有另一任务的恢复证据，当前普通路线为NOT_RUN_WAITING_FOR_SINGLE_INPUT_OWNER；待其会话收尾后，唯一执行者先核最新普通候选的owned clone实际进程／窗口及用户接管状态，再执行普通12路线及编辑保存重开。键盘效果尚未闭合，不预先宣称PASS，不合成输入或重启service；旧包、FAIL和此前blocked检查点均保留。

**公开恢复更正前的退出4与恢复记录（历史原文保留，当前结论见上文）**：

> | 4 普通输入保存重开 | BLOCKED_AT_PUBLIC_APP_SELECTION；12路线全部NOT_RUN/input0；工程teleport、设置、保存与菜单截图不关闭此项 |
>
> 宿主保持active：本次恢复审计为第2个真实turn，旧blocked三turn及未知操作者paused不累计；本turn有地图/HDR/跨水面和完整sampler修复、当前候选及必要交接独立进展，不blocked、不complete，也不把工具调用、跨日、上下文压缩当Goal turn。普通恢复条件是公开macOS `getApp` 有界初始观察返回可用App状态/handle，或文档化替代入口。恢复后先核对最新v11候选及实际进程/窗口是否已由用户接管，再由唯一执行者完成普通12路线及编辑保存重开。无新能力证据不重复旧超长getApp/getState、不合成输入或重启service；旧包、FAIL和此前blocked检查点均保留。

**历史 blocked 检查点（本次恢复前）**：本轮第三个真实宿主Goal turn，首次实际新能力审计 `blocker-audit-r3/audit.json`：公开文档0.3755秒返回但API未变，当前App/GoalApp绑定undefined（0.0139秒），选应用／launch／输入均0。原service PID1250身份仍同，service存在不证明应用选择恢复；后来的其它／未知归属游戏不操作。首轮审计发现真实独立C/D编辑证据及v6保护缺证，先完成本轮实现、双构建、实际正负控、当前视觉、v7和新保护，没有仅因三turn就blocked。当前性能、独立审查和交接已完成，最终保护18／0再核九包1971文件、2985src和5张精选原字节。剩余必须条件是公开CUA普通输入、连续动态／卸载及编辑保存重开，当前没有可用App绑定或文档化替代入口；不存在可继续关闭这些条件的独立实现／验证操作。宿主已实际返回 `blocked`（2026-10-06T13:25:34Z），未设置预算，未标记complete。新恢复点 `build/reference-visual-goal/reference-complete-v7-recovery-r2.json`，原v6/v4恢复点及失败不覆盖；记录当前普通候选、全部已关闭own进程、旧包保护、准确源／资源／binary／原图／工程证据和12条未执行路线。公开getApp初始观察有界返回可用App handle或文档化新入口后，先核对v7候选进程／窗口及是否已被用户接管，再由唯一执行者从普通菜单完成12路线；按用户恢复操作重新审计阻塞，不合成输入、不重启service、不无新证据重复失败。状态依据实际连续三turn及当前impasse，不以工具次数或上下文压缩当turn。

## 本次恢复审计与尚缺的独立工程证据

宿主恢复后实际 `get_goal` 返回 active。`resumed-audit-r1/audit.json`（SHA `29b3fc7d44ff54f8ea95144cc2583624b2060657a19d15f19ce54feca02ee649`）保留首个新turn与能力审计：公开文档5.2765秒返回，API未变，App绑定均undefined、应用选择／launch／输入均0；service PID1250仍存在，不能由存在推断可用。fresh blocked audit从1开始，旧三turn不累计。

重新完整核对第5节发现上一检查点的“独立工作已耗尽”结论漏了两个仍能推进的工程项目，现明确纠正：①真实 Player 带动常规驻留 demand，目标区块卸载／原GPU释放，再返回从同存档加载／新上传和主及镜像绘制；旧13stage整World切换、冻结edit、只动rendercamera的低太阳图和静态性能都不覆盖。②真实切换图形配置后由新的自有进程／Root载入同一持续存档；旧同Root切World、直接config写入或加载后覆盖玩家状态的截图脚本都不覆盖。上一blocked时间、恢复点、失败与保护收据原样保留为历史，不将遗漏工程项归到CUA阻塞。

恢复初审时，卸载往返按新增 `docs/contracts/reference-residency-render-contract-v1.md` 冻结的3phase／正常模拟／2RTT／实际退役间隙／fromStorage和新incarnation执行，状态实现及native **NOT_RUN**；切档启动验证先冻结独立协议、保留同world／玩家和正常时间，不创建v8或覆盖v7。两个项目只补工程证据，普通12路线全部仍NOT_RUN、输入0，退出条件4未关闭。本次仍继续可独立工作，不因旧blocked状态停止，也不将Goal标为完成。


## 恢复审计第1 turn：实际驻留返回及新身份 Query（r3）

上一恢复点“独立工作耗尽”经本轮范围审计被纠正：13阶段切世界不等于实际 Player 驻留卸载返回，同Root切世界也不等于设置切档后的新进程重启。本轮继续 active，旧 blocked 三turn及包/证据原样保留，不计入此次恢复的阻塞门槛。

最小 `World::observeWorldIdentity()` 在既有 mutex 内复制已加载身份，未改变存档、流送和模拟。责任图旧基线实际98方法、旧表95行及旧hash不一致，portable baseline原件保留真实FAIL；当前补齐3旧Query、Wildlife参数及新Query，99方法/58Query/38Command/3Tick，公共hash `C890E0E5009CB2353AAB1F532A95B85C70F0D5307E45B5A9227251F835746F4F`。新portable工具与原PowerShell使用相同边界/归一化，当前真实正例和7故障副本拒绝；macOS没有pwsh，Windows原入口NOT_RUN。

当前r3两客户端构建实际exit0、src2986前后不变。Debug二进制 `380411c1390b844daa86c835802d09dc156e12d7d786f012c415c86d1d61569d`，Release `862500d5f41993fc900662fdeee059ea4e3ef99f2076718bff866ea6c00385d0`；两新自有工程包各168文件/167managed，源码清单 `f2666e0d1c0675b099c81c7c42a065a227e233b212a5def5d6ef2c86a0b863c0`，旧九用户包未刷新。r1 Debug编译错误、r2完整World外层180秒超时（4008 PASS/0语义FAIL但无完整结果/child PID未保存）原样保留，不算通过。r3真正完整World Debug281.899秒、Release38.119秒自然exit0，各4465 PASS/0 FAIL，原断言和独立summary工具不变；完整src2986前后相同，当前二进制与日志身份见 `build/reference-visual-implementation/reference-residency-world-validation-r3/results.json`。

同World正常模拟的r3三个阶段实际运行：Release PID29889及Debug PID30362自然exit0、各44帧、2次线性RTT读回、输入0；两目标真实卸载后缓存缺席/旧GL退休，原存档fromStorage新incarnation返回、ID/meta和本帧主/反射上传独立核对。修后oracle各274/274；Release校准34/34为1实际正例+33故障副本。首个oracle-r1实际223/224 FAIL保留：release.before在clearSelection后仍有allocated native storage，误用了运行active门槛；新工具仅改为allocated-storage观测域，附件/预算/GL/旧id/name/删除门槛保留。原生skip-target-retirement PID30194自然exit1、真实 old target visual retained；B数据已卸载，但旧node/renderables/state及4个GL buffer仍alive，callback访问0不能掩盖cache残留。15新Debug入口负控全部自然exit1且在资源/Root/Window之前拒绝，原包168及模板56未改；所有这些是工程证据，普通输入路线仍NOT_RUN。

r3取景局限必须保留：Root实际查看 A/returned 两原图，建筑/灯/水保持，但返回右侧白桦只剩叶条且背景尚缺；此时global CPU-ready176/deferred168，不能从两目标恢复宣称全视野恢复。B独立解码 exact r10 模板，树干(225,67,-175)4/meta2及叶(225,72,-177)5/meta2分属14,4,-11及14,4,-12，两chunk和r9逐字节相同。默认关闭探针将另加这两个固定section的post-return真实uploadserial/currentrevision/GpuResident/!offered见证及有界等待；此前r3不是新见证门槛的PASS。精选原件 `.local-evidence/reference-visual-20261006/residency-r3/index.json` SHA `374bbef78079e949a1e9878baed6ffe40311aa459b4b5b005941df3a28284277`，不覆盖早截取原图。

设置重启合同和runner已准备，静态正/负输入51/0；三新PID/Root/同save连续保存实际native仍NOT_RUN，不能用配置token或旧同Root生命周期关闭。新代码/见证、当前原生重启、最终新包和本地提交尚待本轮后续。公开CUA没有新可用App能力，普通12路线/input仍0，退出条件4未满足；不合成输入、不擅自complete。


## 恢复第1 turn：r4等待失败及实际三进程切档

r4增加正确树/叶见证后重新生成工程：31projects/396groups/3221memberships/54refs，9正负例通过；Debug19.276秒/Release29.591秒exit0，完整2987src前后不变。新Debug `de58e9915df767002eccffbd848a297f8f32b4d4371c56cfe51e0bd88a14c859`、Release `a512c14bd0be79013719260f3686428aa99551cfb93ab06622826d47b3258024`，源码清单 `498eb7c07e37160e498a0f6f47589433cf40971c999ec95118a4737330864694`，两工程包各168/167，原用户包未刷新。

必须纠正此前“早取景”的暂定解释：r4实际Release PID34614自然exit1，返回阶段已等待1024帧/约6.16秒，整probe1056帧/8.388秒；globalCpuReady=0，tree14,4,-11 live415/uploaded=null/GpuResident=false，而leaf14,4,-12已在45帧重传serial3。原两目标卸载/新incarnation重载/current上传仍正确，不能掩盖树体缺失。冻结337fed99 oracle及独立审查均保持FAIL，原件在 `reference-residency-native-r2`，独立review SHA `2f1fada4b359372a8cc2d17682359bbb3a35b106710f0775e0861b6b3f4cb1b4`。未扩大帧/时间门槛或归因shader。

源审确认正常连接缺口：GPU退出Near被销毁；数据仍Resident、CPU经ack成为Clean并保留时，再入Near既不是CpuReady，也不会被只处理Dirty的worker重建，无法重新offer。r4尚未记录tree自己的实际meshState/incarnation，不能把该具体树的Clean说成已直接观测。新版将按World当前Near/Resident/Clean/revision/incarnation只读复制现有CPU mesh，经正常上传/确认恢复缺失GPU；普通CpuReady优先、normal+replay同帧最多8，不markDirty、不改blockrevision/存档、不额外保留World CPU镜像。新修复/当前World与原生证据仍待运行，旧失败不覆盖。

设置重启r4首run真实stage1 PID34871自然exit0；另一游戏PID34882于阶段中出现，stage2启动前安全拒绝，launch1/FAIL原件保持，不操作该窗口。NEW r2显式busy-host engineering串行PIDs35102/35118/35143均自然exit0，三个真正新Root、同一save未重拷或reset，仅正常config两key切 standard-HDR→compatibility-legacy→standard-HDR。世界时间按磁盘加载6128→6258→6400，最终6530，玩家/库存继承及四采样ID/meta、同帧main Terrain/Water linked program/实际sampler、HDR4/legacy0/恢复HDR4、真实AAstrength0及legacy inactive/null域经runner核对；独立审查与三7000原图复审另存。legacy RTT/附件/实际采样为0，不声称CPUcamera/组件/缓存全销毁。期间并行宿主不称隔离/性能/普通输入。10个新Debug settings admission负控均自然exit1、pre-resource/Root/window、所有原包/模板/自有links原样，记录 `reference-settings-restart-startup-guards-r1/audit.json`。

Settings runner静态差异r4真实33/4失败保留：有符号negative section错用GL计数非负门槛；只修section整数域，新r5差异33/0、runner `e5341cf7060c60983be3d5b6cc1a65226677c93ce51ebae1266884392c30723f`。不把这些静态数值副本叫native或普通输入。新版正式v8尚未创建；旧v7和全部旧包仍保护，公开CUA普通12路线及退出条件4仍未满足。


## 恢复第1 turn：r5生产返回修复与r6真实往返

真实缺失链路修复接入普通 `syncWorldGeometry`：GPU Near表示已销毁、World数据与CPU网格仍Resident且Clean时，`observeRetainedSectionMeshes` 只对实际Near／Resident／Loaded／当前revision与incarnation进行find-only复制，经正常上传和ack恢复。普通CpuReady优先，normal＋replay总计每帧≤8；请求或预留超过8直接拒绝，不load、不markDirty、不改blockrevision／存档、不额外长期保留CPU镜像。incarnation非零ack防ABA，原两字段0域兼容。合法empty mesh仅留数值fullXYZ/inc/revision，Near退出与World销毁清除，非GPU所有权。

r5真实World Debug PID40476自然0／285.380秒、Release PID41240自然0／38.170秒，各4491 PASS/0 FAIL；两配置真实聚焦各26/0。26项覆盖复制完整生产mesh、不alias、block／savebyte不变、staleinc/rev、非Clean状态、缺失find-only、nonNear／resident-only、实际normal共享8预算、oversize拒绝及empty合法；不是删旧测试或改计数造PASS。全2988源前后逐SHA相同。责任图原baseline98方法与旧95表不一致真实FAIL保留；当前100方法=59Query/38Command/3Tick，公共hash `9FF359DBFE6DB884939272104EDFACAAE782F2A5CE8FFBE23497896B4F2F50CC`。Portable实际正控／7fault拒绝通过；原PowerShell/Windows NOT_RUN。新版summary期望4491来自真实26新测试，实际双full与26校准通过，旧4465工具／拒绝原件保留。

r5原生结果必须保持混合：Release40465自然0，严格树见证327/327、40cal（1actual＋39fault）；Debug40656自然1于51帧，树同inc40/Clean已实际重传，leaf也上传，但地表见证column观察不可用；主图与RTT文件已经写入，不把snapshot计数1误写成物理readback1。真实fault40876自然1却先遇target-data-still-resident，未命中预期GPU缓存负控，不能称负控成功。原件与聚合 `reference-retained-replay-native-r5/independent-aggregate-r1.json` 保持INCOMPLETE_MIXED。

源码发现SurfaceMap是try-lock best-effort batch，锁未取到返回空；r5失败未记录batchsize／mutex，不追认某次具体锁忙。r6只改默认关闭residency观察：A/return先收齐两个column及fixed nonAir严格ID/meta，B只有同批完整known=false才进入缓存门槛；不可用观察在既定帧/时间预算内等待，所有actual事实在draw/readback前冻结。固定样点 blocking existing `World.getBlock`，记录观察域与同帧号，实际mesh/inc/serial门槛不变；生产World/replay未改。

r6 Release PID43395和Debug43744均自然0、各330/330；42cal=1实际正控＋41故障副本全部严格拒绝。两原目标实际退役间隙各6gap／12oldBuffersdead（合12gap／24），fromStorage新inc重载，same-inc40树体Clean通过正常replay返回：Release frame38 ordinary0＋replay8，Debug37 ordinary2＋replay6。真实fault43831自然1／frame32直接命中old-target-visual-retained，B同帧数据unknown但旧node1/renderables2/4bufferalive，oracle保留FAIL。两返回原图实际逐张查看，树干/冠体与建筑桥和倒影存在；正常关闭的cache含empty=0，私有cam/material/HDR/Planar旧native对象退出释放独立核对。聚合 `reference-residency-observation-native-r6/independent-aggregate-r1.json` SHA `d321cf31…82f81eb`，不是普通输入路线。

r7仅SettingsHeader观察块相对r6改变，r5→r7只有Bootstrap＋SettingsHeader；全部World/Runtime/26测试及residency正常运行源码未改。独立 `reference-settings-observation-build-r7/source-scoped-reuse-r1.json` SHA `bcb1214abdec8a3a8a21294929ee8c8a8e439c56c581534b7b5324770431f8d6` 核四条真实World日志、actual PID/自然退出、bin/log SHA、r6正负原件及143运行资源；旧15 residency／10settings admission只复用精确未变函数字节和原case拒绝域，不冒称r7全流程重跑。

## 恢复第1 turn：r7当前设置、性能和v8交付

r6设置单次 PID43985自然exit1，Settings-restart-resident-samples-absent，launch1/snapshot0/PNG0；源168／template56／配置与旧包不变。该错误命中四样点SurfaceMap准入门槛；原件未记录实际size/known/锁所有权，不能证明数据实际缺失或追认具体锁忙。r7只在默认关闭settings Header替换四样点观察：existing blocking、find-only `getBlock`，非Air且完整ID/meta仍严格，known由真实非Air推导；每cell记录固定观察域与 `observed_frame==snapshot.frame` 真正正整数，before-firstupdate身份/玩家/health/库存/time读取不变。Header SHA `727b7859fbd86478bd110153e568252730e14b28d4f1dff7074f797ce14b23c5`。旧r6失败独立19核对原件保留，不重写为成功。

r7 Debug21.386秒／Release32.199秒实际自然0；源2988前后相同，真实r5生成graph成员相同且生成规则未改，因此限定复用31projects/396groups/3221memberships/54refs/9cal。两个新工程包均168文件／167managed，创建c304＋dirty8ecf原样保留。r7实际三个新PID66710／66820／66932均自然0，从同一个隔离save正常连续保存，未重拷/reset：加载time6128→6255→6397，最终6527；stage1/3HDR4、stage2legacy0，sameframe实际Terrain/Water linked program与sampler、固定4cell读取、真实AAstrength0／legacyinactive-null、保存玩家/库存/身份链经独立52/0核对。`reference-settings-restart-native-r5/release-three-processes/audit-r1.json` SHA `71a5e845a9d4b30a6f7504beae137e0b96313c3eba92cc62e053081ac4ac7314`；11原fault＋4新cell观察域/同帧故障共15/15从真实快照校准严格拒绝。三张7000ms原图实际逐张查看，image-review SHA `19cce6d1…` 绑定run `a246d108…`。保护真实集合十包2197，旧字段名all_nine仍保留原件不影响实际集合核对。Settings工程输入0，不代表UI设置操作或普通重开。

当前无PNG性能 owned driver66322／game66327自然0，5秒预热＋30秒采样4743帧；独立CSV P95 9.58292ms／P99 11.3186ms／max18.898ms，>33/50ms及最长串均0，峰child RSS294289408B；terrain7522752B全4743行恒定=145026×44＋285402×4。实际HDRRGBA16F4sample/resolve单sample、反射1280×720/GL0与更新递增；10完整ps／7份含唯一owned66327、lsof实际bin664dc919。周期5.1秒观察不证明全程无外部进程；不称严格三对／速度改善／长期／物理VRAM。CSV无逐帧replay ledger且render_phase_valid全0，8上传预算由不变源码和实际World/Residency证据限定，不冒称采样逐帧测得或GPU分阶段耗时。World6100→6700、tick总600实际推进。`reference-retained-replay-performance-r7/performance/independent-performance-audit-r2.json`51/51，SHA `f20d7a5d6e1e8fa1bb35b1096cdfd9de914b814b8eac472702514c5d6279a32e`。r1 50/51 FAIL是观察日志域误将合法先active0/no-selected-resident-water再active1当重复；新r2严格验证两事件顺序，原件保持，不改运行时或指标门槛。

新街景首r11错误默认pixelratio1，native输出2560×1440使driver自然1/FAIL，原件保持；NEW r12明确1280×720点/ratio2，成功捕获真实5000/10000ms原图，完整freshsave56。Root实际view10000，两栋建筑、浅石/木/瓦、道路/花槽/HUD可见；front水条很窄，不凭此图单独断言清晰镜像。正常构场16chunk/62967写/443shell/49roof/2tree159格，材料/几何未改。

v8普通包由r12fresh author-save创建，未用residency/settings fixture作用户世界；27/0交付核对，旧九包1971／src2988／来源168／freshcapture完整树／save56原样。v8创建后加保护，CLI4/0/0game。离源码完整副本 `/private/tmp/hellomine3d-reference-goal-menu-v8-m4pj2n9j/HelloMine3D Reference Complete v8.app` 实际PID67769自然0、lsof身份true/6.297秒；只ROOT/hidden/有界capture，无SAVE/CATALOGUE/world override/构场/edit/input。原script预期2560×1440却实际两图1280×720，保持FAIL原件；Root真正view05000显示继续游戏/临水建筑完整样板，NEW `menu-isolation/domain-audit-r2.json`26/0，SHA `9f1a9a0a95431b02f06d81a4f262f7479c7b2917624939b42aa716bcb87b17a0`；独立真实view两PNG及完整IHDR/IDAT/CRC/IEND核对。PNG÷config点尺寸=1仅该观察域，actualGL/Retina2未独立观测；cfg与Ogre/Cocoa hiddenwindow日志一致、stderr空、terrain0，普通菜单背景是静态森林，不将保存名当样板世界渲染。只读确认默认启动/保存列表读取/写入隔离，不再启动客户端或篡改旧FAIL。菜单输入0／未进入世界，普通12路线仍NOT_RUN。

精选3原字节图在 `.local-evidence/reference-visual-20261006/delivery-v8/index.json` SHA `578c134740b122e6a63703e1b94d14912d4e5aea4015fce0f26e351b18cda7b6`：r7街道、r7/v8默认菜单、严格同源范围复用的r6返回树体。各图单独保存准确来源bin/src/resource/settings/env/尺寸/AI审查，不把不同Header版本称成统一native。所有历史失败／phase／v6/v7原图均保留，不提交图片。


本轮范围的本地中文提交、当前实际HEAD、final保护收据、空own-live-handle集合和普通12路线恢复点保存到 `build/reference-visual-goal/reference-complete-v8-recovery-r1.json`。恢复前先读该新点与当前文件身份；v7被blocked历史与各失败不覆盖。Goal仍active／fresh audit1，原6参考缺失、普通输入NOT_RUN、WindowsNOT_RUN、人类审美/乐趣NOT_CLAIMED与严格配对性能延期均分别保留。五张补充城市参考用于候选套件讨论，不自动把本Goal12类扩为30–40类。


## 恢复第2 turn：实际地图与HDR链接遗漏

当前宿主重新active，之前提交后的paused状态只保留为历史宿主观测，不能算作阻塞turn或自动恢复依据。新恢复审计 `resumed-audit-r2/audit-inflight-r1.json` 实际十包2197全SHA保持、0普通input；公开CUA当前documentation0.3669秒/相同macAPI，既有4个App变量仍undefined，服务actualPID1250/ppid618/启动Oct6 13:35:45未变。无新可用API或外部状态变化，不重复4819秒/1262秒失败的getApp，不重启服务/合成输入。当前有独立实作/检查，impasse=false，无blocked建议。

地图缺口来自 `OgreUserInterface.cpp` 共用 `mapSurfaceColour`，World真实material/height未丢失。6浅石222/209/184、2陶瓦185/109/68、2木162/124/79、花槽土98/72/47、灯暖255/230/160显式覆盖12ID，旧/unknown/高差clamp和三个共用callsite逐byte不变。UI SHA932a2ae7…，真实函数及实际SurfaceMapSample抽取编译执行1793/0（72新shade/12unknown/1708旧与unknown/1负控总数）；旧2ccd函数12新族全部拒绝。独立R审核无阻断。`resumed-audit-r2/map-material-colour-r1/verification.json` SHAda5eff17…3608；旧v8不含修复，不能借旧包关闭当前map或普通路线。

同当前WaterFSbda252/VS7d95的两组新增shader归因在 `resumed-audit-r2/water-side-native-r1`，仅独立运行资源clone做常量side，正式资源不改，不回写distribution收据。control PID75789与sideConstant75981各自然0/3500、7000ms两2560×1440原图，实际RTT raw/GL0/Water绑定1；冻结r7bin664dc、模板56、源168、十旧2197保持。两个实际原图已查看：constant side变青且上缘仍有三角缺口，较低岸壁三角明暗仍可见。不能把常量着色当修复，也不能仅凭差异唯一归因；原较旧ablation的不同VS/FS不能冒称当前结果。冻结r7未具GL linked sourceID observer，明确NOT_AVAILABLE，不捏造原生program域。

HDR链接负控准备首case因错误要求相同config也必须变更而FAIL、0native，原件保留；NEW r2检查168完整成员和唯一预期资源变化。实际resolve FS `postUv vec2→vec3`、两采样保留.xy，VS仍vec2，单stage可编译但pair不能合法link。hidden/menu/noWorld/24帧 ownedPID76207自然0、1.1200526秒，forcedfallback=legacy且terrain0，未生成世界，driver1/DEFECT_REPRODUCED_LINK_ERROR_HIDDEN_BY_FALLBACK；lsof actualbin664dc，原168/十2197/HDRsource逐SHA保持，输入0。`hdr-link-fallback-calibration-r1/case-r7-r2/run.json` 是真实坏资产未被拒绝的FAIL，后续修复不覆盖它。该阶段随后完成的正式修复与当前正/负native见下一节；原76207坏资产FAIL及初次preparation FAIL均不覆盖。


## 恢复第2 turn：r8链接修复、跨水面和v9交付

正式HDR修复只在 `HdrPipeline.cpp` 对实际resolve pass的VS/FS及递归附加库做独立完整GL链接，先于forced/capability fallback和FBO安装。临时program从未bind，不进入Ogre program cache；16KiB infoLog有界，成功或异常均先删除、确认glIsProgram=false/当前program不变/GL0，再传播真实资产错误。legacy入口保持原早退，完整旧AA-less resolve资源兼容；不能用FBO catch吞掉确定的shader链接错误。适用[HDR合同](../contracts/reference-hdr-contract-v1.md)。

`reference-map-water-build-r8` 实际generate/graph/Debug/Release全exit0，2989src前后不变；生成图31/396/3222/54及9校准。四个有界hidden菜单native在 `hdr-link-current-r8-calibration-r1`：正常HDR PID78721自然0；合法forced fallback78731自然0；同一坏link78742自然1且stderr明确接口类型不匹配，未进入FBO/World；旧AA-less forced fallback78773自然0。四例实际temporaryProgram18分别linked1/1/0/1、全部deleted1/current0→0/GL0。独立 `current-native-audit-r1.json` 164／0（SHA `dd488bf99d4ed6d13394a1bf3b6a5b2ee8d357de53a7ab8fc9bc2fe520254b9a`）是身份/日志/清理观察，真实负例仍nativeexit1，非普通玩法PASS；原source168和当时十包2197原样。

[跨水面合同](../contracts/reference-water-transition-contract-v1.md)限定默认关闭owned工程入口，同Root用真实Player/logicCamera/World medium及正常模拟依次 above-a→collar→below→above-return；四阶段各12有效帧、共16journal/两次active原生RTT。Release PID79435自然0、49帧，Debug79917自然0、49帧，独立各176／0；Release实际正例上的24故障副本均命中指定门槛。collar非水/距plane≤.15，below实际Water7/eye低于plane；inactive TUS移除和linked driver flag0、updateCount不增，允许保留合法oneRTT和无效旧GL sampler绑定；above-return同一selectedplane正常同帧更新，actual driver sampler与真实attachment一致。不能把TUS setter当driver、updateCount当像素或teleport当普通行走。

真实retain-water-binding负控PID79966自然1、frame25，native summary FAILED/1阶段，明确拒绝inactive pass/TUS retained；实际写出错误绑定后由严格gate拒绝，oracle仍FAIL，普通保存未完成。`reference-water-transition-native-r1/independent-aggregate-r1.json` 119／0（SHA `d03477fbec7a6b6f6f73ee17cd97eaa38d2144b7fca0f81612f98e2324d7949f`）仅审核两真实正例和该真实负控，不能把119叫第三个正例PASS。最初oracle因self.mutation未初始化而AttributeError，冻结源和错误原件保留；唯一修复是调整初始化顺序，工具新SHA `8eb0d0be8151a15ef0f835a64dc8d49bd6cde82cc4082000a17683a5901d5037`，全部176门槛和24副本故障不变，在同一actual输出上重跑reader，没有为工具修复重新启动native。第二resident水位仍NOT_RUN_NO_DECLARED_SECOND_RESIDENT_LEVEL；这些观察不代替普通水路线或完整资源生命周期。

v9来自r13 probe-off fresh作者街景保存，没有把water/edit/residency fixture保存当用户世界。离源码完整临时副本实际PID79129自然0/7.372134秒，lsof真实Release50b061；严格7env只ROOT/hidden和有界菜单capture，无SAVE/CATALOGUE/SkipMenu/world override、input0、未点击Continue。Cocoa实际1280×720点/2560×1440backing×2，两原PNG已由根和独立代理实际查看；每图330chunks全CRC及完整RGB11059200B解码通过，两图全像素相同，SHA `ee37cc3e…5398b`。菜单列出“临水建筑完整样板”，背景是静态森林 artwork，terrain四类全0；不是实际样板World画面或普通进入世界证明。源v9完整226和临时保存56全成员/字节保持。

菜单独立 `final-workbench-v9-r1/menu-isolation/domain-audit-r2.json` 58／0（SHA `1323102333a8e79047f9d3a8a85405c6f588357eb0bcb8633ff5207ae1e1f686`）；r1两个reader域错误FAIL保留：handoff的NOT_RUN是状态字符串；准备helper79362与创建后新增v9保护helper c6bc68是不同合法时点。只去除4个v9保护token在内存中得到exact79362，未重写创建身份/旧包或放宽PNG/保存/二进制门槛。原v8尺寸假设FAIL也保持。精选3原字节图 `delivery-v9/index.json` SHA `6dc653b204c3c9c764d0d53e8d2444048a5de2dae1193644298421d63e8f8d21` 包含当前街道、默认菜单、实际above-return；各自保存身份/设置/env/原路径与审查，不宣称普通连续动作。

## 当前岸边归因：保留未解三角明暗

`resumed-audit-r2/water-side-native-r1` 两r7实际native0（control75789/sideConstant75981），`water-all-constant-native-r1` 的wholeRGBA常量76960自然0；side常量仍留三角，wholeRGBA常量抑制大块对比但同时改变RGB/alpha，不能据此唯一归因或称正式修复。`water-optical-split-native-r1` alpha常量77792/planarOff77799均自然0，三角仍在；Off正确移除反射/零target。原results因误要求reason=disabled而实际reason=off保持FAIL；独立domain22／0严格核inactive/零target/无RTT读回，只纠正日志观察域，正式shader/质量门槛未改。

r8 `water-rgb-attribution-native-r1` control80173/lighting-RGB80217/depth-RGB80233三例自然0，`water-source-shore-attribution-native-r1` control81897/skyPalette81923/shoreRGB81972三例自然0；两组各41／0原图/域核对，AI实际看07000ms原件。只替final diffuseLight*exposure、body depth RGB、skyAvailability或fragment shore RGB任一项，透明带/三角仍在。正常World时钟推进，不能跨case逐像素称唯一因果；这些clone注入资源不回写分发收据，正式shader/资源manifest未变，draw-bound源码ID未观察不伪造。随后 Fresnel/fog 三例PID83577/83595/83632全部自然0，各自原图实际查看，独立41／0（SHA `7493f9b0cbaf1ea384220479747a934e7157b98beed3be2f5a39aff32d78f34d`）；固定RGB Fresnel增强镜像、fog off近岸影响很小，三角均仍在。法线UP／完整来源neutral三例84422/84446/84470同样自然0，根代理实际查看原件，三角仍在；不能将独立final-light或sky-palette对照当所有光源消费者已排除。当前没有正式shader修改；三角仍OPEN，不扩大为全部画质PASS或宣称独立工作耗尽。


当前历史复用边界见 `reference-map-water-build-r8/source-scoped-reuse-r1.json`：World+Tests215逐SHA相同，旧4491×2/focus26×2仅限该core。Residency/Settings Header和正常安装/退役/释放函数保持，但Bootstrap新Water挂载与HDR真实链接初始化改变，所以旧330×2、三PID52/15及旧入口case只保留各自协议/精确函数/拒绝域，不宣称r8整版本全流程PASS。Planar.render仅新增lastRenderedFrame数值记录、shutdown新增probe.reset；bind/reset/releaseTarget/clearMaterials/unbindWater与Hdr.remove原文同。新HDR164、Water176×2另有当前native实证，CGL3502/旧143资源不能把严格压力FAIL变PASS。该只读审计首准备文本SyntaxError在执行前失败、未生成收据/未改源码，修正后才写唯一证据；原错误说明保留。


跨水面启动准入另有三个当前r8真实阴性：unknown-opt-in PID83302自然1/0.5976815秒、orphan-fault83339自然1/0.4393404秒、foreign-diagnostic83487自然1/0.4012328秒。分别只把16基线中的probe改bogus、移除probe保留retain fault、额外加入HELLO_PERF_CAPTURE=0；actual lsof三份均核同一50b061真实owned binary。stdout严格只有CRASH_DIAGNOSTICS/CRASH_REPORT/TRACY三条合法pre-entry前言；stderr完整为 `Ogre bootstrap failed: Reference world edit: Water transition exact opt-in1 required`（前两），或相同前缀的extra diagnostic环境拒绝（第三）。无Root/resource/Cocoa/HDR/World日志、无water/catalogue/bin saves目录；source168/template56、三个Runtime原168+config169、全部clone save56、十一包2423均全成员/SHA保持。原driver漏了既有require的 `Reference world edit: ` 前缀，所以三份run原FAIL不改；新独立 `reference-water-transition-startup-guards-r1/domain-audit-r2.json` 72／0（SHA `f602f8886959ff3c8f0dbf6582968a1ea93c2f3ce718e54f6fc7f115203de1dd`）只严格核实际完整字符串，未改driver/源码/原run、未重启native，保持三actual nativeexit1的negative身份，不叫正常Water正例PASS。


## 恢复第2 turn继续：水面透射分离与真实双水位候选

`water-transmission-attribution-native-r1` 当前r8四owned克隆实际PID84668/84695/84730/84765全部自然exit0。control、仅finalRGB常量保留原Alpha、仅Alpha0、仅Alpha1四张7000ms原件由根与独立代理实际查看。RGB固定时三角对比不明显；Alpha0的实际岸墙/床背景无所见同轮廓三角；Alpha1保留真实RGB仍显三角。结论仅收敛到visible Water RGB有贡献，不区分单面插值与top/side/backface多个Water覆盖；depth_write off/cull none等真实状态未改，不能把常量覆盖当生产美术修复。独立图域收据53／0（`image-domain-review-r1.json` SHA `c5e649a0ae65bff5c9141b5a4b5a080911d0a4bd986666015fcb9505a84e9b2f`）是技术身份／保全审核，没有质量PASS门槛。四例前后正式2989源／168源包／56模板／十一包2423逐成员和SHA保持。

新的只读保存审计发现当前v9既有64.9与66.9两个真实水位候选，不能继续将第二层说成保存中不存在。`reference-water-plane-build-r9/save-candidate-audit-r1.json` 实际18／0，SHA `edbd7f2e31a01b8cb186fe2e29fbfac2f8ac09a026c5d0451f0abce67650a116`：v9完整56文件、16主区块／48主+备份区块原样；与r13的全部48chunk成员与字节精确同。高柱(194,−183)64..66为Water7/0、67/68为Air0/0、bed63为32/0；低柱(176,−176)63/64为Water7/0、65/66为Air0/0、firstbed34为Stone3/0。保存解码和候选选面演算不证明native resident／World观察／组件选面已发生。

新增[真实选面合同](../contracts/reference-water-plane-selection-contract-v1.md)已冻结，默认关闭新mode只补同Root high→low→high→low的实际World/组件/pass/driver矩阵链和retain-selected-plane真实负控；原四阶段和其已发生证据不改。进入该阶段时实现／runner／独立oracle及r9 native尚未执行，保留其准备记录与11包2423基线；后续真实结果另列下节，不把准备收据改成运行证明。v9创建身份不回写，普通12路线仍NOT_RUN/input0。


## 恢复第2 turn：r9实际双水位与独立参考纠正

仅新增默认关闭 `plane-switch-v1` 选择观察：同一Root/Scene/Window/World，正常模拟及生产teleport，high-a→low-b→high-return→low-return，实际World选面66.9→64.9→66.9→64.9；blocking/find-only实读两柱11格Water/Air/bed，原四阶段schema、默认行为和正常运行无probe路径保持。记录组件、实际Ogre pass原矩阵和linked GL column-major读回，矩阵按真实Ogre上传规则独立转换；真实fault只对low-b组件保留旧高面，World不改。合同见[真实选面](../contracts/reference-water-plane-selection-contract-v1.md)，未新增保存字段或流送。

`reference-water-plane-build-r9` 实际generate PID88566、graph88573、Debug88574、Release88627全exit0，完整2989src及11旧包2423前后逐SHA不变；两新168/167工程包实际完成。Debug binary `739495700441960dbf5be196fee71025265e80dede6ceb712ca2eec816c51be2`，Release `1f23ca5ed547ec2c0221b3dcdc4132dc0530cf0f71413e3a238e5deac1d3309b`，src清单 `60d760b27ca79485228b826f4e820bdc978b9333c67a6478632c2a2541cea0c9`；资源143 manifest仍 `f859433d9801ca4e41f34fc3c8ed91715d16f00ffb98e970f3b95870b956b91c`。build-results SHA `b7cc01eb1d7c17249848510064d09b1203f31fe365e747dcac52c7e37e638676`。创建来源固定b170d3aa＋dirty81763f02，不因后续提交回写包。

`reference-water-plane-native-r1` Release PID88790、Debug88905均自然exit0，48帧/16records；12/24/36/48检查点的World/component/pass/GL面一致、采样矩阵和current RTT/attachment/sampler一致；四同帧RTT各1280×720 RGBA16F、四main各2560×1440，updates13→25→37→49、合法one目标同generation，正常保存与origin恢复真实完成。冻结runner `d48eb93bc969c37aa396986b94773c6381be43caee9546f1db2ff3777143cd75`，独立reader `6c96220d4ad25d9139ef25a743add43e7cfbb312ce6908ed533a5247d7fff0db` 真实Release292／0、Debug292／0；Release实际正例46故障副本全部命中指定门槛。根实际查看Release四main＋四RTT preview及Debug low-return main，低面原图可见倒置两层屋／树／桥，宽蓝侧带仍明显；不能称普通连续转头或其它水层像素fallback已完成。

真实retain-selected-plane PID89047自然1：high-a后low-b第24帧留下actual World64.9、component/pass/driver66.9且enabled0/TUS移除的真实错态，再严格拒绝。summary FAILED、save=false/restoration=null，不把旧RTT命名为low当前读回；oracle仍FAIL并单列EXPECTED_NATIVE_FAULT_REJECTED。当前r9原四阶段另有Release PID89106自然0、旧oracle176／0；旧runner仅保护其声明10包2197，额外11包2423由当前独立baseline核对，不假写旧driver曾查11包。

两次独立reader错误和原件全部保留：Release旧r1 FAIL64误省镜像camera复制时的第二次Quaternion normalization，旧r2 FAIL86误把正常渲染eye等同post-tick PlayerY+.6。现按实际Debug/Release ARM64 Norm的rounded x²/三次FMA独立重建复制姿态，再求镜像矩阵；所有完整16与XYW门槛仍5e−5。低位实际正常gravity单tick后Player65.4/v−2，生产previous65.5/current65.4插值给出eye合法段[66.0,66.1]；main==logic、单tickY/velocity及原1e−5仍严格核，stationary/nonlow仍保留Y+.6检查。实际scheduler alpha NOT_OBSERVED，未推造其值；不是调阈值或改native来造PASS。旧完整脚本、FAIL和校准拒绝Traceback不覆盖，最终纠正receipt SHA `eecbca9443ecd22ada1ec2815640f6efe3c533ed8f39785c2b05147a19d123f0`。独立重新读292×2，聚合266／0（SHA `a80427a7fa7247297ea264833d00a7a8c20a256f0f68c25d18d9f4cf10a802fa`）核源码、包、原件、完整PNG CRC/像素、保存格/identity、natural exit/lsof和保护；聚合首r1误把旧10包保护字典等同新11包的FAIL另保留。

新scenario三实际入口阴性 PID89218/89234/89238均自然1、0.418/0.431/0.441秒，严格在resource/Root/window/World前分别拒绝orphan-scenario、unknown-scenario及fault-scenario-mismatch，完整正式双前缀reason＋LF及三pre-entry日志吻合；lsof实际证书全部取得。`reference-water-plane-startup-guards-r1/domain-audit-r1.json` 100／0、SHA `264a226980fc26eaa83d383276ced8fbf886eeca22d1f11dd4a0c51808678c7f`，各Runtime169/save56、source168/template56/旧十一2423原样，未新建water/catalogue/save产物。旧r8启动reader漏前缀FAIL和VS16/FS8压力FAIL继续保留。

当前r9两归因组都未改正式shader：geometry PID89429/89441自然0，depth-all89594/89607自然0，四7000ms原图根与独立代理实际查看，两组各33／0技术审核；geometry原Alpha分类不能给每像素独占face证明，depth全部消费者固定仍有三角。geometry审计SHA `4eef973a06bf45eb4f47b89013b3405f82d3f28ffe8c0699d87eb4444f46abc0`，depth-all SHA `f535fa69d8403ccd769989eb6efb98e25f94069cdc191a681d5f37ddbab1d76d`。最初geometry r9 import失败为0native，原脚本和独立失败receipt保留；仅NEW r10补tools import路径。此前normal/source三图独立41／0（SHA `63cfae8935d349c36913684ea6ad6a389edcb19d04860c5ec431bf34c5bb39cf`）同样保留残留，未宣画质PASS。Alpha1几何分类新PID90349/90368自然0，根实际查看两原图：大红top均匀，绿侧条带边缘尖三角；它保留depth_write off/cull_none及提交顺序，只是终端覆盖归因，不证明物理遮挡或正式修复，继续独立核查。

`reference-water-plane-build-r9/source-scoped-reuse-r1.json`（SHA `a3552ad262598f9501d6be2e65884737b909c9a560a2bcfc6ace60552bba48cb`）独立核r8→r9同2989成员、仅三runtime文件改变；World/Tests215、正式资源143和C GPU输入24逐SHA相同，旧4491×2/focus26×2/shape2495×2/recipe184×2/Resource201×2/C3502及HDR164只在对应未变域复用。18个正常资源安装/退役/释放等函数提取原文同；新scenario/init/matrix观察不能由旧native整体代验，本次Water证据另实跑292×2/原176。source compiler0 warnings与实际host destination警告分开，不声称无警告。两次审计准备错误的完整脚本/错误记录保留，0native/0test重跑。


## 当前v10普通候选与离源码菜单

v10使用当前r9工程包和 `scene-v2-native-r15/street-msaa4` 真正fresh街道作者保存，不用water/编辑/residency fixture保存。根owned PID90274自然exit0/12.365928秒、无forced cleanup，lsof核真实1f23；无Water/edit/residency probe，真实scenev2两布局/12部件/2树/正常World保存，仍是作者诊断，不冒普通建造。首r14外围路径guard在0native拒绝；r15完成后外围误要求existing capture helper隐式None返回为0，原wrapper FAIL_RETAINED和捕获CAPTURED均不改。独立恢复42／0（`final-workbench-v10-r1/domain-recovery-r1.json` SHA `e532a6edb88fc8d0ca5a99e1923d56399b89f15961525ab6f63e52b83e58833c`）核实际natural exit/current源码/raw/保存/保护，只纠正读者API假设，不重启native或改门槛。根实际查看10000ms原件，两栋/瓦窗/桥/HUD可读，蓝侧带仍在。

新候选普通config严格继承v9菜单的medium shadow，其余v12字段与fresh capture相同；fresh捕获实际shadow off，唯一off→medium差异在before/27项核对中明确，不能说两者图形条件相同。普通启动器仍清除四组诊断变量、只设置自身ROOT，托管167/可写save56/全226分别记账。只有live display name/spawn准备更改，区块、备份、地图/preview均原字节保持；赠送库存和作者构场不证明正常制作。

完整226临时副本位于 `/private/tmp/hellomine3d-reference-goal-menu-v10-l8oxiz5z/HelloMine3D Reference Complete v10.app`；actual PID91272自然exit0，七环境只ROOT/hidden/有界capture，无WORLD/SAVE/CATALOGUE/SkipMenu override、没有点击Continue/input0。根实际查看5000ms原件，列出“临水建筑完整样板”和静态森林菜单；没有进入World。source226/临时save56/十二包2649/全src前后保持。独立菜单27／0与创建后保护CLI4／4已完成，证据范围仍是隐藏菜单，不代替12条普通路线。

精选当前三个原字节图 `.local-evidence/reference-visual-20261006/delivery-v10/index.json` SHA `8e3700de118c0929444549e03b551e8fc0c7531fddc2f684fcf0983792691095`：r9 fresh street（shadow off）、v10默认菜单、r9 low-return主画面（工程teleport）。准确绑定来源/设置/二进制/src/原SHA及根实际审查，保存原件不提交Git，不宣称此默认关闭观察批次带来新的美术收益。v9精选和创建身份继续保留。


v10独立审核最终收据：`final-workbench-v10-r1/creation-independent-review-r2.json` 24／0，SHA `d764d272f79da43e427aef2b1b2c90c7013fdcf2eecc94f7bf66e85117746118`；`menu-isolation/domain-audit-r2.json` 27／0，SHA `81c12f4db360e317dc8a25368fdb81120c5253424e9fb5b3ad9c90b3c7e93375`。两张原图实际查看，均显示“继续游戏／临水建筑完整样板”；完整330 chunks CRC、IEND EOF与11059200B RGB解码通过，实际Cocoa1280×720 points／2560×1440 backing、七env及lsof临时映像一致。原r1把WorldSave v12合法重复objective／inventory／actor记录误作唯一键而FAIL；完整脚本与失败收据保留，r2只纠正metadata解析域，唯一singleton和record counts、settings唯一键与全部包门槛保持。

`helper-protection-r1/audit.json` 真实CLI4／4，SHA `0c8de0bc309942a567a45ece1027eaacd014d33dbdbde9b3074152bcf0a1d4ce`；大小写alias/包后代/证据后代/重复bundle均前置拒绝，0native／0包创建。创建及postcheck使用helper c6bc，创建后96efd只新增四处v10保护上下文，内存去除后精确恢复c6bc；十二包2649全部成员／SHA保持。身份仍b170＋dirty81763，current2c73分开记账，未追改创建来源。

## 当前水面depth/order归因范围

opaque分类仅把Water pass的depth_write off改on，保持Alpha1分类颜色、cull_none与几何；新独立技术审核36／0（`resumed-audit-r2/water-geometric-depthwrite-attribution-native-r10/image-domain-review-r1.json`，SHA `d7cb54e212784cb25423ce4aaa8aabbc254fd8d92bfb464e51ccaace7a0d151d`）。远岸绿侧带／红top尖三角消失，近岸及桥下部分绿面仍在，支持局部depth/order贡献；未观测实际driver depth mask或主draw源码ID，不建立唯一病因或生产画质PASS；正式WaterVS／FS未改。

随后原RGB／原Alpha单pass只切depth_write：`water-original-depthwrite-attribution-native-r9/results.json` 的control91547／depth-on91556均自然exit0；根已实际看两张7000ms原图，三角和细线仍留，不能把分类图改善扩为正常透明水修复。独立图域审核37／0，仅ATTRIBUTION_AVAILABLE_SINGLE_PASS_NOT_A_FIX（`water-original-depthwrite-attribution-native-r9/image-domain-review-r1.json`，SHA `fe11890cda02f7aceaf0beab5cf96f4839b17bf1f2760c1a08960dc1bf785bae`）；仅资源变更尚不能建立全局透明排序。宽蓝侧带／跨面覆盖仍OPEN，12普通路线保持NOT_RUN/input0。


新r10默认关闭global-depth工程原型不改变正常候选v10／r9或正式Water资源。generate92943／graph92951／Debug92953／Release92998均自然exit0；2990源文件相对r9只改HdrPipeline.h／Bootstrap并新增1诊断header。两168文件工程包Release SHA `5d1c31c6dc561e00b6a9334c7fb1902e9db524319002f4ffbe90800a080bb822`、Debug `3c8144d541b8f30f660d9a1c87b6bbae37b797a4d1a9c24fdc87209b69ea8512`；十二包2649原样。scoped收据 `reference-water-depth-prototype-build-r10/source-scoped-reuse-r1.json` SHA `29ffa0068082540650059d4f9bdd15af8aeb0e8ed712c0a1e5839a7172d1b6b1` 只允许旧4491×2／focus26×2的World/core域复用，新getter／初始化／监听／诊断另需当前实际证据；143正式资源逐字节同，不把实验注入计为正式资源。实现提交2c73与后续helper／文档本地提交 `b5f284521c032212ab092d1602bcf9dfbc945312` 分开，v10创建b170＋dirty81763不回写。

Release control93127／hook93147和Debug hook93314均自然exit0，三例使用同一实验资源组（FS `0dd60550c1fbffe9e4144e54ba1a3e94eea7e7562798ee3a535f918ee2936aa9`，VS和完整material逐字节同）；实读全depth priority99→colour100、原GL mask／uniform／primitive query与HDR1。Release1068运行帧／312 query draw／12采样帧，Debug533／192／8；resident Water及成对depth／colour从早期9→15，每采样帧全部queued配对且排序1，extra geometry／RTT均0，不泛称全程15。根实际查看两Release＋Debug7000ms原图，三角仍留，未promote正式Water；独立D84／0与R170／0分别限定图域和有限运行证据，见下文。unknown-optin93357与missing-interface93362均自然exit1并以完整reason拒绝，前者preRoot、后者实际HDR初始化后，0 installed／draw／PNG，`--validate`未验；input0／普通12 NOT_RUN，宿主active／恢复turn2继续。

本轮Release同shader pair的独立技术审查84／0，`water-depth-prepass-hook-prototype-native-r10/image-domain-review-r1.json` SHA `7b6668be65ff657c1126ff88a64af1b5554a5da13eed8b78d485b1be3dde41df`。根／独立代理实际查看四张3500／7000原图，窄岸带变化、主要三角明暗保留；反射、床和HUD仍可见。导出证书仅每类首draw及该frame全数量，1068 frame的逐对象状态／顺序由当前native谓词执行并自然退出支撑，不冒称完整raw逐draw journal。首reader大小写字段KeyError原脚本／失败保留，修解析后再审，无native重跑。


R独立有限运行审核170／0（`reference-water-depth-prototype-build-r10/independent-runtime-audit-r1.json`，SHA `7c1d0d8d9f2abb517d59cef9388ba11a42966a164521979f96c1924da52a7316`）核双构建／2990源／包／保护、三native与两实际阴性；冻结收据未改。其warning检查仅literal小写compiler `warning:` 为0，Debug／Release xcodebuild各有1条大写destination `WARNING`，不能写成全部warning0。

正式WaterFS bda252本已先用derivative几何法线gate拒绝陡面／竖面，再判断水位高度；仅height造成侧面反射的归因不成立，这不是新修复。nearest四例93894／93908／93923／93930均自然0，独立55／0（`water-nearest-input-attribution-native-r10/image-domain-review-r1.json`，SHA `88e847951fd00e50b607890296c698d7f9138e1ed2203bb53bca86c6bee7dbb9`），实际8PNG中flat连续、depth／shore平滑bands、light近岸pink／white渐变，固定ROI无先前大tri；这是HDR编码显示域，不是原float值，也不排除精确共面或建立唯一病因／画质PASS。原正式资源depth-hook默认off93838自然0、旧oracle176／0另成立，但stderr实际有unit0 sampler unloadable／zero-texture警告，R补审`default-off-domain-audit-r1.json`真实30项／1FAIL保留，不能叫整体PASS；新独立r2仅补录known-warning上下文22／0（`default-off-domain-audit-r2.json`，SHA `e00d56f0f5cc41680ea8384b45488303ab7dce05cc0bd845bf4bf28cf51d0dbf`），原r1完整SHA `18a578765bf01349674f4c868039b71f0a3c77d06ffb05ad418d2fcad7d3c60d`／29通过1失败保持；r8两配置、r9原协议与当前r10四组stderr均同164B／SHA `72d8a1e4cadf6abfa1001e42f838538410be57addaa3136175e4579de73cb1f9`。Planar当前完整源码与r9逐字节同；不能从无时戳日志判定触发帧、唯一像素原因或无害。正常候选仍v10／r9、12普通路线NOT_RUN，本批已由根核对完整八文件与当前构建源码，按授权准备小批次本地提交。


新HDR侧面RGB法线两例 `water-side-role-attribution-native-r10` 的control94567／side-rgb-normal94575均自然0，根实际查看两张7000ms原件，独立37／0（`image-domain-review-r1.json`，SHA `c88518185c706b4fd3ac124f4bafcafbedc85ae7c5d73b682e98ecac90228b77`）另看全部四图。每例312 actual queries／12导出sample、1065／1063运行帧和resident9→15，保持global-depth实际mask／顺序、2990源／168工程包／56模板／十二包2649保护。只改陡面RGB normal并保存原Fresnel供Alpha，正式Planar几何gate未改，variant material只加explicit param1；actual side uniform未读回、Alpha compiled-bit未验，World finaltime6126／6131不同。近似岸带与主要斜三角仍在，未见充分改善，明确NOT_PROMOTE；preset中的material“unchanged”过宽文字由新audit erratum纠正，冻结preset不改。倒影中的真实屋瓦／柱／桥轮廓不能一概列为破图；普通连续观察与宽透明带的充分画质判断仍开放，不以更多因子截图循环替代普通路线。

## r11／r12完整sampler修复与失败保存

本批将正常inactive Water的完整绑定接入生产PlanarWaterReflection，仅改cpp/h与默认关闭观察Header三文件；无新World、持久化或Water资源。完整合同定义最多一owned blank TUS、借用既有Manager8×8 RGB8、active恢复和reset只清exact对象。实际probe证明inactive flag0／reflection TUS缺席／update不增，完整storage与过滤有实际查询域；return实际绑定当前RGBA16F RTT，reset释放证书0／0／removed1／failures0／manager_owned0。Manager仍拥有其纹理，不把借用记成新RTT或组件删除纹理。

r11 generate／graph／Debug／Release及双168工程包均自然0，独立22／0；首Release98995实际自然1，above-a frame12只有begin，无observation／PNG／校准，83B粗错误GL query state/error保留。r11诊断用indexed GL_SAMPLER_BINDING，当前GL4.1须按active unit查询；仅两处改为GetIntegerv并移到选unit后，未放宽GL0／state／storage／过滤。具体原失败GL enum没有记录，不能从规范推造实际枚举。旧源码、delta与反向字节保留在query-correction-r1。

r12六生成／graph／客户端构建／包子进程99605／99613／99615／99658／99736／99746均自然0，双工程包168／167；独立23／0（`reference-water-sampler-query-build-r12/r12-build-independent-audit-r1/audit.json` SHA `513f04d4884ff2dc6feff0b4a2e1414b4ea471931bbedc9015db8ebf830b3feb`）。每配置小写compiler warning:0、各1条大写destination WARNING分开记。`source-scoped-reuse-r1.json` SHA `8dd8a7b59d9b7ebe04588acd15dca0ac9c11f1ae0d8e288aa617fef993e51952`只核World/core215与正式143资源精确不变，复用旧4491×2／focus26×2的该域，不称全部r12重跑；本轮bind／观察／正常客户端另验。r11源码清单11a229…与r12 f127ef…、创建dirtyeaad／914eba分别保留，未追改。

Release99806／Debug99924实际自然0，完整四phase／16journal与正常保存；两次stderr0B。Release原reader204／0＋35指定故障成立；Debug原16／1因post-tick Player+.6错误假设而FAIL，原wrapper不改。实际Debug dt .024、velocity[0,-2,0]、requestedY67.5／current67.4000015／eye68.0540085，符合生产20Hz单tick与previous/current相机插值。新reader只修此时点，保留EPS1e−5、x/z／logic/main／rotation／World／plane，严格dt<.05、精确f32位移与端点闭区间，actualalpha仍未读。新readerb41b于同一原件各212／0，Debug37个副本全部指定gate拒绝；没有重启native。独立重审78／0（`water-fallback-sampler-native-r12/reader-reassessment-audit-r1/audit.json` SHA `cc41c269c1d64958fd8245e4ded7e06838ca8e091bda31e2f3b2c9670c1ecfce`）核当前2990／168源包／56模板、所有原native与旧16／1SHA。外wrapper保护12包2649；内部旧driver仅10包2197，是子集，不扩大其保护声明。

实际negative179自然1，collar frame25实际dry-air／inactive／update13、lastRTT12，却保留真实reflection TUS／linked enabled1，错态observation先落盘。nativeFAILED／normal_savefalse，新oracle整体FAIL（actual-process-exit），只另记EXPECTED_NATIVE_FAULT_REJECTED，不当正例，也不是副本故障。负控未曾建立fallback，不把未出现的清理证书当已验异常释放。

三个真实新进程260／270／278沿同一隔离保存正常config作HDR→legacy→HDR，无pose/time覆盖，均自然0／stderr0B。独立52／0（`water-fallback-legacy-restart-r12/independent-audit-r3.json` SHA `1acf5e240f0fe84d864b4e9da43204ebcc5444f46008955cc1889a0c1410909d`）；legacy真实inactive／window samples0／8×8RGB8一mip，HDR恢复samples4与当前RTT。r1 reader误计rglob来源/未管理额外媒体导致52／2、r2误将3个合法bin模板作media导致52／1均保留；r3按原2990源码后缀、143正式media和3bin模板逐SHA读取，未改native／资源。三7000原图已实际查看；普通设置UI／重开仍未验。

当前Release四main＋两RTT原图独立56／0，`image-domain-review-r1.json` SHA `07752e18c20656e3dd6ea5df863a3affce4b4d166b325b6e41150b83d21657cd`，宽岸带及局部斜线仍留。本版未出现旧164B警告只按实际stderr说明，不能推出唯一原因或整体画质／连续60Hz已通过。完整旧shader无可选接口、samplerCube/array与全部异常注入没有新native覆盖；本版双水位完整循环、ordinary12、编辑后普通保存重开与长期稳定性仍OPEN。CUA能力没有新变化，不重试已证长时间悬挂getApp或合成输入；本恢复仍第2真实turn，宿主active，不complete，不据此提前blocked。新必要修复／构建／候选／交接均为本轮独立进展。
