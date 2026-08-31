# HelloMine3D PLAYABILITY-RC 工程封板报告

> 历史报告说明：以下结论冻结 2026-08-31 封板当时的事实，不回写为 AI 或真人 PASS。
> 同日后续确立的 Architecture Lab 验收模型见
> `docs/ai-assisted-gameplay-acceptance-v1.md`；原 `Verify/Deferred` 人工项已被重新分类为
> AI 场景 `NOT_RUN`、主观体验 `NOT_CLAIMED` 或历史合同 `SUPERSEDED`。

日期：2026-08-31
结论：`PASS（Windows 自动工程）`；`Verify/Deferred（真人试玩、Physical Input v2、正式产品体验、macOS 原生）`

## 身份与边界

- 冻结运行时代码身份：`320e293c2f1db7f46aba776ddccdcf94369f2d05`（P11E）。P11F 只修正构建、
  性能采集和无桌面延期工具并补充证据，不修改客户端玩法或渲染源码。
- Windows 工具链：Visual Studio 2017、v141、x64 Debug/Release。最终 Release EXE 为
  `8,752,640` bytes，SHA-256 `2B6B824636D3D3B6E442231CFA0114D7A181298AFE06B546789B72546B317347`。
- 正式 Q1 机器：NVIDIA GeForce GTX 1050 Ti、驱动 `30.0.14.7212`、隐藏 1600×900 窗口。
- 冻结数据身份：save v12、terrain v4、settings v8、目标 v3、敌人 v3、探索奖励 v1；资源
  manifest 84 项、双语各 411 key。
- 本报告只关闭 Stage 11 的 Windows 自动工程范围，不把真人或目标平台缺失证据写成 PASS，
  不自动创建标签、不 push。

## Windows 完整门禁

`scripts\verify_build.ps1 -VisualStudioVersion 2017 -SkipRealWindow` 从头到尾通过；会话恢复为
活动桌面后，同一 Release EXE 又独立完成真实窗口崩溃和完整打包链：

| 检查 | 结果 |
| ---- | ---- |
| 资源 manifest / terrain atlas / 性能夹具 / Stage 10 补充 | `84 / 278 / 38 / 11`，PASS |
| VS2017/v141 Debug、Release | 全量重建 0 error；既有第三方/兼容 warning `2044 / 2023` |
| WorldRuntimeSmoke | Debug、Release 均 `832/832` |
| ResourcePackSmoke / RecipeSmoke | 双配置均 `80/80` / `122/122` |
| Catalogue / Storage / Backup / Q2 / Crash smoke | `59 / 16 / 19 / 12 / 21`，PASS |
| validation-only / 启动负例 | 双配置 PASS / `15/15` |
| 受控崩溃 | dump `136,615` bytes；匹配符号、错误符号拒绝、保存恢复和下次提示 PASS |
| 符号归档 | 7 项，SHA-256 `B62C72F1CEF8B555E8615A90FC90594FE125A54E669C912E209047DFBC89E57B` |
| 干净发行包 | `104` 项；真实窗口、受控崩溃、下次提示和两类负例 PASS |
| 发行 ZIP | SHA-256 `422F97E87046D4B6D5FC4BB99C37886FF37C4461A152C1162FA66A972B12F459` |

门禁默认工具链已从错误的 VS2022/v143 固定改为 VS2017/v141，并在生成后静态检查每个第一方
客户端/崩溃目标的 `PlatformToolset`。`-SkipRealWindow` 只在无活动桌面时关闭真实 OpenGL
窗口、受控崩溃/符号化联动和下次图形提示，摘要逐项写 `DEFERRED`；活动桌面恢复后上述项目
已用完整模式补齐。

## 正式 Q1

证据目录为 `docs/baselines/playability-rc-windows-hidden-v1/`。六类 baseline/repeat 均使用
运行时提交 `320e293`、save v12、seed `20260820`、隐藏 1600×900 窗口，比较结果全部 PASS。
采集器不再硬编码旧 save v11，保存/恢复场景从同轮通用摘要继承当前版本。

| 场景 | baseline | repeat |
| ---- | -------- | ------ |
| 可用菜单 / 首次可控世界 | `996.539 / 610.143 ms` | `715.915 / 398.768 ms` |
| 通用 frame P95 / P99 | `6.819 / 11.358 ms` | `7.594 / 11.367 ms` |
| 保存事务 | `133.741 ms` | `189.593 ms` |
| 备份恢复 | `53.179 ms` | `63.072 ms` |
| 快速流送 frame P95 / P99 | `15.234 / 19.676 ms` | `14.730 / 20.647 ms` |
| 快速流送 chunk visible P95 | `32.224 ms` | `52.797 ms` |
| 规模玩法 frame P95 / P99 | `9.850 / 12.873 ms` | `9.215 / 12.166 ms` |

规模玩法两轮均保持 361 个已加载区块、910,222 个 resident terrain vertices、1,750,560 个
indices 和 36,129,344 bytes terrain buffer；本轮没有放宽预算或新增性能例外。

## 正式 Q3

`tools\run_release_candidate_soak.ps1` 使用 Release、seed `20260820`、schedule v2，nominal/stress
串行各运行 1800 秒。两个世界汇总和两个进程汇总均 PASS：

| 指标 | nominal | stress |
| ---- | ------- | ------ |
| fixed ticks / failures | `36,000 / 0` | `36,000 / 0` |
| movement / block edit / actor lifecycle / save-reload | `361 / 1,800 / 900 / 179` | `901 / 7,200 / 1,800 / 359` |
| 峰值 private / working set | `19,202,048 / 25,964,544` bytes | `25,640,960 / 32,706,560` bytes |
| 峰值 handles / threads | `229 / 4` | `229 / 5` |
| 稳态 private / handle 增长 | `1,286,144 / 1` | `8,892,416 / 0` |
| timeout / child exit | `false / 0` | `false / 0` |

## 明确延期的证据

以下项目已经实现但没有被自动化冒充为真人签收：

- Physical Input v2 十三项，以及 D2、D4、D6、R3 的真实键鼠、窗口焦点、暂停、Alt+Tab、
  最小化、回焦、保存重启链；R3 仍只保留项目所有者此前完成的部分非正式自测。
- P11-0 火把/熔炉动态光照，P11B 动作反馈，P11-1 安全落脚点与工具速度，P11C 完整 30 分钟，
  P11D 三类结构奖励，P11-2 山地/洞口，P11E 敌人辨识、掉落价值与 Waystone 共鸣的真人试玩。
- 正式视觉风格、中文/英文可读性、音效/音乐听感，以及 macOS 原生 Debug/Release 与真实窗口。
- 里程碑实际耗时、误操作、死亡原因、超过 60 秒的困惑时段和主动改变计划的时刻，只能由恢复
  后的逐批真人记录提供。

## 封板结论

P11F 可标记为 `Engineering Done（Windows 自动工程）`，Stage 11 九个计划工程批次全部完成；
当前文档内没有剩余已批准的代码开发批次。后续只剩上述 `Verify/Deferred` 验收，或由项目所有者
另行批准新的产品开发路线。真人/macOS 证据未补齐前不创建 1.0 标签，也不宣称完整产品签收。
