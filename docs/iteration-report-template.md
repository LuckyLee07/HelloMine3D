# HelloMine3D 迭代报告模板

每个功能批次复制下面模板。只勾选与本次改动相关的验证；选择依据见
`docs/validation-matrix.md`。

```text
迭代：
日期：
范围：
任务 ID：

主要改动：
-

已运行验证：
- [ ] Windows 工程生成/受影响目标编译
- [ ] 定向 headless 自动测试
- [ ] HelloMine3DWorldRuntimeSmoke
- [ ] 交互冒烟
- [ ] 渲染截图（需要时）
- [ ] 性能采集/比较（需要时）
- [ ] 长时间 soak（封板或高风险改动）
- [ ] AI/Computer Use 交互场景（适用时；记录 PASS/FAIL/BLOCKED/NOT_RUN）
- [ ] AI 视觉/多帧/双语/音频功能检查（适用时）
- [ ] 干净发行包（发布候选）

改动前指标：
- frame_p95_ms：
- loaded chunks：
- mesh rebuilds：
- validation checks/failures：

改动后指标：
- frame_p95_ms：
- loaded chunks：
- mesh rebuilds：
- validation checks/failures：

已知风险：
-

依赖状态：
-

验收证据：
-

声明边界：
- functional_playability：
- human_fun：NOT_CLAIMED
- physical_input_feel：NOT_CLAIMED

下一项建议：
-
```
