# HelloMine3D VISUAL-RC 工程封板报告

> 历史报告说明：以下结论冻结 2026-08-28 的真实证据，不回写为后续 AI/真人 PASS。
> Architecture Lab 的当前退出模型见 `docs/current/ai-assisted-gameplay-acceptance-v1.md`。

日期：2026-08-28
结论：`PASS（Windows 工程）`；`Verify（macOS 原生、正式产品体验、Physical Input v2）`

## 身份与边界

- 最终运行时代码身份：`d6172bd2b2e1d6e9ed9ed7f82f251531100360e8`。其后的改动只涉及
  验收记录、验证工具和路线状态，不改变客户端渲染实现。
- Windows 工具链：Visual Studio 2017、v141、Debug/Release。
- 开发者视觉记录机器：NVIDIA GeForce GTX 1050 Ti、NVIDIA 472.12、OpenGL 4.6.0.0。
  Q1 schema 3 的 `comparison_gpu/driver` 仍为 `unknown`，不以视觉记录值伪造 Q1 字段。
- 配置/存档/地形身份保持 settings v6、save v11、terrain v3；地形顶点 stride 仍为 32 字节。
- 本报告关闭 VISUAL-RC 的 Windows 工程范围，不创建 1.0 标签、不 push。macOS 原生编译和窗口
  冒烟、正式视觉/双语可读性/听感以及 Physical Input v2 仍需相应目标机器和操作者记录。

## Windows 完整门禁

`scripts\verify_build.ps1` 以最终运行时身份通过：

| 检查 | 结果 |
| ---- | ---- |
| 资源 manifest / terrain atlas / 性能夹具 / Stage 10 补充合同 | `77 / 261 / 38 / 11`，PASS |
| VS2017/v141 Debug、Release | 全量重建，0 error；第三方历史 warning 2003/2002 |
| WorldRuntimeSmoke | Debug、Release 均 `743/743` |
| ResourcePackSmoke | Debug、Release 均 `80/80` |
| RecipeSmoke / shader 与启动负例 | `117/117` / `15/15` |
| 受控崩溃 | dump `145,169` bytes，符号化与隐私检查 PASS |
| Release 可执行文件 | SHA-256 `5346A576CD94AF8E5A9BAA0C69DF77EEB6F242BA3654324A1D0DBE813B32DE53` |
| 符号归档 | SHA-256 `40E55D885700AA3112E1F59554E436F7509A32AC3E0CC964371A3F6833822752` |
| 干净发行包 | `97` files，ZIP SHA-256 `ACCB99B3F984D5FB8EF11279280406F8477ED625719F4801479DB6F7FE7A6005` |

完整门禁结束后又对新增的 V10D/V10E 开发者视觉记录执行了 `-RequirePass` 定向校验。

## 最终视觉矩阵

项目所有者已明确授权 Codex 审阅真实 Release 原图。逐批记录如下：

| 批次 | 证据 | 结论 |
| ---- | ---- | ---- |
| V10A | AO/no-AO 八场景十六图及真实窗口检查 | PASS；接触层次、透明叶片和接缝无明显缺陷 |
| V10B1 | 参数化材质管线、静态前景兼容及五分钟窗口检查 | PASS（Windows）；macOS Verify |
| V10B2 | 森林 HUD、遗迹、营地三张原尺寸图 | PASS |
| V10B3 | 五生态近/中景固定图 | PASS |
| V10C | 十图昼夜/海岸/云层上下边界及正午多帧运动序列 | PASS（Windows）；macOS Verify |
| V10D | Off/Medium/High × 正午/黄昏六图 | PASS（Windows）；macOS Verify |
| V10E | 明暗阶梯、On 昼夜、1024×768 设置页、Off 昼夜六图 | PASS（Windows）；macOS Verify |

