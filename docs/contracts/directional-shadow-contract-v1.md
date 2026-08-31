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
