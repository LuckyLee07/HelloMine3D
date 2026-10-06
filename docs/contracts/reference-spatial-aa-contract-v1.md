# 参考画质抗锯齿合同 v1

2026-10-06。范围：完整 Goal 批次 E；保持世界、设置 v12 和旧 post Off/On 语义。普通 linear-HDR 优先采用实际四采样场景路径，legacy 主窗口仍为零采样。此选择依据当前工程和独立面积校准，真实动态画质与性能仍由当前 native 客户端分别验收。

HDR 首个 compositor 的 input previous 继承主窗口 FSAA，四采样线性 RGBA16F 场景经 Ogre 原生 blit 解算到单采样 RGBA16F，再由完整 HDR resolve 固定曝光、tone map 和 sRGB 编码。MSAA4 实际 active 时 spatialAaStrength=0，不叠加空间过滤。最多四个场景采样，无历史、运动向量或逐帧增长资源；UI 在 compositor 后绘制。阴影和 PlanarWaterReflection 目标自身保持线性 HDR。

优先候选须先通过原生窗口格式、GL_MAX_SAMPLES 与真实窗口采样数检查。场景实际颜色、独立 owned 深度/模板和解算纹理均查询原生附件格式、尺寸及采样数，并在实际目标读回 .18/2/8/.5 验证线性 HDR 解算。只读 getFSAA 不构成实际存储通过。目标为固定尺寸，beforeFrame 先检查 8,294,400 物理像素上限，再释放旧目标并重建；私有深度不进入共享 resize 池。预算日志分别记四采样色/深度、单色解算与主窗口 back/depth 的格式逻辑 bytes 及增量，driver-owned front/present 物理 VRAM 明确未测。

四采样能力、原生附件或解算不可用时完整退回单采样 HDR；浮点 HDR 不可用或尺寸超限时沿原路径回 legacy，缩小后按预算恢复。错资产和实际 shader 编译错误先于强制/能力回退明确失败。旧资源缺 spatial AA uniform 时保留旧 resolve 并记 available=0，不要求新增空间过滤接口。

单采样 HDR 保留有界空间过滤：tone map/sRGB 后最多九次 sceneTexture 采样、方向最多四物理像素，不新增目标。中心 Alpha 保持，低对比原样返回，高对比窄/宽过滤受邻域亮度范围约束；负/零 inverseTextureSize 返回中心色，纹理 clamp 阻止越界。此路线用于四采样不可用时回退及显式比较。

普通 HDR 无 HELLOMINE3D_MSAA4 时优先尝试四采样；精确值 0 禁用候选，1 显式启用候选，均不得绕过能力/资产/预算门禁。HELLOMINE3D_SPATIAL_AA_OFF=1 仅禁单采样空间过滤。capture_visual_macos 每次显式写两个环境值 1/0，使 --msaa4 固定请求四采样，默认 capture 固定单采样空间过滤，--spatial-aa-off 固定单采样原 resolve；这些比较不随普通默认改变或继承 shell 的 AA 开关。实际回退由日志判定，requested 四采样不等于 active 四采样。

已保留的空间过滤 r2：97 例、719 检查，工程失败 0、静态面积 MAE 质量失败 12；其 aggregate 改善不能掩盖薄线个例退化。新真实几何 MSAA4 r2 使用相同 97 例和独立 64×64 面积 oracle，实际三角半平面/窄条覆盖、RGBA16F4+depth24stencil8、原生 blit、完整生产 resolve strength0：605 检查，工程失败 0/GL0；94 例改善、2 例不变、1 例失败。斜率 1、宽 1 像素、相位 .5 的细线 MAE .005115017028 对单采样 .004963502367（+3.05%）；四有限采样点不能精确积分任意相位窄条。15 例总 MAE .02855740184 对 .08180623035，97 例总 .2522805439 对 .8359477833。这些证据支持当前优先 MSAA4 的选择，所有真实失败保留、门槛不放宽，不声明全画质 PASS。

实际动态和成本仍需连续观察栏杆、叶片、瓦檐及水面，并测当前场景 P95/P99 与三路线成本；独立 CGL 工程和固定面积结果不替代 native Ogre 绑定、正常输入或动态视觉验收。