V10E 是最后一个运行时批次，它的最终图同时覆盖此前材质、生态、大气、云层和阴影实现。检查
确认明暗阶梯保持严格递增，昼夜地形可读，HUD/UI 未被 compositor 处理，设置页无裁切，未见
过量 bloom、阴影 acne、穿透、云缝或透明叶片错误。各批结构化记录位于
`docs/reports/developer-visual-record-v10*.txt` 及对应合同。

## 正式 Q1

证据目录：`docs/baselines/visual-rc-windows-hidden-v1/`。六类 baseline/repeat 比较全部 PASS；
使用隐藏窗口、seed `20260820`、1600×900、shadow Off、post Off 和隔离运行根。

| 场景 | baseline | repeat |
| ---- | -------- | ------ |
| 可用菜单 / 可控制世界 | `1025.764 / 653.815 ms` | `1080.602 / 637.314 ms` |
| 冷启动/进世界 frame P95 / P99 | `9.606 / 13.368 ms` | `10.350 / 13.477 ms` |
| 保存事务 | `179.849 ms` | `157.536 ms` |
| 备份恢复 | `59.528 ms` | `54.997 ms` |
| 快速流送 frame P95 / P99 | `11.727 / 17.935 ms` | `12.648 / 16.700 ms` |
| 快速流送 chunk visible P95 | `64.275 ms` | `69.471 ms` |
| 规模玩法 frame P95 / P99 | `12.210 / 14.653 ms` | `12.359 / 15.254 ms` |

规模玩法保持 361 个已加载区块，resident terrain 约 899.7k vertices、1.724m indices、35.69 MB
buffer；固定模拟频率为 20 Hz。本轮没有新增性能例外。

## 正式 Q3

`tools\run_release_candidate_soak.ps1` 以 seed `20260820` 正式运行 nominal/stress 各 1800 秒，
两个世界汇总和两个进程汇总均 PASS：

| 指标 | nominal | stress |
| ---- | ------- | ------ |
| fixed ticks / failures | `36,000 / 0` | `36,000 / 0` |
| movement / block edit / actor lifecycle / save-reload | `361 / 1,800 / 900 / 179` | `901 / 7,200 / 1,800 / 359` |
| peak private / working set | `20,824,064 / 27,648,000` bytes | `30,011,392 / 36,847,616` bytes |
| peak handles / threads | `259 / 4` | `259 / 4` |
| steady private / handle growth | `4,177,920 / 1` | `13,840,384 / 1` |
| timeout / child exit | `false / 0` | `false / 0` |

## 资源、许可与跨平台状态

- 77 项 manifest 包含 4 个许可证条目；英文/中文 Credits key 与字体、音效、音乐许可路径一致。
- 97 文件干净包保留原创纹理/音效/音乐许可文件和 14 份第三方 notice；资源包覆盖保持
  base-only、路径规范化、缺失/损坏失败语义和冻结视图合同。
- Windows 上重新生成全部 31 个 Xcode 项目后，`tools\validate_xcode_generation.ps1` 通过
  `239` 项工程图静态检查。期间修复了校验器大小写不敏感导致 `AudioSampleBank.cpp` 被 `iOS`
  子串误报的问题；结果为 `native_build=NOT_RUN`，不替代真实 macOS `xcodebuild` 和窗口冒烟。
- 正式产品体验未用自动截图冒充真人签字。开发者视觉检查已完成；正式视觉风格、双语可读性、
  听感和 Physical Input v2 保持 `Verify`。

## 封板结论

VISUAL-RC 可标记为 `Done（Windows 工程）`。当前没有剩余的 Stage 10 代码开发批次；下一步是
产品方向评审，决定另立 `1.0-RC` 缺陷封板合同，还是只选择一个有试玩证据的最小玩法批次。
macOS 原生与正式产品体验未完成前，不创建 1.0 标签。覆盖 `origin/master..HEAD` 的 bundle 在
本报告提交后单独生成并用 `git bundle verify` 校验，避免 bundle 哈希与其自身记录形成循环。
