# 参考画质 Goal 集成历史检查点

以下保存整合过程中的旧版本事实，当前状态以执行记录为准，不构成新任务。

# HelloMine3D 参考画质完整交付执行记录

## 范围和起点

2026-10-06：用户明确启动“HelloMine3D 参考画质完整交付”，宿主 Goal 为 active。
在 codex/reference-visual-v1 及其既有独立工作树继续 A–E 全部范围，见[提示词](../current/reference-visual-goal-prompt-2026-10-06.md)和[方案](../current/reference-visual-upgrade-plan-2026-10-06.md)。
首版有效工程证据与未验项保持，见[首版执行记录](../reports/reference-visual-prototype-execution-2026-10-06.md)。

起始 HEAD 7b39402b，完整首版尚未提交；启动时重新核对全部 2974 个 src 文件与已交付包源码清单一致。
游戏 Release SHA-256 fbd721d9d4e51e856c1ceba913000d06dec567ae32d31dc007edd4b533163667。
世界 v12／设置 v12，shape v2 四向，HDR 固定曝光。
首版 2887 世界、814 GPU 与双配置构建证据只按原版本和覆盖复用，后续变化逐项重新验证。
只读进程核对未发现游戏客户端或在跑验证；默认沙箱 ps 被拒，权限内提升只读后获得实际结果。

## 当前账本

| 范围 | 工作流 | 执行事实与下一步 |
| --- | --- | --- |
| A HDR 收尾与生命周期 | Doing | 颜色与浮点能力首版已验；当前 CUA、resize、切世界及资源循环重新核对 |
| B 建筑套件与正常获取 | Doing | 12类真实部件及11制作／1冶炼已接入；221项真实World焦点通过，普通输入及全回归待验 |
| C 材质与光照 | Doing | 三通道严格profile、四关键材质与8个附近真实灯源已接入；独立GPU3502/0，实景光照待验 |
| D 真实倒影 | Doing | 有界单水位真实Ogre RTT已运行；当前机位未确认建筑镜像，正在读回诊断 |
| E 植被／AA／第二样板 | Doing | 实施48×48双布局正式场景及有界空间AA，动态质量待验 |
| 普通输入路线 | Doing | 当前公开CUA已恢复；唯一操作者持有GoalWorkbench，最终当前版待验 |
| 最终整合与客户端 | Todo | 全部退出条件满足后交付当前版本；首版包独立保护 |

## 执行和保护

实现范围接入正常 World／revision／保存，不把 Ogre 直接摆设或诊断库存当功能完成。
主工作区 master 和另一视觉 Goal 的代码、状态、客户端不写入；本 Goal 写入限参考画质工作树。
保护已交付 build/reference-visual-v1/HelloMine3D Reference Visual.app 及其用户可用存档，不刷新原件。
验证使用另一个单一 Goal-owned 工作包；自动画面诊断隐藏不激活，公开正常输入由唯一操作者持有。

独立调查／实现归属：
B 由 asset_pipeline_feasibility 负责 Block／Item／Recipe 等域、定义形状及相关测试；
D 由 render_upgrade_feasibility 负责 PlanarWaterReflection 和 Water shader，主线程统一整合 Bootstrap；
A 和正常输入验证由 prototype_validation 负责，当前先核对服务与现有包；
主线程负责 C、多通道资源与导出、Bootstrap、样板整合、manifest、版本账本和整体验证。

## 恢复区

Goal active，未设置 token 预算。先完整保存已验证首版为本地提交，再开始新运行时写入。
所有可独立实现继续推进；工具缺能力只影响对应验收，按宿主实际阻塞规则维护，不假报完成。
实际运行 handle、包身份、失败、原图与提交按批次追加。严格配对性能沿用延期，有限资源、明显卡顿和存档可靠性仍为必验。

## 2026-10-06 集成进展（尚未整体验收）

