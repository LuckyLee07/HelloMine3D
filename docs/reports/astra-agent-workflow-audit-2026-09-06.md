# GPT-6 Astra 仓库指令与工作流审计

日期：2026-09-06。基线：`9a36796`，开始时工作区干净。
范围：仓库自有代理入口、技能发现、开发/验证/恢复流程；不改游戏运行时、历史合同结果、用户全局配置或第三方 CI。

## 官方依据

通过官方 OpenAI Docs 搜索后实际读取以下页面，按当日内容核对：

- [Using GPT-6 Astra](https://developers.openai.com/api/docs/guides/latest-model)：
  Astra 对指令和技能更敏感；建议明确自主推进、澄清边界、委派条件及与改动相称的测试。
- [AGENTS.md discovery](https://developers.openai.com/codex/guides/agents-md)：
  根目录到工作目录的指令层级、override 优先级与默认 32 KiB 总大小限制。
- [Build skills](https://developers.openai.com/codex/skills)：
  技能用 name/description 发现，正文按需加载；仓库目录为 `.agents/skills`，入口为大写 `SKILL.md`。

辅助 best-practices 搜索返回结果，但其页面 fetch 返回 404；不依赖未读取正文作为本次修改依据。
以下具体规则是针对本仓库证据作出的工程选择，不是照抄官方模板，也不是性能基准结果。

## 盘点与发现

使用 `git ls-files` 和 `rg --files --hidden --no-ignore` 盘点；生成输出不作为指令源。
检查根目录及祖先的 AGENTS 文件。改动前无仓库 AGENTS.md、SKILL.md、小写 skill.md、
`.agents/`、`.codex/` 或项目自有 `.github/workflows/`。
已有 7 个 workflow YAML 和 1 个配套 PowerShell 文件均在第三方 glm/imgui 内，不是本项目 CI。
执行流程实际分布在 README、`docs/current/`、`scripts/` 与 `tools/`。

| 发现 | 处理 |
| --- | --- |
| 缺少自动发现的项目指令入口，长期会话承担过多导航信息 | 新增 27 行 AGENTS.md，只保留项目定位、按需入口和关键约束；不装入历次 Goal 时间线。 |
| 没有可按需发现的验证技能 | 新增单一 `hellomine3d-validation/SKILL.md`，聚焦检查选择、证据和玩法交接；不复制验证矩阵、脚本或模型手册。 |
| README 暗示所有提交前均跑双配置全量回归 | 按验证矩阵/合同触发，文字改动只做相关检查，必要门禁不变。 |
| README 的 macOS Make 默认命令与矩阵干净门禁不一致 | 检查 premake 后确认两配置共用 bin；README 对齐保留 `-B`，不把加速等同于取消干净构建。 |
| 黑盒规则正文写死 Windows，与顶部 macOS 授权补充易产生冲突 | 正文改为当前批准平台，引用现有补充；Windows 证据不改写。 |
| 路线图“下一批”可能误导模型自动开发下一候选 | 明确当前批准批次才构成承诺，候选不自动获批。 |
| 恢复、窗口交接与权限失败混在长报告里 | 新增按需工作流：单份恢复记录、实际 handle、单窗口操作者、具体授权交接、按原因处理阻塞。 |
| 过去通过编辑器转存截图，且反复唤醒不推进 | 指向已验证的直接截图脚本；复用运行实例，区分等待/失败，不因观察超时重启。 |

没有通过提高上下文上限、强制最高 reasoning、关闭审批或复制全局规则来“解锁模型”。
本项目是本地 C++ 游戏，没有待迁移的 OpenAI API 调用；本次不添加模型路由或 API 参数。
全局/宿主规则仍可能影响行为，仓库 Markdown 不能改变工具权限或保证审批成功。

## 验证范围

- 官方 skill-creator 的 `quick_validate.py .agents/skills/hellomine3d-validation`：PASS。
- `git diff --check`：PASS。
- 链接、入口位置、命令参数与构建脚本的一致性：65 个本地链接解析成功；截图工具 `--help` 实际运行成功。
- 内容走查采用下表；这是静态决策审查，不声称模型行为 A/B 测试或节省 token 的量化结论。
- 运行时、构建脚本、测试阈值未改，不重复游戏全量门禁；已有游戏证据保留原版本。

实际命令、平台、退出码与被检查文件哈希见
[validation evidence](astra-agent-workflow-audit-2026-09-06-evidence.json)。

| 代表请求 | 应有路径与审查结果 |
| --- | --- |
| 修正文案拼写 | 不必调用验收技能；检查文案/链接，不全量重编译。 |
| 修 Cocoa 菜单焦点 | 按矩阵做相关输入回归和独立正常窗口检查，不能只用 headless 关闭。 |
| 完成已批准架构批次 | 读取相关合同、实现与完整必需门禁；不从历史报告自动重演已完成批次。 |
| D2 调查没有进入证据 | 保留 Candidate；不因工作流建议自主推进就发明开发需求。 |
| macOS 包验收 | 核验包身份、正常输入、原图及设置；不标为 Windows PASS。 |
| 独立任务创建世界 | 初始交接包含真实授权与具体对象；新可见任务按用户批准创建。 |
| AI-06 仅更换工作目录 | 仓库仍可读时不能声称 package-only；该项缺口不阻止其他检查。 |
| 工具观察超时或明确拒绝 | 前者核对原 handle，后者保留原因并处理缺少的条件；不重复启动或换工具规避。 |

## 结果与后续观察

新增常驻入口小于 3 KiB；详细流程和技能正文按需读取。原先没有入口，因此不声称“AGENTS 缩短百分比”。
改动减少的是重复决策和不适用流程的触发，实际完成率、延迟和 token 成本需后续同类任务比较。
技能结构通过校验；新会话是否实际发现/选择它应由该会话确认，不能用文件存在冒充加载验证。

并行进行的独立游戏验收任务 `01a07548-30a1-7110-9c91-175f30c20c4b` 已完成正常建档和六组 UI 检查，
报告发现暂停界面覆盖问题，证据位于 `/private/tmp/hm3d-ai07-20260906/visible-ui-matrix/`。
这是先前明确授权后取得的结果，不归因于本次新指令；原游戏 Goal 仍需回收证据、修复和回归，未完成。
