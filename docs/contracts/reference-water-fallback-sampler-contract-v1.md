# Water 停用反射的完整 sampler 占位合同 v1

状态：r12 同源 Debug／Release 固定四阶段工程观察均自然退出0，新严格 reader 在原数据上各 `212/0`；实际保留绑定负控自然退出1，有限三进程 HDR→legacy→HDR 兼容观察 `52/0`。r11 查询失败、原 Debug reader `16/1` 及所有旧原件保留。普通12条路线、连续画质与 Goal 退出条件仍开放；不写入未来 v11 结果。初始代码基线 `5ce2209e731f370e36b5c9ae32793c2249390f84`，工程身份与限制见下方执行记录。

## 问题与观测域纠正

r8 两配置、r9 原协议89106与 r10 默认关闭93838均曾自然退出0、通过旧176项工程观察，并产生同一条164字节 Apple `UNSUPPORTED ... sampler type (Float) ... zero texture` 警告。警告没有帧／阶段时间戳，不能声称其唯一成因、像素无害，或从 GL0 推导驱动没有警告。[OpenGL 3.3 core §3.9.2](https://registry.khronos.org/OpenGL/specs/gl/glspec33.core.pdf) 定义 incomplete texture 实际采样结果；这不等于本警告已经证明非法 draw。当前 shader 的 enabled0 分支在 texture 指令前返回，保持此语义。

旧 `ReferenceWaterTransitionDiagnostics.h` 仅在 `enabled > .5` 时调用 `glGetTexLevelParameteriv` 读宽、高、格式。因此旧 inactive `width/height/format=0` 是**未查询的初始化值**，不是实际 GL image storage 为零的证据。旧 texture ID／uniform flag 的查询仍属其真实域；旧结果和 default-off 补录29/1失败保留。新事实明确版本与 checked/observed 域，禁止把旧未查询数据填充成新正例。

## 正常绑定与兼容边界

`PlanarWaterReflection` 仅在真实停用 Water pass 具有可选、单个 sampler2D `planarReflectionTexture` 和 `planarReflectionEnabled` 时建立一个独立 owned blank TextureUnitState，名字固定 `HelloMine3D.PlanarWater.CompleteSamplerFallback`。原 `planarReflection` TUS 必须删除，enabled仍0、component waterSamplerBound仍false，RTT仍停更。没有这套可选接口的完整旧 shader 走原路径；不增加旧包 mandatory token、不改材质覆盖优先级或损坏资源错误处理。

blank TUS 通过既有 Ogre RenderSystem 正常 `_setTextureUnitSettings`／GL3Plus `_setTexture(enabled=true,null)` 使用 `GL3PlusTextureManager::getWarningTextureID()`。该 Manager 构造时已创建完整8×8 RGB8、base=maxLevel0的纹理，Manager 自己释放它。占位仅保持 sampler 完整，shader仍不消费反射。不得借用 UI atlas／阴影白纹理、调用 raw GL bind 改 Ogre cache、创建新纹理／RTT、保留 TexturePtr 或新增世界／存档／碰撞状态。额外 RTT bytes为0，Water pass 的 owned fallback TUS最多1个，过滤明确 linear min/mag、无 mip、clamp。

active 恢复前移除且仅移除 exact owned fallback对象，再按原路径建立真实 `planarReflection` TUS，上传当前 sampler index／VP／plane／texel size和enabled1。未知或重复保留名字冲突明确失败，不能覆盖外部 TUS。程序缺接口、pass替换与异常路径仍清理本组件拥有的对象。`resetWorld`／析构必须移除 owned fallback，清non-owning pass/unit指针；不删除 Manager 纹理，不伪称组件拥有其 native ID。每曾使用 fallback 的 reset 周期输出一次真实 TUS释放证书，包含 owned_tus、pass_bound、removed、failures 与 manager_texture_owned；成功须0/0/removed>0/0/0。

## 默认关闭诊断与精确事实

复用原严格 owned Water入口和四阶段 `above-a → collar → below → above-return`，不新增环境授权变量、原30s/2048帧、每阶段8s/512帧、连续12有效正常模拟帧、journal32／文件32／256MiB、两显式RTT readback和四 main PNG上限不变。无 probe 时不建立 listener／query／GL观察；普通绑定修复则正常生效。`plane-switch-v1` 的旧四 active阶段／World选面／矩阵门槛不变；本轮fresh四阶段与legacy结果不能追认为新绑定版本的双水位完整循环，旧292或176也不证明本版新增fallback观察。

新增 `draw.sampler.observation_version=complete-2d-sampler-v1`。真实 linked GL uniform查询 sampler的type/array size、unit，读取该unit实际2D binding。所有非零且 glIsTexture 的绑定，不论enabled，都查询真实 level0 width/height/format、texture base/max/min/mag；实际 GL_SAMPLER_BINDING 非零时独立查询该sampler对象的effective min/mag，否则effective值来自纹理。缺失／无效绑定保留 storage_observed=false及初始化值，不能据此声称实际storage；先落 observation，再由完整性门禁拒绝，不把坏绑定抹成没有错误的记录。

同时记录真实 Manager warning ID及validity、原 active unit和两处2D binding／sampler-object binding的前后数值。查询不绑定纹理，不改变 program、sampler、GL cache；原 active unit与bindings必须恢复，GL before/after严格0。`sampling_enabled`仍表示实际 linked反射flag域，不是GPU执行指令计数或placeholder采样证明。

新 collar/below 仍严格 actualWorld介质／眼高、同Root身份、正常dt、停更／旧lastRenderedFrame、reflectionTUS缺席／flag0／waterSamplerBound false。另需 fallback真实名字／数量1／exact owned对象／blank TexturePtr／index匹配actualsampler，Manager ID相同；原生sampler scalar2D、storage observed=true、8×8 GL_RGB8、base=max0、effective和texture min/mag均GL_LINEAR。above/return严格fallback缺席、enabled1、真实sampler绑定组件当前RGBA16F附件、同帧新RTT与原尺寸／depth／private-material预算不变。阴性场景不扩大显式读回预算。

## 独立 oracle、负控与范围

新 [reference_water_fallback_sampler_oracle.py](../../tools/tests/reference_water_fallback_sampler_oracle.py) 读取真实 summary／16条 journal、实际进程exitcode、raw／PNG全字节及原生runtime stdout释放证书。它保留旧四阶段身份、World、姿态／介质、clock、真实附件、raw finite／signal和PNG CRC／尺寸等独立谓词，但对新事实写精确完整性谓词，不通过先把新storage改成0再调用旧oracle来伪造兼容。旧 [176 oracle](../../tools/tests/reference_water_transition_oracle.py) 文件保持不变；本版inactive完整绑定不满足其旧初始化值假设，不回写旧PASS。

现有实际 `retain-water-binding` 仍仅在collar跳过正常bind，保留真实RTT/TUS和driver flag1；同World实际inactive事实先落盘后native1/FAILED。新oracle必须仍FAIL，最多另记 `EXPECTED_NATIVE_FAULT_REJECTED`，不当正例。校准只从真实完整正例复制故障，包含未查询域、错误sampler类型／Manager身份、错误恢复／fallback拥有者／重复TUS、尺寸／格式／mip／过滤、缺释放证书，以及原World／GL／图片／退出码等有界负例，要求命中特定门槛，不接受任意失败。

Root外部driver负责实际binary/lsof、全source/resource身份、隔离Runtime/save/catalogue、源模板与已交付包保护、自然退出／仅自有清理；oracle不伪造这些证书。工程结果仅证明本固定样板的当前绑定完整性／恢复与有界TUS释放，不证明普通游泳、连续60Hz、所有资源包、所有水位、像素增益或警告唯一成因。

## 执行记录

本轮保留全部实际失败，不重新标记原 wrapper 或旧 oracle。r11 Release PID98995 在 above-a frame12 自然退出1，只有 begin、无 observation／PNG／校准，错误为 `Water transition GL query state/error`。具体 GL error enum 未记录，不能写成已测得某个枚举。旧 Header 用 `glGetIntegeri_v(GL_SAMPLER_BINDING, unit)`；[OpenGL 4.1 core §3.8.2／表6.15](https://registry.khronos.org/OpenGL/specs/gl/glspec41.core.pdf) 的该查询依赖 active texture unit。修正仅把两处替换为 `glGetIntegerv(GL_SAMPLER_BINDING)`，并把首次查询移到 `glActiveTexture(unit)` 之后；保留原 GL0、binding 恢复、storage 与过滤门槛。[查询修正原件](../../build/reference-visual-implementation/water-fallback-sampler-query-correction-r1/correction-receipt-r1.json) 与 [r11 失败域](../../build/reference-visual-goal/resumed-audit-r2/water-fallback-sampler-native-r11/release/failure-domain-r1.json) 可核对；不将这一诊断 API 失败归到生产 sampler 修复。

r12 使用 source2990 同源 Debug／Release 包；详细binary／lsof／source身份留在Root执行收据。PID99806／99924 均自然退出0，四阶段、16条 journal、input0、正常模拟推进、正常恢复与保存。inactive 两阶段真实 linked flag0、reflection TUS 缺席、独立 owned blank TUS恰1；sampler实际绑定 Manager 的8×8 RGB8纹理，base=max0、texture/effective min/mag均linear、无sampler对象，RTT update停更。active恢复真实当前1280×720 RGBA16F RTT且fallback缺席。查询前后GL0、active unit／两处2D与sampler binding恢复，释放证书 owned_tus0／pass_bound0／removed1／failures0／manager_texture_owned0。两次完整 stderr均0字节；这是本轮警告未出现的证据，不证明旧警告唯一成因、所有设备结果或画质增益。源168、模板56与外部12包2649文件保护收据保持原样。[Release native](../../build/reference-visual-goal/resumed-audit-r2/water-fallback-sampler-native-r12/release/native/run.json)、[Debug native](../../build/reference-visual-goal/resumed-audit-r2/water-fallback-sampler-native-r12/debug/native/run.json) 保存实际身份；TUS释放不等于组件删除了Manager的GL纹理。

Release 原 reader 为 `204/0`，原35个故障副本全部命中指定门槛。Debug 原 reader 为 `16/1`，错误假设镜头眼位总是 post-tick Player+.6；实际 Player 已受一次固定.05s重力tick影响，而生产镜头用 previous/current 插值。仅 Python reader 修正此时点假设，不改 C++、正常模拟或原 `1e-5`：严格要求 teleport-reset 静止态，或 `0<dt<.05`、velocity恰[0,-2,0]、精确float32 −.1位移与眼位落在真实 previous/current+.6闭区间；保留X/Z、logic/main一致、旋转、World介质、反射与原生storage门槛。没有 actual interpolation alpha观测声明。新 reader SHA `b41b74cac567d96f6960105ca3ea3348e1eb43c4da597a7b93918ab363c1f165` 在相同原件上 [Release 212/0](../../build/reference-visual-goal/resumed-audit-r2/water-fallback-sampler-native-r12/release/oracle-r2.json)、[Debug 212/0](../../build/reference-visual-goal/resumed-audit-r2/water-fallback-sampler-native-r12/debug/oracle-r2.json)；[Debug校准](../../build/reference-visual-goal/resumed-audit-r2/water-fallback-sampler-native-r12/debug/calibration-r2.json) 为1个实际正例与37个故障副本，全部命中指定拒绝门槛。原 Debug `16/1`／wrapper FAIL不覆盖，也不是重新运行native。 [R独立重新审核78/0](../../build/reference-visual-goal/resumed-audit-r2/water-fallback-sampler-native-r12/reader-reassessment-audit-r1/audit.json)（SHA `cc41c269c1d64958fd8245e4ded7e06838ca8e091bda31e2f3b2c9670c1ecfce`）核对原件、严格插值来源、37个指定gate负副本和实际native负控。

实际 `retain-water-binding` PID179 自然退出1，summary FAILED／completed_phases1／normal_savefalse。collar真实World为空气、eye-plane约.14、component inactive且RTT停更，但真实pass仍保留reflection TUS、linked enabled1与原RTT binding；observation在拒绝前保存。新oracle整体仍FAIL，只另标预期原生故障被拒；不是正例、不是JSON副本校准。该负控从未建立fallback，不能要求不存在的fallback释放证书或据此宣称它的全异常清理已验证。[负控原件](../../build/reference-visual-goal/resumed-audit-r2/water-fallback-sampler-native-r12/negative/native/run.json) 与 [非零退出oracle](../../build/reference-visual-goal/resumed-audit-r2/water-fallback-sampler-native-r12/negative/oracle-r2.json) 保留。

有限兼容补录在同一 owned save 上以正常config进行三个独立进程 HDR→legacy→HDR，PID260／270／278均自然退出0，[独立审核52/0](../../build/reference-visual-goal/resumed-audit-r2/water-fallback-legacy-restart-r12/independent-audit-r3.json)。legacy真实window samples0／HDR与RTT inactive，Water linked flag0、sampler绑定8×8 RGB8／1mip并正常释放owned TUS；两次HDR恢复实际4samples与1280×720反射目标，三次stderr均0字节。此settings快照只证非零disabled storage／format／mips；Manager身份、exact TUS owner、base/max/filter与active恢复由上述四阶段新probe另证。不能把52项称为所有旧完整shader包、samplerCube／array接口或所有平台兼容证明。

[六张实际原图的独立有限审查](../../build/reference-visual-goal/resumed-audit-r2/water-fallback-sampler-native-r12/release/image-domain-review-r1.json) 保留宽透明水岸带与局部三角明暗残余；没有以GL0或完整sampler关闭画质问题。本版尚不证明普通游泳／连续60Hz、所有旧override、全部异常释放注入或新绑定版本的双水位完整循环。既有旧双水位／生命周期仅按不变函数和原身份限定复用，不能追认为本版fresh native。未来v11及更广普通路线结果不在本记录内；不因此重复World4491或全部GPU压力。
