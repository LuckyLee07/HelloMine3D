# BETA-RC 工程封板报告（2026-08-26）

> 历史报告说明：以下内容保留 2026-08-26 封板时的真实状态；原人工延期项的当前分类见
> `docs/ai-assisted-gameplay-acceptance-v1.md`，不在本报告内改写历史结论。

## 结论

BETA-RC 工程验收通过。N7A-N12C 的冻结代码已完成全门禁、正式 Q1、正式 Q3、崩溃/符号、
许可证与干净发行包复核；没有新增玩法或放宽性能预算。R3 只完成了部分真人自测，其余按用户
决定延期，因此本报告不把 R3 标为 `PASS`，也不创建本地 Beta 标签。

## 冻结身份

- 运行时代码：`9b323b2378f063ee81e4eee1a1dc37bc97a74aa4`（N12C）。
- 工具链：VS2017/v141，Windows x64 Debug/Release。
- 保存/地形/设置：save v11、terrain v3、settings v4。
- 资源：64 项 manifest、352 个 `en-US`/`zh-CN` 对齐文本 key、38 项资源包检查。

## 自动门禁与发行证据

- Debug/Release 全量重建与十三个测试目标通过；世界运行时 `699/699`，资源包 `38/38`。
- 隐藏校验客户端及真实三帧 WaveOut 流式音乐客户端退出码均为 0。
- 受控 dump 为 131,321 字节。
- 符号 ZIP SHA-256：`8283f062ff83cc6f050e92f5ff3b69aa48553307071888ba2d94867bad33ec87`。
- 84 文件发行 ZIP SHA-256：`DABE356BC511A110838DC1CD98BEE9392D0FC03FFBA1D40DB8271B6BD5392E2A`。

## Q1 正式比较

正式采集器的保存身份从过期的 v8 修正为当前 v11。冷启动、进世界、保存事务、备份恢复、
快速流送和规模玩法六个场景均以同一冻结提交采集 baseline/repeat，全部比较 `PASS`。

| 场景 | baseline | repeat | 关键结果 |
| ---- | -------- | ------ | -------- |
| 冷启动/进世界 | 可用菜单 778.017 ms | 671.091 ms | 首次可控 389.302/373.373 ms；帧 P95 3.083/3.053 ms |
| 保存事务 | 140.824 ms | 140.407 ms | `PASS` |
| 备份恢复 | 51.111 ms | 63.087 ms | `PASS` |
| 快速流送 | 帧 P95 7.814 ms | 7.833 ms | 区块可见 P95 40.285/41.783 ms；20 fixed tick/s |
| 规模玩法 | 帧 P95 6.830 ms | 6.987 ms | 帧 P99 9.238/9.240 ms；20 fixed tick/s |

跟踪摘要位于 `docs/baselines/release-candidate-windows-hidden-v1/`。

## Q3 正式长稳

Release x64、seed `20260820`、schedule v2 的 nominal/stress 两档串行运行，各完成 1800 秒、
36,000 fixed ticks 和 360 个进程采样，子进程退出码为 0，所有世界不变量与包装层预算通过。

| Profile | 移动/编辑/Actor/保存 | 峰值 private | 稳态增长 | 峰值句柄/线程 |
| ------- | -------------------- | ------------ | -------- | ------------- |
| nominal | 361/1,800/900/179 | 19,623,936 | 2,236,416 | 251/4 |
| stress | 901/7,200/1,800/359 | 26,513,408 | 6,799,360 | 252/4 |

## R3 边界

自动预检覆盖 699 项世界逻辑、25 项输入/交互逻辑、后台焦点守卫和人工记录 schema，结果为
`automated_result=PASS`。证据同时明确记录 `physical_input_result=NOT_RUN`、
`r3_closure=NOT_ELIGIBLE`。用户已完成部分非正式自测，其余真人键鼠、窗口焦点、双语可读性
和听感继续延期；D2、D4、D6、R3 保持 `Verify`。

## 封板边界

封板提交仅包含性能采集器的 save-v11 身份修正、重新采集的跟踪摘要和文档，不改变运行时
代码。提交后生成覆盖 `origin/master..HEAD` 的本地 bundle，并执行 `git bundle verify`；bundle
作为忽略产物留在 `bin/`，不进入提交。没有 push，也没有创建 Beta 标签。