- 首版已提交 552345675265f77f8a30dbf1ae205a52d1abb64f，保护原用户包。B/C/D均在此分支续接，当前工作区仍有本批未提交实现。
- B：12类作者源和确定性导出，追加10个稳定ID，11制作+1 Clay冶炼；纯几何双配置2495/0、生产配方双配置184/0。正式World聚焦已新增，待集中构建/运行。
- C：4张正式albedo源图、提示/SHA；六材质语义、3channel64 arrays、严格独立profile、普通/阴影PBR、World真实灯源512B/section索引及8源上限。确定性export/rebuild通过，GPU和真实运行待验。
- D：PlanarWaterReflection接普通客户端。模块GPU45/0、policy19/0只是有限覆盖；真实Ogre镜像、生命周期/编辑更新待验，不能记D完成。
- 首个Release整合构建正在执行，尚无通过结论。新source由Premake生成纳入，不手改生成工程。
- CUA能力已重核恢复，独立GoalWorkbench由唯一普通输入执行者持有；首版初始化失败留历史，不作当前永久阻塞。
- 仍需A生命周期、B/C/D真实运行、E第二栋/植被/AA、最终普通输入、当前包及原图/指标。Goal继续active。

- 整合 r1 Release客户端编译保留 FAIL（exit65）：Ogre 1.10没有Vector4指针数组的setNamedConstant重载；另诊断日志多字符newline警告。已改为真实float32数组count8/multiple4并修正newline，r2重建验证，不削减门禁。

- r2 Release客户端构建通过，首方0警告；新版World r1真实全回归3108项49失败保留，其中48个四向射线和1个相邻半砖剔除暴露实际缺陷。修正选取射线步距归一化及opaque／邻面遮挡解耦；r3 Release真实ARCHITECTURAL_KIT焦点221/0通过，尚需全回归。
- C独立严格profile生产路径26/0；实际完整普通／阴影GLSL、三数组／21mips、独立BRDF与法线、真实surface层及负例校准3502/0（reference-surface-gpu/r3）。Atlas138语义与61图标、array138层7mips及旧共享图标保留、确定性导出通过。
- IntegrationProbe-r2为独立隐藏诊断包，arm64二进制SHA beb193b740fe4ea0717d1347db8fa753ac15ae0d454dcc17b5f1adfe481c1076。实际2560×1440帧／1280×720反射RTT、RGBA16F storage PASS；street／water均exit0，峰值进程RSS约227MB。只证明当前生产运行和有限分配，不是普通玩法或最终画质通过；正面水岸图尚未确认可读建筑镜像，D继续实际读回。
- 本次E空间AA使用最多9次采样、无历史／额外RTT，在HDR resolve后且UI前计算；当前正在独立GPU和实际平台方案比较，未记验收PASS。

- 原会话已恢复六张参考附件的准确路径／turn，但临时原文件均已不存在；限定本地检索亦未找到同名副本。缺项及原路径保存在.local-evidence/reference-visual-20261006/reference-originals/index.json，按提示词可观察特征推进，不声称已对原图审美比较。
- E空间AA完整生产GLSL及独立几何面积coverage检查通过，aggregate绝对误差0.0717939对关闭0.0816977；Alpha、退化尺寸与范围通过。当前Apple M1 Pro/OpenGL4.1实际maxSamples=4，RGBA16F+depth24stencil8四采样storage／真实几何resolve支持；2560×1440需117964800B颜色+58982400B深度+29491200B resolve，先选无新增目标的9tap空间路径，后续按实际连续画面与帧指标判断。此处不是动态质量通过。
- water-hdr-r2客户端exit0，真实RTT读回非有限=0、GLerrors0，内容确有实际房屋；capture脚本把新增反射preview误当第三个主画面导致预期计数FAIL，保留失败并为主帧限定capture_*.png，同时独立纳入三件反射诊断及显式开关条件。反射混合可读性继续检验。

- 实际Debug客户端构建也通过，首方0警告（vendor警告保留）；Release完整World修复后3108/0通过，含旧世界／配置和正常主线工程回归。HDR完整GPU改后814/0通过。
- water-hdr-r3因执行参数遗漏Retina pixelRatio=2导致尺寸FAIL保留；补明确倍率的r4已CAPTURED，两个2560×1440主帧及三件actual1280×720反射诊断独立入哈希索引。

- 双配置正式ResourcePackSmoke当前版本201/0，各自构建与运行日志在integration-r1；Xcode生成工程自检含负例通过31projects。
- 恢复区：实现工作树与codex/reference-visual-v1保持；唯一正常输入执行者拥有build/reference-visual-goal/GoalWorkbench.app，原首版副本SHA fbd721d9...。root独立IntegrationProbe-r3(3b49c63d...)／agent D独立DProbe-r3均仅隐藏开发诊断，不作为最终交付或普通输入PASS。客户端自动诊断每次为新进程，按各capture.json started/finished和child_returncode记录；未终止其它Goal客户端。

