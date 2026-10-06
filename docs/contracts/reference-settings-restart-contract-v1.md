# Reference 设置切档与真实重启工程合同 v1

日期：2026-10-06；当前 r7 实际证据已独立审核，工程层通过。本文补充参考画质 Goal 第5节“切档后进程重启、同世界加载”的工程证据。它不证明普通设置 UI、菜单、输入或完整 Goal 退出条件。既有生命周期、编辑、驻留探针及所有已发布包保持独立且不可覆盖。

## 同存档与三个自然进程

独立 runner `tools/run_reference_settings_restart_macos.py` 只复制一次当前已验证 managed package、一次真实保存模板。新 session 使用 `Runtime.app/Contents/Resources`、`save`、`catalogue`；三个进程使用完全相同的绝对 ROOT、SAVE_DIR、CATALOGUE_DIR 和 config 路径。catalogue 是工程隔离目录，SAVE_DIR 是显式选择已有存档；不能称为普通菜单加载。原模板、原源包与所有已存在发布包逐文件前后保持，输出不得是任意 app 后代，也不得与任何输入或保护路径通过大小写物理别名重叠。

三个 stage 固定为 `stage-1-standard-hdr` → `stage-2-compatibility-legacy` → `stage-3-standard-hdr`。各为一个独立实际 PID，前一自有 child 必须自然 exit0、lsof 证明实际加载复制后二进制并完整保存后，才允许启动下一 PID。中间只原样替换正常 v12 config 的两键 `visualdetail`、`renderpipeline`：`standard/linear-hdr` → `compatibility/legacy` → `standard/linear-hdr`。其余 config 字节不变；初始普通 config 必须与调用者提供的原件完全相同且本身为 standard/linear-hdr。不是 settings UI，不能借文件改写宣称 UI 行为。

save 不在 stage 之间复拷、恢复、改名、改 world_id、改 spawn、改 Player 或任何游戏数据。stage N 保存后的全文件 inventory 必须精确等于 N+1 启动前 inventory；同一 world_id、seed、terrain generation、真实建筑 cells/meta，以及前一保存的 Player inventory、朝向、位置、health、cooldown 和 world_time 在下一进程正常装载后恢复。正常模拟、固定 tick、物理、AI、世界时间及动画照常推进；装载值与观察帧值分开记录，不能用等值图像或固定时间替代真实推进。模板含真正 World 编辑形成的 authored 内容；本任务不另造编辑循环，也不声明普通制作／放置已验证。

## 默认关闭与严格 admission

唯一入口 `HELLOMINE3D_REFERENCE_SETTINGS_RESTART_DIR=<session>/<stage>/restart-facts`。未设置时不创建探针、不读磁盘／GL、不注册回调、不修改默认运行。marker 为 session 下 `.hellomine3d-reference-settings-restart-owned`，内容精确 `HelloMine3D owned reference settings restart session v1` 加 LF。stage 名同时约束实际正常 config 档位，不能以环境注入档位。

ROOT 必须为 session/Runtime.app/Contents/Resources，SAVE_DIR 为 session/save，CATALOGUE_DIR 为 session/catalogue，capture 目录为 session/stage/frames。stage 是 runner 新建的真实目录；restart-facts 与 frames 在 admission 时不存在。session、stage、ROOT、save、catalogue 的已存在路径组件，以及 marker、save/world.meta、save/chunks 都禁止符号链接，marker/meta 必须 regular file、chunks 必须 directory。第一次 catalogue 不存在；后续允许同一正常 catalogue 已存在，但不得换目录或使用任何外国 fixture。

仅允许入口、ROOT、SAVE_DIR、CATALOGUE_DIR、WINDOW_HIDDEN=1，及 HELLO_RENDER_CAPTURE=1、DIR、MS=3500,7000、MAX_DELTA_MS=5000、EXIT=1。继承环境四族 HELLOMINE3D_／HELLO_RENDER_／HELLO_PERF_／HELLO_VISUAL_ 全剥离。任何额外同族变量明确拒绝，包括 seed／Player pose／world time／MSAA／spatial AA／材质／reflection override、validate-only、manifest 输出、其它 edit／residency／lifecycle probe 或 fault、资源包、镜头路径／fixture、性能采样。配置按生产解析，输入回调计数必须0。没有 native fault；故障校准在独立复制证据或 pure fixtures 中进行。

