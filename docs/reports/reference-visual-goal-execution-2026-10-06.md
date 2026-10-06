# HelloMine3D 参考画质完整交付执行记录

2026-10-06启动，持续执行至2026-10-07。宿主 Goal **active**（本次恢复审计第1个真实turn），未设置 token 预算。仅当[提示词第6节](../current/reference-visual-goal-prompt-2026-10-06.md)五项退出条件全部成立才 complete。工程与静态画面通过不等于普通输入验收通过。[方案](../current/reference-visual-upgrade-plan-2026-10-06.md) A–E 范围保持；过程中的旧版本和失败见[集成历史](../archive/reference-visual-goal-integration-history-2026-10-06.md)。

## 当前版本和保护

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
| A HDR | 固定曝光／真实 RGBA16F／独立 owned depth；坏资源／回退和旧包兼容已验；沿用上一轮v6同源生命周期代码的隐藏工程实际resize、A→B→A 正常保存关闭／重建、组件先于 Root 释放通过 | 普通窗口 resize、目录 UI 选世界和设置操作、普通行走返回及长期释放；r6真实Player驻留工程往返和r7新进程切档另列 |
| B 套件 | 12类单格部件，最多8盒、四向；BlockId33–44、Material49–60，11制作+1冶炼；选取方向归一化，opaque与邻面遮挡分离；真实 mesh/编辑/上传/保存路径 | 普通制作、四向摆放、登阶、窗洞选取、挖掘拾取及地图 |
| C 材质与光照 | 四种正式 authored albedo，3×64²×256×7 mip；线性色彩 mip、法线归一化、roughness/metalness/emission；普通/阴影完整 shader；真实最多8灯源有界索引；内外阻光修复 | 普通增删灯／墙后受光、正常连续低太阳／近远；本轮隐藏真实编辑及8帧近远证据另列 |
| D 倒影 | 单选水位、有绝对预算的半物理尺寸 RGBA16F镜像 RTT；真实镜像相机／裁切／独立参数／驻留世界；固定图确见房屋、桥／云；沿用上一轮v6同源代码实际resize／world reset的target、depth、private material、镜像相机释放通过 | 普通转头、入水出水、拆墙／增灯／改岸、行走卸载返回后更新及长期稳定性 |
| E 整合 | 48×48街道／6m水道，两层阳台屋和不同布局L形工坊；2株现行 TreeGenerator 普通种植树 tag0；入口通路清楚；HDR正常首选实际 MSAA4，单HDR空间滤波回退 | 三种AA稀疏连续原图已审查；共面细纹已修并复审8帧；侧面硬切已局部修复；近似水侧着色局限保留，普通完整路线未验 |

新 ID 和合法四向 metadata 追加，世界／设置仍v12，无新增持久字段。单格邻面、封层、AO、Alpha和格级阻光分别判断。套件配方保持成本和物品守恒，12部件在真实World而非Ogre直接摆设；工位、炉子、箱子走正常交互。

正式源在 `media/sources/architectural-kit-v1.json`、`reference-visual-scene-v2.json` 和 `docs/art-sources/reference-visual-v1/`；确定导出到 .block/.shape、SceneData.inc、UI atlas与三个数组，并纳入 manifest。HDR的旧木板layer21和道路layer23分别复用 authored timber147／rough145；旧 Warm array、共享图标和 legacy 配色保持。新资源完整同 owner，完整旧渠道可回旧模式；声明部分、混 owner、坏内容拒绝，高优先旧包不被基础新色吞掉。

实际 World 构场62967／72000写尝试、19200变化、613作者区域、16场地区块；两株普通树127+32最终格，正常编辑、树皮采集、保存重开已专项验证，不伪造 natural-root。第二栋作者／整合及返工成本保存在场景源报告，构场约108ms仅是作者诊断成本。围护修复补全墙体／承接层／顶板、正确瓦阶朝向，492围护格与27瓦接缝核查；三处真实室内天空光0，室外15，本地灯光保留。旧漏空画面原件保留。

## 工程、GPU和资源证据

证据根为 `build/reference-visual-implementation/`，各记录保存准确源码、资源、二进制与命令。下面数字只描述对应范围，不作为未来固定门槛。r6 新增真实 mesh 三角裁剪、冻结启动能力、SectionMeshInput policy 与 WaterVS接触固定；当时重新完成客户端和World双配置。本轮新增默认关闭世界编辑观察入口及Planar纯数值观察，上一轮正常生命周期修复不变，当时双构建与真实编辑另列；旧v7轮World／材质／shader／正式资源未改；本恢复轮World增加只读身份和有界Clean mesh replay，已实跑4491双配置。材质／shader／正式资源仍未改，沿用精确对应范围证据。HDR resolve、C材质数组、围护／构场、配方和ResourceContract未变，复用对应同源范围。814检查不能扩称新水面全部验收。