- AA独立审查发现Ogre自动inverse尺寸绑定可被删而静默禁AA、以及仅剩一个surface shader声明可被forcedFallback掩盖；已增实际auto entry类型/TU0/维数检查和两variant完整性检查，坏输入将重验。AA独立斜边仅aggregate改善，8/15角度未改善，薄线／色边／退化尺寸测试正在扩充，动态quality仍待验。
- E样板v2源及有限World编辑冻结：48×48、622作者区域、62584≤72000写尝试、9032非Air格、16场地区块、两层阳台屋与L形工坊、12部件／8灯／21稀疏花草。实际作者/整合1016.76s，确定导出0.091s；不是客户端performance。direct初建补真实Furnace/Chest初始化，normal Use与保存仍需当前包验收；starter5slot单独记账。

- 当前完整E包SceneProbe-v2-r1实际Release SHA7908345104cfae984374e4072cfbd0d56fe399e0ae7bfb0e9dfdbdfac3776c19，真实World初建18937 changed／97ms、16chunks、保存成功，显式AA auto绑定与3channels实际active。
- AI实际查看street／water两张2560×1440原图：宽低岸水面可辨两栋倒立立面、窗、桥栏、灯柱与云，D独立图像审查同意该固定机位可读。普通动态/编辑仍未验。新浅石／陶瓦明显过暗，关太阳shadow同条件仍黑，定位真实v2 mesher从自身阻光格采光，而非曝光/材质源；B正修露侧四向面vertex来源，保持格级阻光。
- AA扩充97独立geometry case得到719 checks／0 functional FAIL／12 quality FAIL，39改善46不变12退化（薄线最坏2.89×）。失败全部保留spatial-aa-gpu-r2；正在接有界平台实际MSAA4对照再选择默认，不靠改门槛把空间路径宣布PASS。
- 本阶段两张真实精选原图与条件/版本/哈希保存.local-evidence/reference-visual-20261006/phase-2，当前仅局部静态视觉证据，不是Goal完成。发现native preserved grove trunks=0，场地树木协调仍须补齐，不能把空grove记自然树完成。


- 建筑取光修复保留格级阻光；新增156独立World断言覆盖封闭室内/露侧/四向/真实Lantern/内部盒自遮/上传来源。首次3264/36FAIL是新增测试未计已有GPU block-source的+1标签，精确修fixture后Release焦点377/0和完整3264/0通过；失败原件仍在。严格World summary计数3264、25正负校准通过，旧3108范围及伪造FAIL均拒绝。
- 实际原生启动6案例 native-startup-r3 全通过：合法forced能力回退、坏PBR代码先拒绝、单独缺Surface variant、缺AA实际auto entry、legacy下坏normal身份、exact首版AA-less HDR兼容。r1/r2提前退出是夹具schema/最小窗口错误，保留日志，r3使用完整包settings与640×480点。
- 独立审查补旧v2包渠道优先级兼容：完整缺席新四项接口可回旧套；声明部分或混owner仍拒绝。先校验完整新资产，再按真实first-pack-wins优先级决定，不让基础新颜色吞掉更高优先旧array。生产parser/rank专项37/0通过（surface-profile-r4），旧shader双接口全无采用旧程序，单缺/坏代码继续失败；当前实际客户端新版本待build。
- E场地seed42/terrain30真实generator48有限section查询，候选自然roots=0。将复用现行polished TreeGenerator计划2株普通种植树，经World编辑保存；tag0如实，不新增伪natural-root或存档字段。源与编辑预算正在整合。
- MSAA4 helper真实浮点颜色4样本+depth/单采样resolve/HDR信号/状态恢复15/0，仅平台有限覆盖。actual Ogre路径、质量与性能待验。Release-client-r5因同时Xcode构建数据库锁FAIL保留；顺序r6编译成功但新增CGL弃用4首方警告，已声明GL_SILENCE_DEPRECATION并等最终重建，无警告PASS尚不宣称。
- 普通验收此前pending实际为cua.getApp(GoalWorkbench.app)，4819.2s无窗口/handle/输出、无审批拒绝理由；主代理取消已过时的首版轮次后返回原文aborted by user，非人类取消Goal。普通输入0、游戏launch0；新当前包随后唯一执行者续验，当前仍NOT_RUN。旧用户包182文件、旧副本配置/35存档文件未变。