## 一次实际绘制快照

D 独占新 default-off helper/header 与 Bootstrap 接点。每进程只写一个 `restart-facts/snapshot.json`，schema `hellomine3d-reference-settings-restart-observation-v1`、status `OBSERVED`。没有第二套 phase 循环、瞬间切档、冻结 delta 或专用场景。正常装载完成、首个 World update 之前只读复制 loaded_world/loaded_player/loaded_world_time，Player pose／held／inventory 来自真实保存态，health／food／attack cooldown 必须读取 World 实际 getters，不能拿 getSaveState 的默认20／0当恢复事实，随后在一个正常帧实际 main Terrain 与 Water 原生产 native draw 后观察一次并撤下 observer。不得调用重绘来制造 linked program 事实。

必需字段如下；数值是原运行对象／GL 查询，不是配置 token 或磁盘抄录的自证。

| 字段 | 语义 |
| --- | --- |
| pid、stage、frame、elapsed_ms、input_event_count | 实际进程与观察帧；stage 来自合法目录；输入0 |
| config | 实际加载绝对 path、requested visualdetail/renderpipeline、窗口 points、fullscreen、renderdistance、shadow、post |
| loaded_world | observeWorldIdentity() 单锁只读 world_id、seed、terrain_generation_version；save_directory、disk_world_id 独立记录 |
| loaded_player | 首次正常装载后的 position[3]、rotation[3]、held、health、food_cooldown、attack_cooldown、inventory[{material,amount,durability}] |
| loaded_world_time | 首次正常装载值，独立与 stage 启动前 world.meta 比对 |
| world、player、world_time、simulation_delta、simulation_updates | 观察帧实际 World identity／Player 保存态、当前真实 time／本帧 delta／正常 update 次数；delta>0、time 相对装载确实推进 |
| cells | 四个原目标的位置、id、metadata、known、chunk；新增 observation 为 blocking-World.getBlock-find-only-nonAir，observed_frame 为非 bool 正整数并等于 snapshot frame。blocking find-only 的实际非 Air ID/meta 完整匹配后才派生 known=true；不可请求加载、不可把 unknown 返回 Air 视为已知 |
| window | 真正 window framebuffer 绑定下的 framebuffer、width、height、samples(GL_SAMPLES)、gl_error、bindings_restored；尺寸非0、有界，同三个进程尺寸相同 |
| terrain_draw、water_draw | 同 snapshot frame 的实际 main camera 生产 draw、object/material、vertex/fragment 程序名、GL program>0、linked=true、primitive_count>0、source section、gl_error=0、state_restored=true |
| draw.uniforms | 对实际 linked program 查询的 linearHdrMode；Water 的 planarReflectionEnabled、waterDetailStrength、waterBoundaryPinsV1；不将 compatibility 误写为全部 Water detail 关闭 |
| terrain_draw.samplers、water_draw.samplers | 活跃 sampler name、GL location、实际 unit、target、texture id、level0 width/height/depth/internal_format、mip_levels、sampling_enabled；Terrain 与其它消费 sampler 必须绑定真实存储。仅 Water planarReflectionTexture 在实际 planarReflectionEnabled=0 时允许 disabled／texture0／storage0；HDR 消费纹理还须与当帧 Planar resolved 附件 ID／尺寸／format 一致。查询后恢复 active texture／原绑定，不绑定预计资源冒充实际 draw |
| spatial_aa_enabled、spatial_aa_strength、spatial_aa_observation_domain | HDR active 时必须实际读取存在的 HdrResolve spatialAaStrength 参数并记录 finite 强度、启用态由 strength>.5 派生，域为 actual-HdrResolve-Ogre-pass-parameter；本次 MSAA4 强度0。legacy HDR inactive 时强度 null、启用 false、域 actual-HdrPipeline-inactive，不冒称读取未启用参数，也不称 resolve GL uniform；MSAA native 样本独立证明 |
| hdr | 原 HdrPipeline lifecycleFacts 的 actual active、framebuffer／colour／resolve／depth、尺寸／samples／完整性／format／GL error数值；legacy 无 HDR owned target |
| planar | 原反射 actual active、更新 frame、目标尺寸／format／samples／完整性；compatibility+legacy inactive，无当前反射 texture 消费 |

