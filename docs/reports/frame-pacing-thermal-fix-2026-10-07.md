# 帧率节奏与发热修复：2026-10-07

本批应用级节流工程已完成：减少无上限渲染的持续负载；普通OS操作与长期降温效果仍未验证。
实现基于整合树 `e4e6b006ac070345469e823475ce92e4c1722bb3`，由主任务集中交付；未修改主工作区用户文档或原热分析调查记录。

## 实现与语义

- 前台游戏上限60 FPS；菜单／暂停／HUD指针界面30；失焦20；inactive／最小化10；显式hidden诊断60。
- 每次最多sleep 8 ms，随后pump窗口事件并重算剩余deadline；超时不补赶，不在已耗尽帧预算的VSync后再等待。
- 模拟使用包含等待的实际帧间delta，保持20 Hz固定tick、有界追赶、暂停冻结且恢复不补跑，以及World继续后台运行的既有语义。
- `frameMs`改为真实start-to-start周期，包含pacing；update／render仍单列工作耗时。未增加设置持久化或UI。

## 必要验证

| 项目 | 实际结果与证据 |
| --- | --- |
| arm64客户端 | Release／Debug正常增量构建均exit0；`build/thermal-frame-pacing-20261007/{Release,Debug}-client-build.log`。 |
| pure pacing | Debug／Release各46 checks／0 failures／PASS；`build/frame-pacing-20261007222605-Debug/test.log`、`build/frame-pacing-20261007222606-Release/test.log`。 |
| 既有AL-A3聚焦 | 16 checks／0 failures／PASS，固定tick与暂停恢复；`build/thermal-frame-pacing-20261007/fixed-tick-pause.log`。 |
| 轻量源码review | 未发现问题；未重复完整矩阵、全量包或长期压力。 |

两次隐藏原生对照均warmup 2秒＋sample 8秒，共享资源、同settings及seed42／RD3／1280×720点（2560×1440 framebuffer）／Medium阴影／Post Off／linear HDR／MSAA4，Planar inactive。
小文件证据：[comparison.json](../../build/thermal-frame-pacing-20261007/short-check/comparison.json)；两次child自然exit0、stderr0B，未复制完整app，临时world／config已删除。

| 隐藏短检查 | 修改前 | 修改后 |
| --- | --- | --- |
| 实际帧数／8秒 | 1679／209.875 FPS | 464／58.000 FPS |
| `avg_fps` | 211.106（旧工作耗时口径） | 57.982（真实周期，包含pacing） |
| simulation ticks／Hz | 160／20.000 | 160／20.000 |
| ps最后5次CPU | 58.6–63.3%，均值61.02% | 21.3–26.4%，均值23.88% |

二进制SHA-256：baseline Release `5f849fc4851b115d8860028e6c43a61392cc3f58d6f28a9fc05e577b5e6870c6`；new Release `62140cad6a99ff12029be628e54486cfc79b022d63558ac13bb3dc6ae9fee1ec`；new Debug `f001504a09b42a8a344adafb85a07c2bdc0d1e7e8ee263ef7292c6e9c41021cb`。

## 范围与后置

短检查证明隐藏循环的帧率上限和持续CPU占用下降；ps中100%约为一个core，启动峰值仍存在。这不是温度／瓦数测量，也不是严格整机功耗对比或前台普通OS验收。
真正前台、失焦／最小化／恢复及长期降温仍`NOT_RUN`，按用户授权后置、不阻断本批交付，不记PASS。Retina／抗锯齿画质设置和生成loader预算未改；不声称彻底消除所有发热。
