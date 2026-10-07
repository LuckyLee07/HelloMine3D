# V10D 可选方向阴影合同 v1

## 状态与范围

V10D 的 Windows 实现、自动回归、性能检查和开发者视觉检查已经完成，批次状态为
`Done（Windows；macOS Verify）`。项目所有者于 2026-08-28 批准在超过 10% 护栏时使用性能
例外；最终调优后的 frame P95/P99 均未超过 Off 参考值，因此该授权只作为兜底记录，没有实际
消耗，也不由 V10E 或 VISUAL-RC 继承。当前没有 macOS 目标机器，不能把 Windows 证据写成
跨平台通过。

本批只增加可关闭的太阳方向阴影和 settings v5。它不改变 save v11、terrain v3、世界生成、
方块/演员身份、网格顶点格式、图集或资源包优先级；新安装和 v0-v4 迁移均默认 `Off`。

## 档位与渲染边界

| 档位 | 深度纹理 | 最远距离 | 淡出起点 | bias | 采样 |
| ---- | -------- | -------- | -------- | ---- | ---- |
| Off | 不创建 | 0 | 0 | 0 | 旧 V10C shader 路径，无阴影采样和纹理分支 |
| Medium | 512×512 `PF_FLOAT32_R` | 64 m | 48 m | 0.008 | 2×2 PCF |
| High | 1024×1024 `PF_FLOAT32_R` | 96 m | 72 m | 0.004 | 2×2 PCF |

运行时只创建一张近景集成调制式方向 shadow map，强度按太阳强度限制在 0-0.42。solid terrain
和 actor/projectile 投射；terrain、glass/flora 和 actor 接收；水面保持 V10C 路径且不接收，避免
第一版透明水深度关系不稳定。没有级联、点光源阴影、彩色透射、云影、SSAO 或光追。

Off 会恢复原 terrain/flora/actor program，并销毁太阳灯、相机设置和 shadow texture；设置页
切档即时应用。切世界、退出和失败路径也走同一清理函数。ImGui 只在主窗口 viewport 的后队列
渲染，shadow-map 离屏队列不能提前消费 HUD 帧。

## 2026-09-19 视觉稳定性修订

用户反馈阴影闪烁后，r26 保留可选档位、64/96 m 范围、48/72 m 淡出和单张方向投影，
调整 Medium 为 1024² / bias 0.004，High 为 2048² / bias 0.002；深度纹理仍为 `PF_FLOAT32_R`。
接收端对九次深度比较结果作连续二次权重滤波（3×3），不对深度直接插值；强度上限为 0.34。
相较旧纹理，单张深度图从 1/4 MiB 增为 4/16 MiB，另有驱动深度附件成本；帧耗时另行实测。

太阳灯使用第一方 `StableSolarShadowCamera`：参考世界 Z 轴，避免太阳经过头顶时与世界 Y
重合而翻转；投影中心在光源平面按纹素对称取整，负坐标与正坐标一致。该设置归太阳灯所有，
沿用关档/切世界/退出的销毁路径，不改变 World、太阳周期、世界生成、存档或第三方默认相机。
当前资源接口拒绝旧的无权重滤波覆盖；真正的连续性由生产 GLSL 的 GPU 正反例验证。

限定画面与自动验证记录见[视觉升级执行记录](../reports/visual-upgrade-2026-09-17.md)。
连续采样显示跳变减轻，不声称所有正常玩法场景零闪烁；旧 Windows 证据保留原参数，不能套用新版。

## 2026-09-28 V05 接触深度修订

生产 GPU 复现：旧 Medium／High 的固定归一化 bias，对应当前0.5–128／192米深度范围约为
0.51／0.38米，8厘米近遮挡被判为完全受光，削弱脚底、墙脚和树根的接触感。

接收端现在由屏幕导数恢复接收平面在 shadow UV 中的深度梯度，分别求九个 PCF 采样点的
接收深度。正常投影使用原 bias 的0.10倍（约5.1／3.8厘米容差），不靠大偏移抵消坡面误差；
退化投影仍用原 bias。导数在分支及透明裁切前求值，避免 cutout 边缘的未定义导数。
terrain标准／兼容、flora接收以及actor接收使用相同计算。原3×3权重、强度上限0.34、
距离淡出、太阳相机和纹素锚定、纹理大小、档位和Off回退均不变。