四个 frozen 原内容样点：Lantern (201,69,-186)=44/0，Water (194,66,-183)=7/0，StoneBrick (199,68,-196)=35/0，Step (196,66,-184)=33/2。runner 先独立解码模板两份 chunk 文件，native 只用四次既有 blocking `World.getBlock` 读取，在 mainMutex 下 findChunk＋hasLoaded；四个样点均非 Air，只有实际完整 ID/meta 匹配才派生 known=true。每项还记录上述严格 observation 域和当帧 observed_frame；三个 stage 观察值保持。原 SurfaceMap 是 best-effort try-lock，空批次并不证明 chunk 缺席，当前诊断不再调用它，也不增加 retry／phase。chunk `(12,-12)`、`(12,-13)`、section y4 的已加载内容必须真实读到，禁止新的 getChunk/getOrCreate/preload 请求。

standard HDR 两次要求实际 Terrain surface linked程序、terrainArray/terrainNormalArray/terrainSurfaceArray 为实际 64×64×256、7 mip 数组纹理，linearHdrMode=1；实际 window samples4、HDR MSAA/FP16 owned附件有效，spatial FXAA 按真实 MSAA 政策关闭；Water linearHdrMode=1，标准反射实际开启／当前更新。compatibility legacy 要求实际 Terrain atlas linked程序、terrainAtlas 2D 实际绑定，三个数组 sampler 不活跃且 array_sampler_count=0（并不要求无关 texture unit 上历史 binding 为0）；linearHdrMode=0，window samples0，无 HDR owned附件，Water planarReflectionEnabled=0，planar inactive。Water 基础表面与 V10C detail 仍可能正常开启，应记录实际值，不设虚假的0门槛。guard-capable Water boundary pins 可三档皆1；与画质 profile 不混为一谈。

窗口 native GL查询必须临时切到真实 window framebuffer 查询后恢复原 read/draw bindings；无需离屏清屏或额外水 RTT 读回。最多一次快照、两张现有正常 capture PNG；snapshot≤1MiB，绑定记录≤8／每 draw，native primitive query≤2个并在当帧释放。屏幕像素≤8,294,400，不扩大既有资源预算。所有异常保留原日志／partial artifact，不伪造 snapshot。

## runner、保护与审计边界

runner 先核 package managed每项 SHA、完整当前 src membership+每项 SHA、binary/source/resource receipt、plist与build identity、hidden capability和新入口／实际 sink schema／当前 loaded-cell observation 域三个 binary magic，再创建新 session；magic 只准入，实际数值仍由 snapshot 与独立原件审计证明。既有九个 published app1971文件及已存在的 v8 包完整树、source全文件、template全文件及config原件先后逐文件保护；不存在的未来包只能保护目标边界，不能声称保护了其内容。仅新 Runtime 的正常 config 和 save 可变；Runtime managed resources／binary始终保持。

每 child 独立55..120秒外部 deadline（默认90）、五秒进程清单、实际 Popen handle与 lsof loaded executable 证书。默认已有游戏拒启；显式 BUSY_HOST_ENGINEERING 只记录繁忙工程环境，不主张性能隔离。只向自己未退出的 child 发送 terminate／kill；超时或任何 forced cleanup 为 FAIL。必须三个自然 exit0，禁止 wrapper/open 或选择、信号、关闭其他客户端。每阶段保留正常 client日志、config前后原件、world.meta前后原件、全save inventory、两原PNG和native快照，不覆写失败目录。

runner 完成只写 `NATIVE_COMPLETED_PENDING_INDEPENDENT_AUDIT`；不是工程 PASS、ordinary PASS 或完整 Goal 完成。缺一次实际 native snapshot、错误身份／内容／装载继承／推进／模式事实、native非零退出、超时、源／保护文件变化，均 FAIL，不能用 NOT_OBSERVED 略过核心门槛。独立审计自行核数值、原件、PNG和 lineage；校准至少拒绝复拷模板、旧PID／旧program、自报profile、遗失数组或假legacy样本、错world_id／meta、time冻结、非法env和保护路径别名。

## 当前证据

此前静态规范与 r3 的 51/0 校准保留。新增 r4 差异校准 33/4 的真实负 section 坐标拒绝错误同样保留；runner 改为有界有符号 section 后，新 r5 必要静态差异校准 33/0。这些记录均为 pure Python／原件检查，不能替代 native。