| 当前适用检查 | 结果与定位 |
| --- | --- |
| 最终 Debug／Release 客户端 | r7两配置实际自然exit0；`reference-settings-observation-build-r7`，Debug `aa67e7a4…`／Release `664dc919…`；完整2988src前后不变。旧r3/r5/r6构建与第三方警告保留 |
| 完整 World 双配置 | r5真实各4491／0、RETAINED_MESH聚焦各26／0；`reference-retained-replay-world-validation-r5`。r7 World/Runtime/Tests全部源码不变限定复用；旧4465和超时原件保持 |
| 严格 World summary | 4491真实双full通过、26项实际/正负校准通过；旧4465明确拒绝。只更新来自26新测试的期望计数和注释，旧工具与失败原件保留，解析/阈值不放宽 |
| 纯 shape 双配置／生产 recipe 双配置 | 各2495／0；各184／0（含正常成本／发现／守恒） |
| ResourcePack 双配置／生成工程 | 各201／0（unchanged合同）；新生成图31工程／396groups／3221memberships／54refs+9正／负校准（1正+8负），未手改工程 |
| HDR完整生产GPU | 814／0；`hdr-gpu-r2`对应 resolve 及颜色域覆盖 |
| 当前 C普通／阴影完整GPU | 3502／0、GL0；`reference-surface-gpu/r5`含当前道路和木色、三数组21mips、独立BRDF／法线／四向／退化／远原点与坏输入 |
| 当前 C parser／包优先级 | 37／0；`integration-r1/surface-profile-r6`；确定性 export/check PASS，68保护输入哈希不变 |
| D shader／policy | 45／0、19／0；原生真实 RTT RGBA16F、HDR>1、非有限0；当前原图确见建筑镜像 |
| 场景真实 World／导出 | 9／0＋492围护格；`scene-enclosure-r1`，6真实缺陷负例、10场景功能字段保持 |
| 普通包准备器 | 本轮 helper保护r2 67／0，保留旧大小写别名未覆盖缺项；普通launcher实stub33／0含旧3prefix漏口负校准，0game；v7交付27／0＋新增保护CLI4／0，九包1971文件原样；历史59等证据保留，不冒称游戏输入 |
| 适用沿用原生工程生命周期 | Debug／Release各895／0，Release oracle校准23／23；`render-lifecycle-native-r4`；另有真实 delayed-drain负控自然exit1和严格拒绝 |
| 适用沿用生命周期入口 | 十个真实负例自然exit1＋probe-off CPU validation正控exit0；`render-lifecycle-startup-guards-r2`，独立153／0；非普通输入 |
| 启动坏输入／回退 | 6案例PASS；`native-startup-r3`：能力回退、坏PBR、缺variant、缺真实AA auto、legacy坏normal、旧AA-less HDR兼容 |
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

## 当前世界编辑、低太阳与交付证据

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
| 1 全部正常生产方向 | A–E实现与正常路径保持；当前World/replay、r6真实驻留、r7切档及同源C/D工程已接入并核对。普通获取／编辑／移动观察未验，不能整体关闭 |
| 2 画质和动态 | 两栋建筑/材料/室内/HUD、真实RTT与返回树体有实际原图/动态工程证据；旧近远8帧局限、近似水侧光学与原六参考缺失保留；普通持续行走／入水出水与显著闪烁验收未完成 |
| 3 工程和稳定性 | 当前双构建0、World4491×2/26focus×2、r6严格residency330×2/42cal/实际负控、r7三PID与有界无PNG性能有对应证据；旧FS/VS严格压力FAIL保留，普通编辑／碰撞／选取／拾取／地图／保存可靠性未完成 |
| 4 普通输入保存重开 | BLOCKED_AT_PUBLIC_APP_SELECTION；全部12路线NOT_RUN/input0；工程设置和驻留不能关闭此项 |
| 5 交付和身份 | v8普通macOS完整样板候选、2988src/资源/包/保存身份与27交付核对/4新保护CLI已备；离源码默认菜单实际列出样板，原尺寸假设FAIL保留并限定审查；本轮本地提交及最终保护见恢复点，整体退出条件未成立 |

宿主保持active：本次恢复审计只计第1个真实turn，旧blocked三turn不累计；不complete，也不将工具调用、跨日、上下文压缩算成连续Goal turn。普通恢复条件仍是公开macOS `getApp` 初始观察在有效有界等待内返回可用App状态/handle，或工具提供文档化替代能力。届时先核对最新v8候选窗口/实际进程与用户是否接管，由唯一执行者完成普通12路线及编辑后保存重开。无新公开能力证据不重复getApp/getState、不合成输入或重启服务。原包、原失败和此前blocked检查点均保持历史；本轮补完此前漏列的两个独立工程项目，并保存新的active恢复点。

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
