# HelloMine3D 参考画质完整交付执行记录

2026-10-06。宿主 Goal **active**，未设置 token 预算。仅当[提示词第6节](../current/reference-visual-goal-prompt-2026-10-06.md)五项退出条件全部成立才 complete。工程与静态画面通过不等于普通输入验收通过。[方案](../current/reference-visual-upgrade-plan-2026-10-06.md) A–E 范围保持；过程中的旧版本和失败见[集成历史](../archive/reference-visual-goal-integration-history-2026-10-06.md)。

## 当前版本和保护

实现工作树 `/Users/lizi/.codex/worktrees/reference-visual-v1/HelloMine3D`，分支 `codex/reference-visual-v1`。主工作区 master 和另一视觉 Goal 均未写入。首版提交 `552345675265f77f8a30dbf1ae205a52d1abb64f`；主体实现提交 `2f87fc67`，包含正常世界建筑、资源／制作、表面光照、真实倒影和普通 HDR 首选四倍多采样；水岸几何修复提交 `a7f42d0262bbd7a7d2a4a3361ef263ffb4bad64a`；当前水平水面反射修复及包保护提交 `ef6d4350c96dadcaede332089beda8c4e3f7d5e3`。仅本地提交，无推送、发布或标签。

保护首版 `build/reference-visual-v1/HelloMine3D Reference Visual.app`（182文件）和旧 `build/reference-visual-goal/GoalWorkbench.app`，不刷新或删存档。另保护 `WorkbenchCurrent.app` 的实际现状（251文件，未知操作者已改变存档，未覆盖）。v2 `HelloMine3D Reference Complete.app`（226文件）也已交付并保护。v3 `HelloMine3D Reference Complete v3.app`（226文件）也保持原样。当前独立候选包为 `build/reference-visual-goal/HelloMine3D Reference Complete v4.app`，Release arm64，bundle `local.hellomine3d.reference-current-v4`，世界“临水建筑完整样板”，seed42。包可离开源码运行；普通启动器清除三组诊断变量，只设置包自身 ROOT。托管167文件和初始可写56存档文件分别记账。

| 身份 | SHA-256 |
| --- | --- |
| 当前 Release 游戏 | `30f560f41a2328fabaf5802c30a92c5284d0b5214ee5a421c1eb570a2ccc736d` |
| src 源文件清单（含生成 inc） | `66ebb95fca75307666c2d792f065bd8a60c9c0608b3492be8690efa8cc8dd88c` |
| 正式资源 manifest | `f859433d9801ca4e41f34fc3c8ed91715d16f00ffb98e970f3b95870b956b91c` |
| r7/v4 冻结 Water.frag | `bda2529e3d2ab22a0280e23356c2e04af752ceaf615cfdf9f40f8b729890b077` |
| 当前冻结 Water.vert | `7d95d3daa8f043b24df6181cd3903c8d1699a620711a7bd0cbe0e4caed0a4df2` |
| v4 受管理包清单 | `b2ed6f06e8326fcabca7871559f4d106bcc3d32d6aad5959c170f9bd81188862` |
| 当前 Colour 数组 | `5232b1c87d25a8bef1b26ace87e07bbe7eea9520ed17601ec6446bf4eef36a95` |
| Normal 数组 | `853af45c552f7a603f9452c1ce8ceae6ca12eca4b72cd948668f110184c3ed32` |
| Surface 数组 | `7e8df2db6028b9fe27329df3b189537b66f7f8364e9a8d0f666ddadc11f6463d` |

r6/v3创建 identity 保存 `2f87fc67`＋精确 dirty diff，其2983项src及shader已逐项匹配后续 `a7f42d02`。当前r7/v4创建 identity为 `a7f42d02`＋创建时dirty diff（含新FS）；src66ebb…仍完全一致，最终shader另列。保留创建身份不事后改写。完整包与样板来源、配置、地图、备份身份见 `build/reference-visual-implementation/final-workbench-v4-r1/preparation-receipt.json`。此包尚未普通输入；临时全树副本在 `/private/tmp/hellomine3d-reference-goal-menu-v4-76_y_wp4/`，脱离源码实际启动到默认菜单，确见样板名称并自然退出0；源包226文件前后相同，临时存档也相同。仅隐藏原生诊断，没有 SAVE／CATALOGUE／world override，未点击“继续游戏”。菜单证据在 `final-workbench-v4-r1/menu-isolation`，不替代退出条件4。样板是作者构场和 starter 赠送库存，**不是普通输入建造证据**；仅准备时 live 名称和安全 spawn 改为街道保存位置，其余原始存档／备份保持。恢复旧备份会恢复其原名称和森林 spawn，已在交接中明示。

## A–E 实现与当前验收

