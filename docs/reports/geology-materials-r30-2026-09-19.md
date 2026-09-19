# r30：岩层层理与沙面细纹

r28 已将沙丘、岩台和浅水湿地接入新世界，r29 补上湿地草丛轮廓。本批继续处理地形的视觉
辨识：岩壁不再整片共用同一种灰色，沙面增加轻微弯曲、强弱不同的风纹。

当前状态：生产 GPU、双配置资源检查、限定视觉、24 次性能对照及逐帧复核通过，干净候选已交付。综合视觉 Goal 继续。
原始证据根为 `build/visual-upgrade-20260917/geology-materials-r30/`，下文路径相对此目录。

## 实现与范围

- Stone 表面依据连续世界高度和缓慢横向扰动呈现冷灰/暖灰矿物层；顶部变化较轻，避免平台
  地面出现与侧壁一样强的条带。Sand 使用具有局部曲率、疏密和强度变化的方向性细纹。
- 细节按屏幕像素覆盖衰减，远处回到稳定的宽色域。图案不依赖时间，普通与阴影 fragment
  使用同一算法；不新增顶点属性、纹理采样、绘制批次或世界状态。
- 只改两个 terrain fragment。世界 C++、地形生成 v13、world save v12、碰撞、命中、掉落
  均保持。所有世界 Stone/Sand 都会采用该着色，包括玩家放置的块和旧版本世界；这不是
  仅作用于天然岩台的分类，也没有添加新材料 ID 或更多生态。
- 矿石、圆石、炉子、草地与树皮沿用原路径。兼容图集仍叠加地质着色；关闭 surface lighting
  返回旧着色，两种回退分别验证。

## 版本身份

最终资源为 R2，基于 `f53bfb4b2f53e0dee9f059704b267ffd329e41d4`。复用 r29 已构建的
macOS Release x86_64 客户端：332 份运行源码与根目录、隔离检出及 r29 冻结包逐字节一致。
本批没有重新编译客户端，不把资源检查构建记作客户端构建。

| 项目 | SHA-256 |
| --- | --- |
| Release x86_64 | `7fe9df4b4df6d2251715be36bf8a25c3d92455c566c5e89599722205df815136` |
| 332 份源码清单 | `6cfd24b4f7652ff5c11d3f8d4acd9cfa784e200818f12af84917806835be0ec6` |
| R2 tracked diff | `cde4df7a1873a58cc31a87f6b14920d12c854db39cb2e3194dbe3ec70c2ad5b3` |
| 普通 fragment | `ca4e10842c1d3fe7e55c70ffa983ba0fe3bd26c90b32750b6ed7131b05f868e1` |
| 阴影 fragment | `f344139d098334ec6a331c8e352ed599c9b99d278a249a9e6f0e209ecacc8878` |

资源清单本身列出的路径未变，不能用清单文件哈希证明 shader 内容未变。实际资源内容由
`frozen-candidate-r2/Contents/Resources/distribution-sha256.txt` 及性能冻结记录中的两份
shader 内容哈希约束。正式诊断只复用 `../round2-compact-layout-recheck-r3/Diagnostic.app`，
隐藏/noActivate，不另行启动冻结副本。

## 工程验证

| 检查 | 结果与范围 |
| --- | --- |
| 资源检查 Debug / Release | 各 114/114。R1 重新构建对应 ResourcePackSmoke，最终 R2 资源再次运行同一已核对的两个测试程序，记录 `resources-r2/`。 |
| 实际生产 GPU | 最终 `gpu-r2.log` 为 148/148：原 96 项表面反馈/植被/材质回归及新增 52 项地质检查。atlas/array、普通/阴影、夜间、时间不变、区块原点分解、正负接缝与 Alpha 均通过。 |
| 范围与关闭回退 | 关闭 surface lighting 与旧 shader 精确相同；煤、铁、圆石、炉子、生态草及树皮样本逐像素不变。 |
| 远处过滤与负例 | 使用原生产 GLSL，负例只移除两处像素覆盖衰减。array 岩石远处相邻像素平均梯度 0.0817，对应负例 5.9687；沙面 0.0495，对应负例 4.1900。旧版常量色面不能满足新增空间变化条件，故障均被检出。数值只说明该 GPU 夹具的过滤作用。 |
| 世界与存档回归 | 运行 C++ 没有改变，本批没有重复全量 WorldRuntime；r29/r28 历史证据按原版本保留。 |

GPU 入口编译与运行方式（输出目录必须不存在）：

