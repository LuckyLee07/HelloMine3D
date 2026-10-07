# Water 反射 shader 能力合同 v1

2026-10-07。当前r13双配置构建及下列实际原生范围已验证；同pair pass替换和普通输入仍NOT_RUN。本合同修复合法完整旧 Water 资源组没有反射消费者时仍分配 RTT、随后绑定失败的兼容缺口，沿用[平面倒影合同](planar-water-reflection-contract-v1.md)、[完整停用采样合同](reference-water-fallback-sampler-contract-v1.md)和[实际启动链接合同](reference-hdr-contract-v1.md)。不改变 shader、颜色域、世界、网格、存档、设置、反射预算或普通输入。旧视觉残差和数值压力失败保持原结果。

## 三态准入

在 World／worker 创建及第一次反射 render 之前，`PlanarWaterReflection::prepareWaterPass` 接受实际 Water technique0/pass0 和现有启动路径已经真实编译、完整链接的临时 GL program。实际 vertex／fragment stage、GLSL backend、loaded 状态、compile error 和原 shader handle 必须有效。现有 `configureWaterBoundaryPins` 的 actual compile／link 错误继续失败；该 VS mesh-pin 能力与本合同的 FS 反射能力独立。

反射接口恰好是以下五个 scalar uniform（不是 uniform 数组）：

| 名称 | Ogre 类型／元素数 | 实际 GL 类型 |
| --- | --- | --- |
| `planarReflectionTexture` | GCT_SAMPLER2D／1 | GL_SAMPLER_2D |
| `planarReflectionViewProj` | GCT_MATRIX_4X4／16 | GL_FLOAT_MAT4 |
| `planarReflectionEnabled` | GCT_FLOAT1／1 | GL_FLOAT |
| `planarReflectionPlaneY` | GCT_FLOAT1／1 | GL_FLOAT |
| `planarReflectionTexelSize` | GCT_FLOAT2／2 | GL_FLOAT_VEC2 |

分类同时检查实际 fragment program 和 pass 的 named constant 定义、`arraySize=1`／元素数，以及实际完整 linked program 的 active uniform 名、类型、长度和可用 location。当前 vendored `GLSLShader::buildConstantDefinitions` 从实际预处理 source／attached child source 提取 uniform 声明，保留驱动优化掉的声明；它不是 `GL_ACTIVE_UNIFORMS` 的同义词。Ogre 会给 scalar 也生成 `[0]` 参数别名，不能用参数别名判断数组；实际 linked GL uniform 的 `[0]` 名明确拒绝一元素数组。

- **absent**：五个 fragment／pass 声明全缺席，且 linked reflection uniform 全缺席。这是合法完整旧 Water shader。请求 HDR 仍保持 HDR，Water 继续自身近似着色；组件不创建／更新反射颜色目标、owned depth、私有 reflection material 或附加 shadow draw，不添加反射 TUS／停用 sampler placeholder。复用镜像 camera 可以保留一台；它不是 consumer RTT。
- **complete**：五个声明类型与大小均正确，实际 linked 五个 uniform 也正确且可用。继续现有反射、介质／collar／投影／能力回退、实际 RTT 绑定与停用完整 sampler 路径。
- **invalid**：部分声明、程序与 pass 定义不一致、错误类型／数组、active 与 declared 集合不一致、compile／link 错误均明确抛出资源错误。特别是声明完整但五接口被编译器全部／部分优化掉，不能伪装成 absent 旧资源。错误不会因 reflection-off、legacy、float capability 或强制 fallback 被吞为成功。

损坏 program 的默认参数本身可能在资源初始化期间已拒绝；这也属于真实资源失败。旧资源正例必须是 coherent 的完整 Water vertex／fragment／program／default_params 组，不能只把旧 fragment 塞进仍声明新默认参数的当前 program 后称“完整旧包”。

## 正常帧与所有权

启动临时 GL program 从不 bind，原程序对象与 child shader 仍由 Ogre 拥有；本分类仅查询已存在证书。记录实际 `GL_CURRENT_PROGRAM` 前后相同、GL error 为0；删除临时证书仍由原启动 try／catch 执行。没有额外 GL program 创建、每帧 compile／link、测试环境开关或新纹理。

普通帧在 `render` 前 prepare 当前真实 Water pass。只核五个实际声明、已认证 VS／FS resource handle 及原 GL shader handle；缓存只保存数字和非拥有 pass 指针，不持有 program／material／texture 引用。启动资源不可热载；不同 program pair／GL shader 对象必须由 owner 提供新的真实 link 证书，未认证直接拒绝。无 prepare 不得先 render。

同一已认证 program pair 替换 Water pass 时，在旧 pass 仍存活的主 frame 边界先移除组件拥有的 reflection／fallback TUS，释放 RTT／owned depth／私有材质、camera LOD 与 binder 引用，再准备新 pass。不能将前一 pass 的 active 标记／纹理移交为新消费者。旧 pass 销毁前仍必须调用正常 `resetWorld`；组件不拥有并延长 pass 生命周期。

正常 World reset 同样释放既有资源和全部 pass／binder 引用，清当前 prepared pass，保留同一冻结 program pair 的纯数值证书以供再载入核对。正常组件销毁仍先于 Scene／Root；不扫描／清空其它 Scene 或 Manager 资源。

## 可观察事实与待测范围

启动日志 `[WATER_REFLECTION_CAPABILITY]` 给出 `state=absent|complete`、declared／linked 数量、actual program 名、`no_consumer`、current program 前后及 GL0。absent 普通帧实际停止原因是 `shader-interface-absent`，不是 HDR legacy fallback。现有 transition numeric facts追加 `shader_capability`、`shader_declared_interfaces`、`shader_linked_interfaces`、`shader_pass_prepared`；原 pass／target 字段和旧四阶段协议语义不变。

