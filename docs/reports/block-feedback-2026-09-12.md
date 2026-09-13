# 方块选中高亮与破坏表现

2026-09-12 用户要求：花草选中改为贴合模型的高亮，替换黄框及黄色线段裂纹，加入破坏粒子。
实现已进入正常 Sandbox/Ogre 路径，保留命中、硬度、工具、掉落和 save v12。
范围见[补充合同](../contracts/block-feedback-contract-v2.md)。

## 实现

- 用模型表面淡蓝白高亮替代 `OgreBlockOutline`。注册 Cross/Resource 模型、幼苗 metadata 高度、
  实际生态贴图、Alpha 和 Flora 连续风摆都复用现有规则；不再用包围立方体表示花草。
- 真实开采进度控制 0–9 阶段的表面分叉裂纹：从局部向外扩展，深色裂缝配窄碎边。
  停止、切换目标、菜单和切世界会取消对应表现；破坏提交后隐藏已消失的目标。
- 开采中从命中面发出少量碎屑，破坏/放置从实际方块位置发出碎屑，采样原方块材质局部纹理，
  随透视、重力、旋转和淡出变化。拾取继续使用 HUD 图标。
- 世界和 HUD 共用 48 个粒子上限，寿命不超过 0.55 秒；完整破坏 16 个、Reduced 减半、Off 不生成。
  开采每 0.12 秒最多一组 2 个，不追补卡顿期间的发射；轨迹由年龄解析计算。
- 新渲染器最多两个 draw call，选择网格仅在目标/metadata 改变时重建，不改地形顶点格式。
  标准数组纹理与兼容图集均有对应 shader。

## 验证结果

macOS 15.7.3 / Apple M1 Pro，客户端 Xcode x86_64，GPU 探针 arm64。

| 项目 | 结果与范围 |
| --- | --- |
| 客户端 Debug / Release | PASS；Debug 有既有第三方警告，新第一方代码无编译警告 |
| P11B 定向 | PASS 102/102，含世界坐标、实际方块 ID、轨迹帧率独立性、限流、取消、强度和跨世界清理 |
| 完整 WorldRuntime Release | PASS 1032/1032；增加 17 项本次行为/几何检查 |
| ResourcePack Release | PASS 105/105，保留资源负例 |
| 资源清单 / Xcode 工程图 | PASS 80 项 / 31 工程及 9 个自检案例 |
| GPU 生产 shader | PASS 12/12：两纹理路径编译链接、十阶段单调增加、取消恢复、近物遮挡、风摆花朵 Alpha 轮廓 |
| 干净包连续帧 | PASS：石头的局部开采→取消→完整破坏，花朵 High、草丛兼容画质及 Off 回退 |
| 幼苗/薄门模型 | 自动几何检查 PASS；作物实机采集期间继续生长，不把它作为固定幼苗高度证据；开门采集视角侧对平面，未形成可辨识视觉证据 |
| 定向性能 | 见下节；首组旧树冠基线不具可比性，保留记录 |
| Windows / 独立正常输入玩法 / 人类审美 | NOT_RUN / NOT_RUN / NOT_CLAIMED；不改写项目既有验收状态 |

实机采集是明确启用的开发者诊断：临时包、独立存档、固定 seed 和小型场景，定时按下/释放
通过真实 Sandbox 开采和命令路径执行。没有将脚本输入写成正常鼠标验收。
石头序列捕获到阶段 0–9、取消恢复、破坏时 21 个存活碎屑及随后消退；Off 序列各阶段为 0 粒子。
GPU 探针直接编译生产 GLSL 并使用实际数组/图集资产，不以 CPU 重写 shader 算法替代 GPU 结果。

首轮 PNG 连续采集因同步编码大尺寸帧造成严重时间偏移，未覆盖完整破坏，不计动态 PASS。
新增可选 `HELLO_RENDER_CAPTURE_FORMAT=bmp` 降低无损读回保存开销，并记录 target/actual 时间。
成功序列为 1000–6900 ms、每 100 ms 一帧，共 60 帧；视频为 2560×1440、10 fps、6 秒。
原始失败、初版 shader 和后续结果均保存在本地，不删失败换 PASS。

## 性能与交付

匹配基线为 `HelloMine3D-M1-Classic-Crown.app`：共同运行资源除本次修改的 manifest/program/material
声明外逐字节一致。固定 seed 20260807、位置 `(1056.5,106,928.5)`、朝向 `(40,0,0)`、RD8、
standard、阴影 Off、2560×1440；5 秒预热、30 秒采样，关闭读回。玩家按正常重力落至 y=85，
两边最终位置与世界状态相符。采集前记录 frame/render P95 不超过基线 1.10 倍的门槛。

| 指标 | 匹配基线 | 本次 | 比值 |
| --- | ---: | ---: | ---: |
| Frame P95 | 11.847 ms | 11.855 ms | 1.0007 |
| Render P95 | 9.793 ms | 9.800 ms | 1.0007 |
| Frame P99 | 12.751 ms | 12.161 ms | 0.9537 |
| 最大帧 | 21.962 ms | 27.899 ms | 1.2703 |
| 常驻地形 buffer | 35,228,360 bytes | 35,228,360 bytes | 1.0000 |

P95 门槛通过，常驻顶点 877,960、索引 1,783,410 均一致；最大帧仍如实保留。这是一对自然场景
选中表面的定向样本，不能据此宣称稳定提速，也不代表持续满粒子负载或完整 Q1。
原始 CSV 和汇总见 `build/block-feedback-20260912/performance-matched-{before,after}/`，
[机器可读对照](block-feedback-evidence-20260912/performance-matched-comparison.json)。

首组误用了旧树冠包，其常驻几何相差 13.4%；已标记 `INCOMPARABLE`，不计为性能 PASS。

[可运行 Release 包](../../build/block-feedback-20260912/HelloMine3D-Block-Feedback.app) ·
[石头开采视频](../../build/block-feedback-20260912/stone-mining.mp4) ·
[花朵高亮](../../build/block-feedback-20260912/flower-high/frames/capture_01000ms.png) ·
[兼容草丛高亮](../../build/block-feedback-20260912/grass-compatibility/frames/capture_01500ms.png)

交付包是实测 Preview2 的逐项哈希验证副本，共 109 项；无源码目录依赖，不含用户存档。
可执行文件 SHA-256：`f1a3621830709fb0efb22dda92d9f018f77ddc5f9b8ebf37c67ba68939190c3e`。
构建记录起点 HEAD `2a1b821` 加本次工作区改动；报告中另保存新增源文件哈希，补足包工具只记录
tracked diff 的边界。原有交付包与其他任务的提交保留。

全部原始帧、capture.json、平台参数、实际启动命令及测试日志位于
`build/block-feedback-20260912/`。媒体仅保存在本地，文字/哈希索引见
[证据索引](block-feedback-evidence-20260912/identity.json)。

复现入口：`tools/capture_block_feedback_macos.py --help`，
`tools/validate_block_feedback_shader_macos.cpp` 文件头的编译命令，
`tools/encode_render_sequence_macos.swift` 文件头的编码命令。
编码输入使用仅含 BMP 的 `stone-bmp/video-frames/`，避免将导出的 PNG 再计作一帧。
