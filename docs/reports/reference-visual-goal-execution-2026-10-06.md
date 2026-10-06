# HelloMine3D 参考画质完整交付执行记录

## 范围和起点

2026-10-06：用户明确启动“HelloMine3D 参考画质完整交付”，宿主 Goal 为 active。
在 codex/reference-visual-v1 及其既有独立工作树继续 A–E 全部范围，见[提示词](../current/reference-visual-goal-prompt-2026-10-06.md)和[方案](../current/reference-visual-upgrade-plan-2026-10-06.md)。
首版有效工程证据与未验项保持，见[首版执行记录](reference-visual-prototype-execution-2026-10-06.md)。

起始 HEAD 7b39402b，完整首版尚未提交；启动时重新核对全部 2974 个 src 文件与已交付包源码清单一致。
游戏 Release SHA-256 fbd721d9d4e51e856c1ceba913000d06dec567ae32d31dc007edd4b533163667。
世界 v12／设置 v12，shape v2 四向，HDR 固定曝光。
首版 2887 世界、814 GPU 与双配置构建证据只按原版本和覆盖复用，后续变化逐项重新验证。
只读进程核对未发现游戏客户端或在跑验证；默认沙箱 ps 被拒，权限内提升只读后获得实际结果。

## 当前账本

| 范围 | 工作流 | 执行事实与下一步 |
| --- | --- | --- |
| A HDR 收尾与生命周期 | Doing | 颜色与浮点能力首版已验；当前 CUA、resize、切世界及资源循环重新核对 |
| B 建筑套件与正常获取 | Doing | 两类样件已验；独立子任务扩展约10–12类、逐面语义和正式配方 |
| C 材质与光照 | Doing | 仍为颜色贴图；主线程接入完整通道、正确mip与方向受光 |
| D 真实倒影 | Doing | 现有模拟天空反光；子任务实现单水位有界平面反射 |
| E 植被／AA／第二样板 | Todo | 等套件与材质入口稳定后统一美术与场景 |
| 普通输入路线 | NOT_RUN | 重新核对当前公开 CUA；上次服务失败不自动视为永久阻塞 |
| 最终整合与客户端 | Todo | 全部退出条件满足后交付当前版本；首版包独立保护 |

## 执行和保护

实现范围接入正常 World／revision／保存，不把 Ogre 直接摆设或诊断库存当功能完成。
主工作区 master 和另一视觉 Goal 的代码、状态、客户端不写入；本 Goal 写入限参考画质工作树。
保护已交付 build/reference-visual-v1/HelloMine3D Reference Visual.app 及其用户可用存档，不刷新原件。
验证使用另一个单一 Goal-owned 工作包；自动画面诊断隐藏不激活，公开正常输入由唯一操作者持有。

独立调查／实现归属：
B 由 asset_pipeline_feasibility 负责 Block／Item／Recipe 等域、定义形状及相关测试；
D 由 render_upgrade_feasibility 负责 PlanarWaterReflection 和 Water shader，主线程统一整合 Bootstrap；
A 和正常输入验证由 prototype_validation 负责，当前先核对服务与现有包；
主线程负责 C、多通道资源与导出、Bootstrap、样板整合、manifest、版本账本和整体验证。

## 恢复区

Goal active，未设置 token 预算。先完整保存已验证首版为本地提交，再开始新运行时写入。
所有可独立实现继续推进；工具缺能力只影响对应验收，按宿主实际阻塞规则维护，不假报完成。
实际运行 handle、包身份、失败、原图与提交按批次追加。严格配对性能沿用延期，有限资源、明显卡顿和存档可靠性仍为必验。