实际 frozen r4 Release 包 `ReferenceIntegration-Release-r4.app` 的 binary 为 `a512c14bd0be79013719260f3686428aa99551cfb93ab06622826d47b3258024`，2987 项 source manifest 为 `498eb7c07e37160e498a0f6f47589433cf40971c999ec95118a4737330864694`；创建身份是 `c304e5b720e13c5a8a15d42010432680a7dd53cc` 加当时 tracked diff `85942516f5445ce077b1912339d61be45b5233eb123d12c7695b4a13962b7a41`，不因后续 Clean replay 源码变更改写。[实际运行](../../build/reference-visual-implementation/reference-settings-restart-native-r2/release-three-processes/run.json) 中三个自然 exit0 进程为 35102／35118／35143，同一 private save 仅克隆一次；world_time 装载／保存链为 6128→6258→6400→6530，Player pose／inventory／health／cooldowns 逐阶段继承，四个 resident 内容样点保持。配置只切 visualdetail／renderpipeline 两键，normal delta/time 始终推进。

[独立审核](../../build/reference-visual-implementation/reference-settings-restart-native-r2/release-three-processes/audit-r1.json) 实际 52 项／0 失败，并由三份真实 snapshot 的 11 个纯内存故障拷贝全部拒绝校准；三阶段 7000ms 原 PNG 均实际查看。同源／同 executable 的实际 linked Terrain/Water draw、全部 consumed 纹理、原生 HDR4/legacy0 storage、标准 AA strength0／legacy inactive 域匹配。九个发布包1971文件、源包168文件、模板56文件与 config 原件逐项保护。legacy 只证明无 active HDR/Planar RTT 附件和实际反射消费；仍存在的 Planar CPU camera／selection／binder 状态不冒称全组件或缓存销毁。

[独立早期拒绝审核](../../build/reference-visual-implementation/reference-settings-restart-native-r2/release-three-processes/startup-guards-independent-review-r1.json) 核 frozen r4 Debug `de58e9915df767002eccffbd848a297f8f32b4d4371c56cfe51e0bd88a14c859` 的10个实际入口负例均自然 exit1、资源／Root／窗口创建前拒绝，source168／template56／各自 owned link fixtures 原样；不注入额外 NO_DIALOG／report 环境。旧 [native r1](../../build/reference-visual-implementation/reference-settings-restart-native-r1/release-three-processes/run.json) 的 stage1 自然 exit0 后外来34882出现、stage2 launch 前拒绝（launch_count1）仍为真实 FAILED，不操作该外来客户端。

本轮结论仅为 **BUSY_HOST_ENGINEERING 三真实进程的同世界切档重启工程层通过**，runner 原件仍 `NATIVE_COMPLETED_PENDING_INDEPENDENT_AUDIT`，独立 audit 单独给出范围结论。普通设置 UI／菜单／输入、性能隔离、后续不同源码或新 binary 均不由本证据追认；普通验收仍 **NOT_RUN**。

后续 r6 `ReferenceIntegration-Release-r6.app` 的实际 binary 为 `b4a166017b3a93717e5e0f13b88017e717bf37913f3bbd30919f2ba32236de2c`，2988 项 source manifest `a26fbe0ca249dc41cd6fdd08c72b58a5870a502f584b6d44851fff3147d8aec5`。其 [实际失败独立复核](../../build/reference-visual-implementation/reference-settings-restart-native-r4/release-three-processes/r6-failed-independent-review-r1.json) 保留 native FAILED：仅 PID43985／launch1／自然 exit1，stderr 是 resident samples absent，snapshot0／PNG0。19项身份、原件和保护复核无缺项，不把这些复核或 stdout 的成功 GL 初始化／storage 当成重启通过；原包168／模板56／九发布包1971逐项不变。源码支持错误来自 SurfaceMap 四项准入路径，原件没有 mutex ownership 观察，不能把具体 try-lock 失败当成直接记录的事实。

当前 r7 修复只改变 default-off header 的 cells 读数段：移除 SurfaceMap，采用上述 blocking find-only 非 Air 域与同 frame 证书。首次装载 Player／World／time 与 admission 字节保持；World core、存档格式和正常渲染同步不变。当前 reader 同时严格核域、真实正整数 frame、非 Air ID/meta 与 known 真布尔。旧 r4 三进程正例及 r6 失败均不被追认成 r7；新双配置包与三真实进程／三原图的当前独立证据如下。保留52审核门槛与原11个实际副本故障，并为新的 domain／frame 字段单列缺域、错域、旧frame、bool-frame四项必要负校准，不减少旧门槛。

