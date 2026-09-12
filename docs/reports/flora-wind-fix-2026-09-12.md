# 花草固定节奏与抖动修复

2026-09-12，用户反馈花草摆动频率固定、观感生硬。本次局部修复及验证已完成。
起点为 `c34cd32` 加已有暖野 M1 工作区；未覆盖其他进行中的材质/叶簇/设置改动。

## 原因和改动

花草的两个 vertex program 都绑定 `globalTime time_0_x 1.0`。
Ogre 的 `getTime_0_X(x)` 实际返回 `fmod(time, x)`，即每秒归零；旧正弦角速度 1.8
本应约 3.49 秒才完成一圈，却每秒就跳回开头。同时根和顶端使用相同位移幅度，
整株会滑动；局部顶点坐标还让风的空间模式每个区块重复。

- `HelloMine3D.program`：仅将 FloraVertex / FloraShadowVertex 的时间改为连续 `time 1.0`。
- 两个花草 shader：世界坐标驱动的平滑风场，叠加缓慢阵风、相位漂移和不同频率的轻摆。
- 现有 Cross 形状的 repeat V 在根部为 1、顶部为 0，据此固定根部；物理高度进一步缩小幼苗摆幅。
- 普通和阴影接收路径使用一致形变。保留现有纹理/Alpha；不改 CPU 网格、顶点布局、方块、碰撞或存档。

这仍是轻量的茎叶弯曲近似，不是柔体模拟或逐株物理系统；没有逐帧随机数或 CPU 网格重建。

## 验证

macOS 15.7.3 / Apple M1 Pro。独立 GPU 探针为本轮编译的 arm64；游戏复用已冻结的
M1 pixel-final Release x86_64 客户端，只替换三个 shader/program 资源，不声称重新构建了游戏双配置。

| 项目 | 结果 |
| --- | --- |
| 旧版 GPU 复现 | FAIL（预期）：10 项中 8 项失败，每秒边界位移跳变 0.0772541 格、根部漂移 0.0666666 格 |
| 修复 GPU 回归 | PASS 10/10；8 秒 × 120 Hz，共 961 个时间采样 |
| 根部固定 | PASS，位移 0 |
| 秒边界连续性 | PASS，跨 0.0002 秒最大变化 0.0000195503 格 |
| 连续帧、幼苗比例、幅度边界、空间差异 | PASS；120 Hz 最大相邻位移变化 0.000814915 格 |
| 正负坐标/区块平移一致性 | PASS，最大浮点位置差 0.0000610352 格，小于 0.0002 门槛 |
| 阴影接收与普通路径 | PASS，GPU 位移差 0；High 实机启用且未回退 |
| ResourcePack / MeshDirty | PASS：现有测试二进制重跑 105/105 / 通过，身份单独记录 |
| 实机动态序列 | PASS：原版 Off、修复 Off、修复 High，各 80 张原始帧；生成两段 H.264 动态对照 |
| 定向静态性能 | PASS：P95/P99 比值 1.006 / 0.802，低于 1.10 |
| Windows / 独立正常玩法 | NOT_RUN；此次为定向视觉诊断 |

GPU 探针直接编译生产 GLSL，并读取 `.program` 的实际时间绑定，通过 transform feedback
读取顶点位置；没有在 CPU 复制风算法。首次旧版失败和修复成功日志均保留。

```sh
clang++ -std=c++17 -Wno-deprecated-declarations tools/validate_flora_shader_macos.cpp \
  -framework OpenGL -o build/flora-wind-20260912/validate-flora-shader
build/flora-wind-20260912/validate-flora-shader media/ogre
bin/HelloMine3DResourcePackSmoke
bin/HelloMine3DMeshDirtyTests
```

动态序列使用 `tools/capture_flora_wind_macos.py`，seed 20260807，初始位置 `(1056,106,928)`、
朝向 `(8,0,0)`、FOV70、RD8、时间 6000、standard profile、post Off，独立副本和存档。
目标 4–11.9 秒每 100ms 一帧，以 10 fps 编码成 8 秒视频；不是高帧率手感或真人审美验收。
首两次配置请求 960×540，但 Ogre 实际创建 1280×720 / Retina 2560×1440；后续工具明确请求
1280×720，实际窗口保持一致。完整配置、原始帧哈希和启动命令在各 capture.json 中。