```bash
clang++ -std=c++17 -Wno-deprecated-declarations \
  tools/validate_block_feedback_shader_macos.cpp \
  -framework OpenGL -framework CoreGraphics -framework ImageIO -framework CoreFoundation \
  -o /tmp/hellomine-geology-gpu
/tmp/hellomine-geology-gpu "$PWD" /tmp/hellomine-geology-output \
  "$PWD/build/visual-upgrade-20260917/wetland-grass-r29/frozen-candidate-r2/Contents/Resources"
```

## 画面复查与修正

首版沙纹过于平直、强烈，有犁沟感。保留 `pilot-r1/review.json` 的修订结论与 R1 资源；
R2 降低明暗幅度并加入局部曲率和强弱变化，经原机位补拍后再冻结，未覆盖首轮证据。

最终三 seed（0、1、42）岩台/沙丘，加 seed 42 高档阴影、夜间、兼容图集和岩台顶部，共
13 组同机位前后对照、52 张原图；实际审阅每次 10 秒原图，共 26 张。R1 试拍中身份相符的
两个旧版场景及 R2 沙面补拍复用，映射在 `visual-r1/references.json`，其余重新拍摄。
岩壁能辨认跨面的矿物层，顶部保持较低对比度；沙纹较柔和，台阶与仙人掌仍清楚。夜间沙地
两版本均有真实敌人遮挡中央近景，比较使用仍可见的侧面沙台，未将敌人遮挡区域当成空地。

岩壁 High、沙面 Off 各保存 33 帧镜头扫动，125 ms 间隔，实际像素 1280×960；每段查看
4000、4125、5000、5125、8000 ms 五帧。抽样中纹理随地表保持，没有发现骤然反色/跳位。
序列带 HUD fixture，字幕仅在诊断配置关闭；BMP 原件保留，PNG 只是无损格式转换。
最终共保留 118 张原始帧，实际审阅 36 张，结论为 `PASS_SCOPED_GEOLOGY_MATERIAL_VISUALS`，
记录于 `visual-verification-final.json`。8 fps 抽样不能证明全部高频闪烁消失，也不等同正常
持续键鼠操作；后者继续按用户要求暂缓。

## 性能与交付

性能输入、两版资源、两份 shader、运行源码、视觉判断、原图及脚本共 505 项已冻结。
采用岩壁 High 与沙地 Off 两个代表场景，各静止/快速流送、三组 AB/BA/AB 配对，共 24 次；
每次 5 秒预热、30 秒采样，实际 2560×1440、无截图读回，P95/P99 中位数比值上限仍为 1.10。
这不是所有材质和阴影档位的全组合矩阵。原 runner session 88498 已退出 0；结果如下。

| 场景 | P95 旧→新（ms） | 比值 | P99 旧→新（ms） | 比值 |
| --- | --- | --- | --- | --- |
| rock-high-steady | 9.679 → 9.909 | 1.0238 | 10.959 → 10.965 | 1.0005 |
| rock-high-streaming | 11.507 → 11.622 | 1.0100 | 13.784 → 14.220 | 1.0316 |
| sand-off-steady | 6.941 → 6.738 | 0.9708 | 8.666 → 8.170 | 0.9428 |
| sand-off-streaming | 9.028 → 8.714 | 0.9652 | 12.037 → 11.836 | 0.9833 |

四组均通过。`performance-r1/audit-r1.json` 从 100045 行原始帧数据重新核验时间、模拟 tick、
帧耗时合计、分辨率、无读回、世界身份、Normal 难度、驻留字节和时序；错误地形版本与读回
两个故障负例被检出。测试使用临时诊断世界，不修改正常存档。该审计由实施者复算，不称独立
人类验收；每组基于三次运行的中位数，不能据此声称普遍提速或所有设备表现相同。
24 次采样均无生命重置。静止场景驻留字节中位数一致，流送场景末帧面数有小幅差异，
原始值完整保留；不声称两版每一帧的实际工作量完全相同。

干净交付位于 `../delivery-r30/HelloMine3D-Visual-Candidate.app`，包含 111 项分发文件、
332 份运行源码身份。全部媒体资源与实测冻结包一致，两份 shader 逐字节核对；
客户端仅执行 validate-only，成功且未创建窗口，不含用户存档。`../delivery-r30/COMPARISON.md`
提供四组、八张本地原图，`delivery-verification.json` 保存交付身份。

结束时唯一工作客户端已关闭，用户配置原样恢复，八个原世界及 25 份元数据哈希未变，见
`final-state-r1.json`。正常持续移动、完整战斗和独立整体体验仍未验收，不将本批诊断标为
综合 Goal 完成。后续继续优先依据真实场景完善地形与生态辨识，已通过项不重复全量运行。