| 范围 | 正常运行路径与当前证据 | 尚需验收 |
| --- | --- | --- |
| A HDR | 固定曝光／真实 RGBA16F／独立 owned depth，坏 shader 和坏声明明确拒绝；能力回退与完整旧包兼容已验 | 实际 resize、切世界、设置重启、卸载返回、退出和释放循环 |
| B 套件 | 12类单格部件，最多8盒、四向；BlockId33–44、Material49–60，11制作+1冶炼；选取方向归一化，opaque与邻面遮挡分离；真实 mesh/编辑/上传/保存路径 | 普通制作、四向摆放、登阶、窗洞选取、挖掘拾取及地图 |
| C 材质与光照 | 四种正式 authored albedo，3×64²×256×7 mip；线性色彩 mip、法线归一化、roughness/metalness/emission；普通/阴影完整 shader；真实最多8灯源有界索引；内外阻光修复 | 普通增删灯／墙后实际受光变化，连续低太阳和近远检查收尾 |
| D 倒影 | 单选水位、半物理尺寸且≤1920×1080 RGBA16F镜像 RTT；真实镜像相机／裁切／独立参数／驻留世界；排除水、第一人称、反馈、HUD、递归；固定图确见两栋房屋及桥／云倒影 | 普通转头、入水出水、拆墙／增灯／改岸、卸载返回后更新与有限释放 |
| E 整合 | 48×48街道／6m水道，两层阳台屋和不同布局L形工坊；2株现行 TreeGenerator 普通种植树 tag0；入口通路清楚；HDR正常首选实际 MSAA4，单HDR空间滤波回退 | 三种AA稀疏连续原图已审查；共面细纹已修并复审8帧；侧面硬切已局部修复；近似水侧着色局限保留，普通完整路线未验 |

新 ID 和合法四向 metadata 追加，世界／设置仍v12，无新增持久字段。单格邻面、封层、AO、Alpha和格级阻光分别判断。套件配方保持成本和物品守恒，12部件在真实World而非Ogre直接摆设；工位、炉子、箱子走正常交互。

正式源在 `media/sources/architectural-kit-v1.json`、`reference-visual-scene-v2.json` 和 `docs/art-sources/reference-visual-v1/`；确定导出到 .block/.shape、SceneData.inc、UI atlas与三个数组，并纳入 manifest。HDR的旧木板layer21和道路layer23分别复用 authored timber147／rough145；旧 Warm array、共享图标和 legacy 配色保持。新资源完整同 owner，完整旧渠道可回旧模式；声明部分、混 owner、坏内容拒绝，高优先旧包不被基础新色吞掉。

实际 World 构场62967／72000写尝试、19200变化、613作者区域、16场地区块；两株普通树127+32最终格，正常编辑、树皮采集、保存重开已专项验证，不伪造 natural-root。第二栋作者／整合及返工成本保存在场景源报告，构场约108ms仅是作者诊断成本。围护修复补全墙体／承接层／顶板、正确瓦阶朝向，492围护格与27瓦接缝核查；三处真实室内天空光0，室外15，本地灯光保留。旧漏空画面原件保留。

## 工程、GPU和资源证据

证据根为 `build/reference-visual-implementation/`，各记录保存准确源码、资源、二进制与命令。下面数字只描述对应范围，不作为未来固定门槛。r6新增真实 mesh 三角裁剪、冻结启动能力、SectionMeshInput policy 与 WaterVS接触固定；重新完成客户端和World双配置。HDR resolve、C材质数组、围护／构场、配方和ResourceContract未变，复用对应同源范围。814检查不能扩称新水面全部验收。