协调方应在独立 owned clone 保留以下真实正负原件：

1. 当前完整 Water：complete 5／5；现有 above／collar／below／return 的真实 RTT／TUS／linked sampler 与正常清理保持。
2. coherent 历史完整 Water：absent 0／0；原 HDR active 不回退，实际居民水仍渲染近似，反射 update／RTT／depth／私材／附加 shadow 均0；VS pin cap0 不作为 FS absent 的替代证书。
3. 单个声明、缺一个接口、sampler cube／整数2D／array（包括 `[1]`）、float vector 或 matrix 大小错误：实际 shader compile／link可先成功，但分类明确失败且在第一次 RTT 前退出。
4. 五声明完整但实际不消费、全部优化掉：declared5／active0 的资源错误，不是旧兼容成功。
5. 真实坏 GLSL 或跨 stage link：保留原 compile／link 失败；reflection-off／能力 fallback 不掩盖。
6. 同 pair pass 替换及 World reset：旧 owned TUS／目标／私材清理后重新准备，数字证书无资源延寿。若没有实际原生替换证据，保持此项 NOT_RUN，不能以源码检查替代。

本次只完成源码／合同和静态检查，不自行构建、启动原生／UI或改发布包。工程资源兼容、隐藏固定帧与 sampler 完整性都不等于普通画质验收；水岸蓝带、局部三角明暗、VS16／FS8严格压力失败及普通路线继续分别保留。


## 修复前真实失败与 reader 增量

修复前冻结 commit `e794f7b584e6bdcf757783c691a6c4d0f11cc1e6` 的2990项源码均与 r12 Release 包 receipt 匹配；actual executable SHA `b6d2d61371ea6a739a4cba6b532132f4b1abd92b5ec5d719ed57506bba416768`。协调方在独立 Runtime 克隆注入 commit552的完整 Water vertex／fragment，以及从同 commit 提取的 Water program／default_params 和 material stanza；组合文件中的其它当前 stanza不变。四个 coherent 覆盖成员中 material与原包逐字节相同，因此实际改动只有 vertex／fragment／program三项、原168文件中的165项不变。保留的 `water-legacy-interface-native-baseline-r1/native-r1/run.json` 是错误要求“实际四项改动”的 **pre-native driver FAIL**（native_launches0），不能称游戏失败，也未将它重写为 PASS。

修正读数域后，新 `water-legacy-interface-native-baseline-r2/native-r1/run.json` SHA `bcf399bb5de30b5d8554cdf39c2d12113b7c19e78322f66ecd5e83ac8c7a2605` 记录实际 PID8586、lsof加载上述 copied executable、自然 exit1／3.142515秒。stdout已实际建立 linear-HDR、fallback0／MSAA4及1280×720 RGBA16F reflection目标，旧VS实际 compile／link成功且 pin cap0；随后反射 active／plane66.9，stderr精确为 `Ogre bootstrap failed: Water shader missing planar reflection interface: planarReflectionTexture`。这是合法完整旧 FS 无反射消费者仍先创建 RTT、再硬失败的真实兼容缺陷原件，不是 shader compile／link缺陷。源包168文件、模板56文件及13个保护app／2875文件保持；输入0、busy-host工程范围，不声明性能隔离或普通验收。以上两个失败均不覆盖；修复后的同源正例及其精确范围见本合同末节。

现有 `tools/tests/reference_water_fallback_sampler_oracle.py` 的原 reader SHA `b41b74cac567d96f6960105ca3ea3348e1eb43c4da597a7b93918ab363c1f165` 全文冻结在 `build/reference-visual-goal/resumed-audit-r2/water-shader-capability-oracle-preparation-r13/frozen-oracle-b41b.py`。新 reader只在四个实际 phase checkpoint分别增加 complete／declared5／linked5／prepared true四个严格谓词；原212检查、既有37负控定义与1e−5数值门槛不变。新 ignored校准工具仅从未来真实 COMPLETE journal派生 cap-absent／count4／unprepared三个 JSON故障副本，要求命中特定能力门禁；它们不是原生坏 shader执行，不将旧原件人为补字段当新 positive。实际原生部分声明／错误类型／optimized-out资源负例由协调方单独执行与审计。

## r13实际验证更新

两客户端／两168工程包自然0，creation e794＋dirtyd9df保持。旧完整资源组在old-hdr9509、old-legacy9574自然0，实际absent0／0、GL0、反射无consumer资源；旧HDR继续active。partial9639／wrong9675／optimized9698均自然1，具体错误及0capture／0save变化保持原FAIL；float[1]数组10140自然1、具体linked type/array PlaneY拒绝，证明真实链接后数组分支。[五例独立审计](../../build/reference-visual-goal/resumed-audit-r2/water-legacy-interface-native-r13/independent-capability-domain-audit-r1.json)141／0。

Release9726／Debug9810当前四phase各228／0，实际complete5／5／GL0。原212谓词、37副本定义与EPS1e−5不变，Release37＋新增3副本校准通过；真实retained-binding9868仍整体nativeFAILED／oracleFAIL，指定错态拒绝单独记账。reset正常清理证书有效，但没有新增同pair pass替换或普通World切换输入证据；samplerCube／整数sampler和其它数组/矩阵错误组合并未逐个新native覆盖，不能扩大声明。[独立恢复收据](../../build/reference-visual-goal/resumed-audit-r2/r13-native-independent-recovery-r1/receipt.json)05c7dc55…ddb477核完整source2990／media143／双包168／167与保护13包2875，实际读取原journal与校准SHA。所有原失败、图像缺项、普通12路线NOT_RUN保持。