[修复前视频](flora-wind-evidence-20260912/before-wind.mp4) ·
[修复后视频](flora-wind-evidence-20260912/after-wind.mp4)

无录屏性能对照采用同一场景、Off 阴影、5 秒预热和 30 秒采样，在采集前保存 10% 门槛。
P95：10.808 → 10.873 ms；P99：14.713 → 11.800 ms。
常驻 buffer：40411872 → 40412664 bytes（+0.002%）；顶点/索引 +21/+30，是运行中世界派生几何
的小幅差异，不能写成严格一致。最大帧 27.281 → 34.191 ms，保留该长帧；单对样本不证明稳定提速，
也不替代 M1 全部画质档或完整 Q1。原始 CSV 与汇总见[性能对照](flora-wind-evidence-20260912/performance-comparison.json)。

## 交付与保留记录

[HelloMine3D-Flora-Wind-Fixed.app](../../build/flora-wind-20260912/HelloMine3D-Flora-Wind-Fixed.app)
是 M1 pixel-final 的清单验证副本，仅替换两个 Flora vertex shader 和两个时间声明。
包有 110 项哈希记录，包含当前标准材质路径；旧交付包与用户存档保留。
可执行文件 SHA-256：`88a7165b3c053ead8633bf3e586460db9c7ad14d5662f9b7d0d9ebc05bf26dfc`。
包身份、测试身份与证据哈希见[证据索引](flora-wind-evidence-20260912/sha256.json)。

原始 240 帧和全部运行日志在 `build/flora-wind-20260912/`。
系统 ffmpeg 因缺少 libtiff.5 无法启动；没有修改系统依赖，改用 AVFoundation。
默认 CommandLineTools 的 Swift/SDK 不匹配，改用同一 Xcode bundle 的 Swift 6.2.3 和 SDK；
沙箱视频编码器不可用，桌面编码成功，Quick Look 解码预览通过。
编码工具是 `tools/encode_render_sequence_macos.swift`，编译/用法见文件头。
首次失败的空 MP4、编码日志和沙箱预览日志保留，不计为成功产物。

## 后续调整：提高连续摆动的可见性

2026-09-12 用户反馈连续摆动太慢。基于 `1f2800c`，将两个 Flora shader 的主/次/横向摆动
频率提高至原来的两倍，主周期从约 5.71 秒缩短为 2.86 秒；风弱时的强度从 0.025 提高到
0.055，峰值仍为 0.085。阵风和缓慢相位漂移相应加快，保留连续时间、根部固定和幼苗比例。
选中高亮复用生产 Flora shader，因此同步提速。

原有 GPU 护栏不变：风摆 10/10、选中表面 12/12、资源包 105/105 通过。
961 个时间采样中根部漂移和阴影接收路径差异均为 0，120 Hz 最大相邻位移为 0.00151825 格，
低于原 0.004 护栏。相同视角采集原参数 Off、新参数 Off/High，各 80 张 BMP；High 实际启用，
没有回退，最大采集延迟分别为 9/5/23 ms。检查了连续画面，生成 8 秒、10 fps 的本地视频，
macOS Quick Look 解码通过。`capture_flora_wind_macos.py --format bmp` 用于低开销采集，默认 PNG 保留。

此次是两个 shader 的常量调整，复用已验证方块反馈 Release 包的可执行文件，仅替换这两个资源，
重新记录全部包哈希。没有重跑 C++ 全门禁或扩展性能结论；原报告各轮数据仍对应原来的版本。
调参、日志和包身份见[证据索引](flora-wind-tuning-evidence-20260912/identity.json)。

[新版运行包](../../build/flora-wind-tuning-20260912/HelloMine3D-Livelier-Flora.app) ·
[加快后动态预览](../../build/flora-wind-tuning-20260912/faster-wind.mp4)。媒体仍仅保存在本地。