| 当前适用检查 | 结果与定位 |
| --- | --- |
| 最终 Debug／Release 客户端 | PASS／首方0警告；`water-boundary-clip-r2/{Debug,Release}-HelloMine3D-build.log`；vendored Ogre／第三方警告原样保留 |
| 完整 World 双配置 | 各4465／0；`water-boundary-clip-r2/{Debug,Release}-full-world.log`；新boundary1201，实际WATER_DEPTH1230 |
| 严格 World summary | 全量两日志验证通过；26正／负校准含当前真实4465和旧3264拒绝；旧3108、focus、失败或伪造计数不能冒充 full |
| 纯 shape 双配置／生产 recipe 双配置 | 各2495／0；各184／0（含正常成本／发现／守恒） |
| ResourcePack 双配置／生成工程 | 各201／0（unchanged合同）；新生成图31工程／396groups／3217memberships／54refs+9正／负校准（1正+8负），未手改工程 |
| HDR完整生产GPU | 814／0；`hdr-gpu-r2`对应 resolve 及颜色域覆盖 |
| 当前 C普通／阴影完整GPU | 3502／0、GL0；`reference-surface-gpu/r5`含当前道路和木色、三数组21mips、独立BRDF／法线／四向／退化／远原点与坏输入 |
| 当前 C parser／包优先级 | 37／0；`integration-r1/surface-profile-r6`；确定性 export/check PASS，68保护输入哈希不变 |
| D shader／policy | 45／0、19／0；原生真实 RTT RGBA16F、HDR>1、非有限0；当前原图确见建筑镜像 |
| 场景真实 World／导出 | 9／0＋492围护格；`scene-enclosure-r1`，6真实缺陷负例、10场景功能字段保持 |
| 普通包准备器 | 51／0；`workbench-v4-helper-protection-r1`（包括已发布v2/v3新增保护；历史43、47仍保存），身份／v12／chunk／spawn／隔离／launcher负例；未冒称游戏输入 |
| 启动坏输入／回退 | 6案例PASS；`native-startup-r3`：能力回退、坏PBR、缺variant、缺真实AA auto、legacy坏normal、旧AA-less HDR兼容 |
| AA完整生产GPU | 空间滤波97case、719工程checks／0功能失败但12质量失败；实际MSAA97case、605工程checks／0工程失败但1质量失败 |

保留失败：初次 World3108／49暴露48方向射线与1邻面遮挡，已修真实实现；3264／36为新GPU来源测试漏既有+1标签，精确修夹具且所有断言保留。初始surface编译误加header-only .cpp、旧包夹具非法脚本覆盖、并行Xcode数据库锁、初始Ogre数组API和4个CGL弃用警告、Retina参数遗漏／反射preview误计主帧均保留原日志并修正复验。不能以删测试或放宽质量门槛换PASS。

## AA取舍、内存与帧时间

空间滤波97case为39改善、46不变、12退化，薄线最坏2.89倍误差；实际四采样为94改善、2不变、1退化，单个 slope1/width1/phase0.5 的面积参考误差提高3.05%。保留真实有限采样限制，选择MSAA4不宣称零质量回退；无历史、无TAA，UI在场景后绘制。正常HDR无诊断env即优先4sample，legacy0；不支持window／真实storage／resolve时完整回退singleHDR；明确env=0供同条件诊断。

当前2560×1440：MSAA scene颜色117964800B、深度58982400B、单sample resolve29491200B，总206438400B；连窗口back/depth实际格式计339148800B。反射1280×720颜色7372800B+depth3686400B，私有≤96材质／384pass；HDR≤8294400物理像素、最多4sample。RSS不是显存，driver front/present和物理VRAM **NOT_MEASURED**。真实灯源最多27section／110592格／8源，512B索引每section；每帧仍有界扫描，不宣称无扫描。

该 AA 比较对应r4同组冻结Release／资源：无PNG读回，唯一游戏进程串行 warmup5s＋采样30s，固定水岸、RD3、medium shadow、Off post。它不是当前r7新水岸版本的性能基线；当前测量及隔离限制另列。见 `aa-performance-r1/comparison.json`：

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

## 退出条件与恢复

| 提示词第6节 | 当前结论 |
| --- | --- |
| 1 全部正常生产方向 | 实现与工程证据已接入；普通灯／编辑后倒影及A循环仍未验，不能整体关闭 |
| 2 画质和动态 | 建筑/材料/室内/HUD与真实静态倒影可见；共面细纹修复；水侧光学硬切局部修复并原图复审；近似着色和稀疏动态限制仍保留 |
| 3 工程和稳定性 | 所列对应同源检查通过，当前FS严格压力8与VS TFfloat32压力16失败另存；有界resize／切世界／设置重启／卸载返回释放循环 NOT_RUN |
| 4 普通输入保存重开 | BLOCKED_AT_PUBLIC_APP_SELECTION；全部路线 NOT_RUN，AI输入0 |
| 5 交付和身份 | v4独立macOS完整样板候选包与源/资源/收据已备；离源码默认菜单通过；水岸核心a7f42d02及当前shader／包准备器ef6d4350已本地提交；当前身份、证据与缺项文档已同步 |

恢复须有公开macOS getApp初始观察能在有效有界等待内返回可用App状态/handle，或工具提供文档化替代能力；届时先核对最新独立候选包窗口和进程身份、用户是否接管，再由唯一执行者完成普通路线及生命周期。服务未恢复时继续可独立工程／证据工作；当前已冻结v4包；最新恢复点为 `build/reference-visual-goal/reference-complete-v4-recovery-r1.json`，保存包／提交／shader身份、保护对象、未执行路线和公开API恢复边界，不自动延期普通输入要求。Goal仍active，不能complete；两次工具调用与多次上下文压缩不等于宿主规定的三个连续Goalturn阻塞审计。
