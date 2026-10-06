# 单水位真实平面倒影 v1

本合同落实 2026-10-06 完整参考画质 Goal 的 D 批次。它不改变世界、存档、流体、碰撞、水深快照或 streaming 需求。

`PlanarWaterReflection` 只观察 Ogre 当前已有的场景居民。调用方从真实驻留水面选择一个水位，传入动画水面的均值世界高度；当前 WaterVertex 将方块顶面下降 0.10 m，因此均值是实际顶面减 0.10 m。组件没有 World、生成器或 chunk 请求入口，不固定样板水位。其它水位仍用原天空近似。

组件只有一台镜像相机、一张无 MSAA、无硬件 gamma 的 RGBA16F 颜色纹理，以及独立拥有的深度／stencil 缓冲。颜色目标按主 viewport 的实际 framebuffer 宽高向上取整减半，超过 1920×1080 像素的绝对预算或零尺寸时不分配并回退。纹理为固定尺寸且 `autoUpdated=false`，预算检查先于分配；resize 释放旧颜色及深度，不将私有深度注册进 Ogre 的长期共享池。浮点探测直接使用这一张目标及其实际深度 attachment，要求真实 FBO 完整且读回 0.18、2、8、0.5；格式降级、能力／目标创建失败回退。shader、材质或 binder 错误继续报错。

镜像面取选定均值，custom near clip 保留均值以上 0.03 m 的半空间；复制主相机的位姿、FOV、aspect、near/far 和 frustum offset。采样矩阵遵循实际 `AutoParamDataSource` 的 RTT 方向翻转。水下／过水面的 0.15 m collar、非透视／自定义矩阵、非有限 oblique 矩阵停止真实反射。无效投影、behind-camera、UV／半像素 guard 与边缘平滑回退不读取无效坐标；波纹偏移每轴最多 0.012 UV。

只在手动反射 update 内安装作用域 renderable listener，并恢复原 listener。按 Water fragment 接口及材质排除所有水面；按实际材质排除选取／裂纹／碎屑，按 overlay queue 排除 HUD。水和玻璃同属 queue8，不能排除整个队列。当前第一人称手臂由主窗口 ImGui 绘制，反射目标不挂主窗口 UI listener，且 overlays=false；正常第三人称人物仍是场景居民。颜色 pass 用私有材质／GpuProgramParameters 副本，最多 96 个材质、384 个 pass；每次更新从当帧主 pass 深拷贝，再由独立 view binder 设置反射相机的空气雾、局部光和材质参数。镜像 camera、自动矩阵和 linearHdrMode 属于该 view，不临时改变主材质或全局雾、灯、阴影开关。

反射 viewport 保持该场景的正常阴影能力，嵌套 shadow camera 继续使用引擎 caster pass，不替换成反射颜色 pass。主窗口随后按它自己的 camera 正常重绘阴影。附加阴影统计来自实际 shadow target 完成回调及其实际 batch 数，不能把 Ogre 按灯数发出的 `shadowTexturesUpdated` 参数当成实际 cascade／update 数。

每个主 frameSerial 最多更新一次；默认每帧更新，居民上传、相机、光照、编辑与选定水位的变化直接进入当帧真实 draw，不保存静态图缓存。禁止递归手动 update，目标不自动更新且水面不入反射，形成双重防护。混合发生在水体自身照明之后，以 Fresnel、原 ≤8 m 驻留水深和有界波纹采样权重处理；反射纹理已经是线性受光 radiance，不再次 decode、clamp 到 1 或 tone-map，原透明 alpha 和水介质语义保持。关闭、legacy、能力不足及其它水位完整保留原近似。

接线顺序：初始化 Ogre 后 initialize；主 resident visuals、camera 与 environment 同步后 selectPlaneY/clearSelection → render → bindWaterPass，再绘制主窗口。FrameInput 包含实际 HDR 状态、关闭档、权威相机介质、frameSerial／sceneRevision、背景 authored 色和独立 view binder。world 卸载／切换前 resetWorld；组件必须先于 SceneManager／RenderSystem 销毁。resetWorld 释放颜色／深度、材质和 binder 引用，保留最多一台复用镜像相机。

统计区分累积 updateCount、当前颜色 batch／triangle、实际附加 shadow update／batch、目标格式逻辑字节（含 depth/stencil）、私有材质／pass 与 CPU 提交 wall time；后者不是 GPU timer。