不新增uniform、纹理、顶点流、渲染目标、CPU世界查询或持久状态；PCF仍为九次深度读取。
阴影求值提前到alpha discard之前，会为随后被裁切的透明片元执行对应工作；严格配对性能
按当前视觉Goal延期，不宣称本批成本为零。AO曲线、环境光与光照传播保持原合同。

`tools/validate_shadow_filter_macos.cpp` 运行生产函数：8厘米接触遮挡、平地与双方向斜坡跨纹素
移动、移除平面修正后重新出现自阴影的负例，以及原滤波／Off／夜间／距离边界。完整actor
shader及林地／岩坡实景另查；不能只凭GPU平面夹具关闭正常动态与所有场景。

## 2026-09-28 V05b 斜射投影精度修订

V11a在seed42雪地晨间复现细碎自阴影。旧V05直接对接近同值的、已带平移的shadow坐标求差；
低太阳角度和小屏幕投影下，浮点有效位损失会放大接收面梯度误差。实际GPU无障碍平面也可
得到0.5可见度，正常世界同位置medium／high可见黑点，关闭阴影消失。

V05b先对已有世界位置求屏幕切向，再用方向灯的正交投影矩阵变换方向（w=0，不带平移），
最后按原算法求UV深度梯度。terrain／flora与actor一致；深度bias、8厘米接触门槛、九次PCF读取、
滤波权重、强度、距离淡出、相机锚定、纹理和顶点预算不变。不通过增大bias掩盖精度问题。
不新增世界查询或缓存；每片元增加两个方向矩阵变换，严格配对性能仍按Goal延期。

三个阴影fragment program均新增`directionalShadowViewProj`，由Ogre的
`texture_viewproj_matrix 0`提供世界到阴影投影；不能使用再次包含对象world变换的
`texture_worldviewproj_matrix`。资源合同逐program校验绑定和索引，旧fragment缺少矩阵或
旧program缺失／错绑时明确拒绝。完整材质切换仍使用原清理和重绑路径。

GPU工具保留原60项，补terrain／actor、两档、陡坡、窄／倾斜屏幕足迹及远原点的
48项断言，共108项；新增测试必须拒绝冻结旧shader。完整actor与terrain实际GLSL、双配置
资源及客户端另验证；固定机位结果不关闭普通移动、动物／建筑及暂停恢复的剩余动态范围。

## 2026-10-07 V05c 土壁插值精度修订

V11c在当前湿地／草甸土壁复现细点：V05b虽排除了阴影投影平移，但对插值后的大世界坐标
求导仍会损失有效位。Terrain／Flora vertex新增`terrainDerivativePosition`，由对象world矩阵
只变换方向（w=0）；Flora沿用已有animatedVertex。共享接收fragment仅两处屏幕导数改用该值，
绝对世界位置仍供材料分区、树根和雾等原逻辑使用。Actor保持已有路径。

资源接口要求Terrain／Flora输出和共享fragment输入声明成套存在；三单阶段陈旧覆盖分别拒绝。
不增加uniform、纹理读取、顶点流或持久状态；新增一个vec3插值输出，bias、8cm接触门槛、
九次PCF、权重、强度、距离、太阳相机及Off回退保持。严格配对性能继续延期。

显式`HELLOMINE3D_SHADOW_PATCH_CAPTURE_DIR`仅允许全新隐藏seed42／terrain31湿地固定机位，
5000／9000ms两次原主相机绘制观察，含PNG的累计预算64MiB、期限30秒；默认不构造观察器，
不查询World、不回放、不改变正常批处理。原VBO几何投影只证明候选面覆盖固定像素，
内部VAO取数、逐片元归属和World原子快照不在声明内。诊断PNG在swap前读原back buffer，
普通定时捕获时序保持；两名义时点不能冒称普通动态或严格同tick前后配对。
当前工程与四固定场景证据见[执行记录V05c](../reports/visual-experience-polish-execution-2026-09-28.md)。

## 能力选择与回退

Medium/High 要求 vertex/fragment program、GLSL 150 和 float texture。支持时日志冻结请求档、
实际档、纹理、距离、PCF 和 bias；`HELLOMINE3D_V10D_SHADOW_FALLBACK=1` 可强制走能力不足
路径。任何能力或创建失败都完整清理并退到 Off，不阻断世界启动；从设置页触发时还会把实际
Off 原子写回配置并显示双语提示。