当前 r7 Release `ReferenceIntegration-Release-r7.app` 的实际 binary 为 `664dc919bb1d6faa8f449680d8f61823f530e9b62580917852d2b6e235860e06`，2988 项 source manifest 为 `4bf8f24faa1b7d22aca4545fe6e474b159eab72ecb9ae85fda05e5b12f4f41dd`，settings header 为 `727b7859fbd86478bd110153e568252730e14b28d4f1dff7074f797ce14b23c5`。原创建身份保留 `c304e5b720e13c5a8a15d42010432680a7dd53cc` 加 tracked diff `8ecfc8755824ad90c6f5a57a93b12e284493b627ee8fec80ef98caa44e1307dc`，不改写为后续提交。生成图只复用已实际验证的 r5 source 成员集合；r7 当前 source 内容另由双配置 build 的全集前后逐 SHA 与 package receipt 绑定，独立审核时当前 checkout 与 r7 冻结 source 无差异。

[当前三进程独立审核](../../build/reference-visual-implementation/reference-settings-restart-native-r5/release-three-processes/audit-r1.json) SHA `71a5e845a9d4b30a6f7504beae137e0b96313c3eba92cc62e053081ac4ac7314`：52 项核心／0失败，实际三份 snapshot 的原11项加新4项故障副本全部拒绝，共15／15；没有制造新的成功 snapshot。三个自然 exit0 PID 为66710／66820／66932，同一 save 的装载／保存时间链为6128→6255→6397→6527，正常 delta 均为正，Player／world_id／seed／generation 和四个内容样点由磁盘保存链与实际 runtime 独立比对。四项 cells 均为真实非 Air ID/meta、known 真布尔、固定 blocking find-only 域，observed_frame 是非 bool 正整数并分别等于 snapshot frame2／1／2。

| 当前原生数值 | standard HDR（stage1／3） | compatibility legacy（stage2） |
| --- | --- | --- |
| window GL_SAMPLES | 4／4 | 0 |
| 实际 linked Terrain／Water program | 26／30 | 13／18 |
| 实际消费 terrain array sampler | 3，64×64×256、7 mip | 0；消费256×256、9 mip atlas |
| HDR／Planar RTT | actual MSAA4／FP16 owned附件和当前 resolved texture 消费 | inactive；无 active owned target，planar sampler disabled、texture／storage0 |
| spatial AA | 实际 Ogre-pass strength0、enabled false | strength null、enabled false、HDR inactive 域 |

[三张7000ms原图实际查看记录](../../build/reference-visual-implementation/reference-settings-restart-native-r5/release-three-processes/image-review-r1.json) SHA `19cce6d1f5a8fbf907c1b3b5a9a6a7106e16a9a704c09f39b48959b39302d393` 严格绑定当前 run SHA `a246d1086e61738e7be4c95e107d52b13f86c217d2a4ef9b6c136dac5064cc13` 和三个2560×1440原 PNG。两种布局的建筑、闭合屋顶、桥、花槽、水道与同一 HUD 视点保持；legacy 的纹理／照明与反射显示不同，第三进程恢复 standard 外观。动物与 World time 照常推进，不做等像素承诺。

本轮 source包168／managed167、模板56、config原件及实际已存在的十个发布包2197文件（包括v8）逐文件原样；runner 的历史字段名 `all_nine_published_apps_unchanged` 不代替集合审核，独立审核明确核了十包。原件仍为 `NATIVE_COMPLETED_PENDING_INDEPENDENT_AUDIT`，上述独立审核只给 **PASS_ENGINEERING_THREE_PROCESS_SETTINGS_RESTART**。BUSY_HOST_ENGINEERING、input0；审核器未启动客户端、未改源码。r6 stage1 launch1／自然exit1／snapshot0／PNG0及更早 foreign-client 拒启失败均保留；旧入口负例只覆盖其原 frozen r4 版本，不追认 r7 新 native。普通设置 UI／菜单／输入仍 **NOT_RUN**，不声明性能隔离、动态画质或完整 Goal 完成。