`captureDiagnostic` 仅接受隐藏 RenderCapture 且非 perf 路径，最多四次、拒绝覆盖已有证据；使用实际 native RTT 的 GL FBO 读取 RGBA float32 原始值，同时保存明确标注 clamp＋sRGB 编码且无 tone map 的 PNG 预览，以及镜像相机、采样矩阵、纹理绑定和 GL 错误事实。原始文件保留 GL bottom-left 行序及宿主 float32 字节序。readback 恢复 framebuffer／pack／PBO 状态，不修改世界或主相机，不进入普通／性能路径。

## 当前验证边界

生产 budget／exclusion 函数的独立 policy test 19 项通过；实际 Water.frag 在 offscreen CGL 的 RGBA16F fixture 45 项通过，覆盖 >1 线性反射、透明 alpha、depth 1/3/8、其它水位／underwater／crossing／legacy、无效投影和 UV guard、有界波纹。早 clamp、double decode 与多水位采样故障均被拒。原件在 `build/reference-visual-implementation/planar-water/gpu-r1/`，工具明确标记 native Ogre RTT／正常游戏／resident streaming 为 NOT_RUN。

早期 Release native 隐藏客户端已对实际 1280×720 RGBA16F RTT 读回：GL error=0、非有限成分=0，目标包含真实驻留屋墙／窗／桥；主 Water 纹理绑定一致。同包、同存档／机位／时间／画质 on／off 对照中水域平均变化 3.776/255，静态地面逐字节相同，证明生产主水面实际采样和混合。原图、raw、事实与分析在 `build/reference-visual-implementation/integration-r1/water-hdr-r2/` 和 `build/reference-visual-implementation/planar-water/native-r3-{on,off}-matched/`、`native-r3-analysis.json`。旧窄渠／高岸墙和当前混合使可辨建筑倒影仍未通过视觉项；斜视诊断也保留，不能将 RTT 内容或像素差异当作正常倒影视觉验收。

这些结果仅验证对应的 CPU policy 与生产 fragment 行为。实际镜像／clip、建筑倒影、native Ogre framebuffer、双配置客户端、正常转头／接近／出入水、拆墙／增灯／填岸、卸载返回及 resize／世界切换资源循环仍须由完整 Goal 的协调构建和唯一正常客户端操作者验证。工程 API 可用、shader fixture 或诊断截图不替代这些项目。

## 2026-10-06 水岸整合增量

`scene-v2-native-r4` 与旧／新资产×legacy／HDR四组合的原生原图已经看见两栋建筑、桥、灯及云的镜像；这是当前完整样板实景覆盖，早期窄渠不可辨的失败保留。普通转头、编辑后更新与A生命周期仍 NOT_RUN，不能将隐藏固定／稀疏镜头当普通输入通过。

近岸剩余细线／三角纹经六组真实隐藏shader ablation定位到Water完整侧面和partial StoneStep竖面的共面。constant RGBA、关波纹／细节／倒影仍有；删全部竖面减纹但露暗缝，只作为归因，未作为生产方案。证据 `shore-filter-native-ablation-r2/ablation-review.json`。

新生产侧面按邻不透明compound的8×8固定边界覆盖裁剪、保留原三角属性与真实空隙，语义及预算见[套件合同](reference-architectural-kit-contract-v1.md)。Water沿用44B顶点和消费者独立uv3；可选 `waterBoundaryPinsV1` 与active float uv3由Ogre实际编译对象的临时完整GL链接证书确认，发生在World／worker创建前。缺接口的合法完整旧VS使用旧mesh路径，不新增mandatory资源token、不写save、不热切现有World。接口证书不证明任意第三方自定义shader遵守语义；bad compile／link继续明确失败。

当前认证路径对raw shore>0接触角抑制Ywave但保留非pin −.10；真实新cut pin固定实Y。四侧top接触角rawshore≥.25，独立topface同值，固定top=.90大于最高eighth内切.875；开放水shore0与guard关闭保留旧波动。侧面法线仍为原有近似。新完整VS+FS CGL560／16：504语义／几何检查0失败，另56严格TFfloat32 bit压力比较有16失败（最大9.54e−7）如实保留，不能称整工具PASS。旧高cut反例失败已修；新源当前原生效果及旧VS路径须在重建客户端复验。

HDR岸纹sin^12另有有界像素相位面积过滤，无新纹理／history／RTT。完整frag114／7的7失败为额外RGBA32F bit压力，实际RGBA8／16F各7组相同及独立面积oracle通过；不将该信号改善扩称实景共面缺陷已解决。
