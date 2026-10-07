# 参考画质合入 master：2026-10-07

状态：冲突处理与必要验证已完成；本记录随本地合并提交交付，最终合并身份以Git两父提交及master HEAD为准。

整合输入：master `6bf03c2b90537b106aa2873a2cada486e72106e7` 与参考分支 `codex/reference-visual-v1` 的 `e32e57dfbefb2b4b9929bd64a927eae110ec348e`。
执行位置：`/Users/lizi/.codex/worktrees/reference-visual-merge/HelloMine3D`；主工作区用户改动由主任务保护，恢复在合并提交后执行；参考源分支未改写。

## 冲突处理

共10个冲突，保留双方适用实现与真实状态，未通过删除检查或放宽门槛换取通过：

- 纹理3项：`DefaultPack.png`、`WarmWilderness64.hmt`、`warm-wilderness-v2/pixel-revision/array-build.json`，整合主干材质修订与参考资源／导出链。
- 运行与测试2项：`OgreBootstrap.cpp`、`WorldRuntimeSmokeMain.cpp`，保留主干功能与参考HDR、套件、倒影和生命周期接线及双方用例。
- 水面1项：`HelloMine3DWater.vert`，保留主干相机相对共享边投影；参考cut pin保持真实边界高度，其余保持原波动与−0.10表面偏移。
- 工具2项：`verify_xcode.sh`保留本机架构及严格完整世界摘要；`capture_visual_macos.py`保留BMP／连续水面与参考场景／倒影诊断，专用夹具隔离，预览不混入定时帧计数。
- 账本2项：`todolist.md`、`validation-matrix.md`，保留视觉精修开发阶段收口及参考Goal按用户授权完成的阶段结论。

## 当前最小验证

| 项目 | 结果 | 范围／证据 |
| --- | --- | --- |
| 纹理源、atlas／array与资源清单 | PASS | 当前整合资源的定向验证；不冒称普通动态画质验收。 |
| 水面GPU | PASS | `build/reference-visual-merge/water-shader/sampler-complete-r2/run.json`：Apple M1 Pro，exit0，stderr0B；16冻结clip、640相机相对对、840原边界对及24 pin／24 guard-off／12普通岸角通过。 |
| Release客户端 | PASS | 当前整合源码的目标构建。 |
| Release世界测试目标 | PASS | 目标构建；不等同完整世界测试运行。 |
| Release Cocoa输入 | PASS | 当前配置的定向非可见输入回归；不冒称普通玩法输入。 |
| Debug客户端 | PASS | exit0；binary SHA-256 `36e950d5fd64f5f8a734e65e9700dca594a9ef492093994c3ed0092e1fdeb248`。 |
| Release完整世界smoke | PASS | 自然exit0，4553 checks／0 failures／status PASS；4553严格摘要实际PASS，自测19／19。 |
| 隐藏HDR启动 | PASS（启动绑定）／FAIL（既有显示） | retina-r2 helper／child exit0、stderr0B、实际1920×1280；linear-hdr／fallback0、colour／depth／stencil samples4、Planar active1／GL0／nonfinite0。HDR＋Post On下HUD正立但3D世界上下倒置，属明确产品显示FAIL待修。 |
| 同包Post Off健康检查 | PASS（启动健康／图像方向） | 同包、同scene及参数仅改Post Off；`native-startup-post-off` helper exit0，5秒原图已实际查看，3D／HUD正立。不宣称完整画质或普通玩法PASS。 |

水面首轮GPU自然exit0，但旧夹具未为新增倒影sampler提供完整纹理，driver警告原件保留在`water-shader/`。只修现有夹具的关闭态完整2D绑定后必要复验；该复验未追加生产shader改动，也未运行旧独立32F压力全套。
本轮只选择受合并影响的必要检查；未启动普通游戏验收窗口或扩大完整矩阵。
旧4491摘要reader拒绝4553的结果保留：4491＋参考视角保护2＋主干设置语言32＋工位视线28；按真实运行更新精确计数，未放宽门槛。隐藏启动首轮helper exit1是pixel-ratio1期待960×640而Retina实际1920×1280；包已启动，仅改倍率2在新目录必要复验，首轮原件保留。

## 后置与限制

普通连续行走、全12套件与四向建造／挖掘拾取、编辑后光照／倒影及保存重开专项继续为`DEFERRED_BY_USER`，原BLOCKED／NOT_RUN及有限PASS按原版本保持。
历史TF32F／RGBA32F补充逐bit压力仍FAIL，不改写整工具PASS；正常RGBA8／16F已有通过结果仅按未变域引用。原六参考图及完整动态画质缺项保留，不伪造普通玩法、整体视觉或人类体验PASS。
倒置非合并引入：参考原`r5-hdr-post-on/capture_04000ms`及`final-hdr-post-on/capture_07000ms`同样倒置，最新r16街景Post Off正立；HdrPipeline／相机／场景配置与整合树无diff，HDR／Post初始化无相关差异。同包Post Off启动健康与3D／HUD方向已通过；HDR＋Post On既有显示FAIL保留后续修复，本次不扩展shader范围。
