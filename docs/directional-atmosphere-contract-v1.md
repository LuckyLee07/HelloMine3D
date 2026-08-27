# V10C 定向大气与空间云层合同 v1

## 状态与范围

V10C 的 Windows 工程、自动回归和开发者视觉检查已经完成；视觉结论为 `PASS`。批次暂保留
`Verify`，只等待快速流送 Q1 单样本可见延迟的例外决策。macOS 没有本机目标，继续为
`Verify`，不得把 Windows 结果写成跨平台通过。

本批只改变天空、雾和相关 shader 参数：不改变方块 id、terrain v3、save v11、settings v4、
掉落、世界生成、网格格式、图集或资源包优先级。`WorldEnvironmentState` 的大气合同版本固定为
1。

## 定向雾

世界状态继续提供原有 `fogColour`，并增加 `fogSunwardColour` 和
`fogDirectionalStrength`。视线的水平分量与太阳水平分量对齐时，低地平线按三次方权重向暖色雾
混合；视线抬高后由 smoothstep 衰减回基础雾。背向太阳、垂直观察或退化向量均严格回到基础雾。

地形、水、演员和天空使用同一公式与同一组状态。flora 通过 terrain fragment path 继承该语义；
固定功能雾仍使用基础色作为安全底色。这样黄昏顺光方向呈暖色，逆光方向保持冷灰紫，且远景地形、
水面和天空不会在地平线产生不同颜色的断带。

## 有界空间云层

V10C 使用世界空间水平 slab，而不是跟随相机的无限投影：

- 中心高度 168、厚度 24，合法高度区间为 156 至 180。
- 水平噪声尺度 92，速度 `(1.6, 0.55)` world units/s，最远采样距离 2400。
- 相机射线与 slab 求 near/far 交点；下方只看向上方时命中，上方只看向下方时命中，层内水平射线
  可在有界距离内采样。
- near/far 两个低频样本提供厚度和云底/云顶明暗，不实现天气、碰撞、体积 raymarch 或云影。
- 位移直接由绝对 elapsed time 计算，不累积逐帧 delta，因此帧率变化和拆分更新不会改变位置。

该模型保留像素化轮廓，同时使云在地形上方拥有可观察的高度、透视、平移和上下表面关系。

## 能力选择与回退

启动时先检查顶点/片元程序能力和 GLSL 150。支持时日志写入：

```text
[V10C_ATMOSPHERE] enabled=1 fallback=0 reason=supported
```

`HELLOMINE3D_V10C_FALLBACK=1` 可强制回退，日志写入
`enabled=0 fallback=1 reason=forced-fs2`。回退关闭定向雾强度并恢复 FS2 的无限上半球云公式；旧公式
继续使用 `time_0_x`，V10C 使用不循环的绝对 `time`。回退不是仅跳过绘制，而是已由真实 Release
OpenGL 画面验证的旧路径。

`AtmosphereShaderContract` 在创建 Ogre 窗口前，从资源包解析后的有效资源视图校验 program、
terrain/water/actor/sky shader 接口。缺失 V10C uniform 或错误覆盖会在启动阶段给出明确失败，不把
问题拖到 GPU 编译后的黑屏。

## 自动证据

- VS2017/v141 Debug/Release V10C 聚焦回归均为 21/21；最终 Release 世界完整回归为 741/741。
- ResourcePackSmoke Debug/Release 为 65/65，覆盖有效接口和无效覆盖；Release 启动负例 14/14，
  包含 `invalid-v10c-atmosphere-interface`。
- resource manifest 为 67 项，缺项/陈旧项两个负例通过。真实 Release GL3Plus 的 V10C 与强制
  FS2 路径均编译、捕获并以空 stderr 退出。
- 规模玩法 Q1 与 V10B3 达到完全相同的 361 loaded chunks、1833 sections、1292 GPU sections，
  所有几何和驻留值逐项相同；frame P95/P99 为 -1.404%/+0.766%，mesh avg/max 为
  +0.282%/+6.757%，全部在 10% 内。