强制 High 回退的真实 Release 结果为：

```text
[V10D_SHADOW] requested=high active=off fallback=1 reason=forced
```

进程退出码和 stderr 均为 0。资源包冻结视图在 Ogre 窗口创建前校验七个新增 shader 以及
program/material 接口，缺失 uniform 或陈旧覆盖会明确失败。

## 自动与性能证据

- VS2017/v141 Debug/Release 客户端和受影响目标通过；Debug/Release V10D 设置聚焦均为 21/21，
  ResourcePackSmoke 为 75/75；最终 Release 世界完整回归为 742/742。
- resource manifest 为 74 项，缺项/陈旧项负例和 3 项清单用例通过；Stage 10 阴影档位补充
  身份 9/9 通过，不同档位判为 `INCOMPARABLE`，非法档位判为 `INVALID`。
- Release 启动失败矩阵 14/14 通过，包含 V10C 大气接口负例；V10D shader 的有效/漂移接口由
  ResourcePackSmoke 覆盖。
- GTX 1050 Ti、OpenGL 4.6、1280×720、windowed、RD8、同一 scaled-gameplay 夹具、1 秒预热和
  10 秒采样下，Off/Medium/High 均保持 20 tick/s、361 loaded chunks 和 1833 sections。

| 档位 | sampled FPS | frame P50 | frame P95 | frame P99 |
| ---- | ----------- | --------- | --------- | --------- |
| Off | 209.3 | 3.687 ms | 12.041 ms | 15.543 ms |
| Medium | 174.3 | 4.840 ms | 11.529 ms | 14.001 ms |
| High | 174.0 | 4.762 ms | 11.957 ms | 14.318 ms |

相对 Off，Medium/High 的 frame P95 为 -4.25%/-0.70%，P99 为 -9.92%/-7.88%；10% 护栏
无需例外。采样 FPS 只作诊断，正式判断按 frame percentile 和相同几何/驻留身份。VISUAL-RC
仍需以最终整体代码身份重跑正式 Q1/Q3。

## 开发者视觉检查

六张 1280×720 隐藏 RuntimeReadback 来自实现提交
`3c47ccb0b4aa9f600c4d328167a0060447b8fbab`（短号 `3c47ccb`）对应的 Release 客户端，GPU 为
NVIDIA GeForce GTX 1050 Ti，OpenGL 4.6。固定夹具包含 19×19 接收地面、悬浮块、贴地块、
拱/柱、glass 和 actor；正午与黄昏分别检查短阴影与斜阳长阴影。

| 画面 | 视觉结论 |
| ---- | -------- |
| Off noon/dusk | 没有投影，V10C 天空、雾、材质和 HUD 保持原路径。 |
| Medium noon/dusk | 悬浮块与 actor 接触关系清楚；黄昏长阴影连续，无整面误黑或条带。 |
| High noon/dusk | 边缘较 Medium 更细，近景拱/柱与贴地关系稳定；没有明显 acne、peter-panning、穿模或跳变。 |

所有六张原图都保留准星、状态条、快捷栏和提示，证明 shadow RTT 不会吞掉主窗口 HUD。开发者
逐张按原尺寸检查，结论为 `PASS`；正式产品体验仍留到 VISUAL-RC，不并入 R3。

```text
validation-v10d-off-noon_12000ms.png    6F852FC422AFA851AE419322B16EAD3EC9166F20126CCDFDF74C2B8818C9E81D
validation-v10d-off-dusk_12000ms.png    33BD9654D44B58849F5E2BCAA01E78C5F367DDC388ACEB1510FCCB36A2E34574
validation-v10d-medium-noon_12000ms.png EED908529B13A6645860AF6B45A573B365BA00D8E73BB1FD7ED2AED051DA9D4A
validation-v10d-medium-dusk_12000ms.png 5642400C79F5DD03F97F078281501CF30B8D1CCF5944E04958DF46242A61BE46
validation-v10d-high-noon_12000ms.png   9570A18FF65D808734B03434049988495F0E5880444529F88EFF884E61309F12
validation-v10d-high-dusk_12000ms.png   B024CEA2BD7D7147657C4084E279D2231127010D01F6D288B237F5E0B03A0FAF
```