- 快速流送 Q1 的 frame P95/P99 为 +4.645%/+4.340%，mesh 与几何/驻留指标也在 10% 内；但
  `chunk_visible_p95/p99` 从 40.584 ms 到 49.636 ms（+22.304%）。复测为 50.873 ms，强制
  FS2 回退为 53.999 ms，说明该单样本信号不由 V10C 云 shader 引起；绝对预算上限为 1000 ms。
  按 Stage 10 纪律仍保留 `REVIEW_REQUIRED`，未获得项目所有者批准前不把整批改为 `Done`。

性能原始 summary、冻结/增量比较与上一批逐项比较位于
`docs/baselines/stage10-v10c-windows-hidden-v1/`。

## 开发者视觉检查

所有图片来自同一 Release 客户端，SHA-256 为
`C079622C8684678AF4767B684B7EBC267442BB6A5280B6DDC3472ABDC42BA4B2`；GTX 1050 Ti、
OpenGL 4.6、1280x720、windowed、FOV 105、隐藏 `RuntimeReadback`，没有显示或激活桌面窗口。
审阅者逐张按原尺寸检查：

| 画面 | 固定身份 | 视觉结论 |
| --- | --- | --- |
| forest noon 08/14/20s | seed 296595，pos `2766 102 2905`，rot `-15 118.4 0`，time 6000 | 云团连续平移；轮廓和世界关系稳定，无跳变、闪烁或相机粘连。 |
| dusk sunward/backlit | seed 20260807，pos `928 105 0`，rot yaw `270/90`，time 12000 | 顺太阳方向有暖色地平雾和日轮，反方向保持冷灰紫；地形、水与天空方向一致。 |
| forest night | seed 296595，同森林机位，time 18000 | 星、暗云和地形均可辨，没有白云、黑带或曝光断层。 |
| coast noon | seed 0，pos `285 72 40`，rot `5 270 0`，time 6000 | 水面、天空和远景雾连续，没有硬地平线、颜色断带或云层穿水。 |
| cloud below/inside/above | below 使用 forest noon；inside/above 为 seed 20260807、x/z `1312/0`、初始 y `168/210` | 下方见云底薄层，层内出现受控雾化，顶部能看到云层位于地形上方；边界没有翻转或整屏黑白。 |
| FS2 fallback | grassland dusk，强制 fallback | 旧式无限云形态实际恢复，证明回退路径不是日志假象。 |

纳入证据的 PNG SHA-256：

```text
validation-v10c-forest-noon-08s.png  60275DBD68E56B5A07AAECF88B0CEE01E4AB955BD516194A8B6A228F2E2B6B95
validation-v10c-forest-noon-14s.png  9D8BA4C66B868332F0E3C4C0EDAFED0EA4F979104F322DAA62B2E2D50FAE9810
validation-v10c-forest-noon-20s.png  82277C29918A90B38C71B82DB8A6344500D5347B317226FED33078B619156D79
validation-v10c-dusk-sunward.png     FAD619BD4D12EDA2618488465FBFE5DB3C813C846B9BCA9E87FB824713BA2120
validation-v10c-dusk-backlit.png     83F3E0ABDA00D42687E2D431C08956DBC09D46356890C0851E48AF2813AE0F3E
validation-v10c-forest-night.png     68FFBE03255BA4F29717D8874B04558E24E50EEC34B69AE6124C3BFE2CAFA029
validation-v10c-coast-noon.png       258A402D0856E5C1EB9E962CAD48642999DAD53A7B9143FB0A26178F379E9A71
validation-v10c-cloud-inside.png      A8048A595FC50BDE47E6EA72C2C5E984A5BB6944CB7B5C82342E3F8B4735FB78
validation-v10c-cloud-above.png       4A09949EF3E6D3B6E8B2A4DFE4F3084D72C6D87E3D5DEC4557959A55F7AEC602
validation-v10c-fs2-fallback.png      D574EE211B2C54382E9C39B996426B1DE9AA3EA226AE336B3AE82F4D182E55F5
```

结论为 `PASS`：V10C 的运动、方向、昼昏夜、水面共享雾和高度边界均由真实多帧画面覆盖；青色
小块与敌人警告/受伤状态一致，是既有敌人投射物，不是 shader 泄漏。截图上的离屏 FPS 只作
诊断，性能结论以 Q1 summary 为准。
