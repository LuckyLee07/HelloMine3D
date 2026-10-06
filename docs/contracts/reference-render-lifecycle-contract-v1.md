# 有界实际渲染资源生命周期诊断 v1

本合同补完整参考画质 Goal A／§5 的工程生命周期证据。普通输入、正常目录 UI 选择、连续视觉、长 session 和三组配对性能均不由此探针关闭。既有 v4／r7 包、旧失败和旧不完整隔离证据保留。

## 严格入口

默认不设置 `HELLOMINE3D_RENDER_LIFECYCLE_PROBE` 时完全关闭。显式值只能为 `1`；要求 `HELLOMINE3D_WINDOW_HIDDEN=1`、windowed、linear HDR、standard、初始 1280×720 point。该工程探针要求实际 HDR colour/depth/stencil 四采样；能力不足不能假装通过生命周期。

`runOgreBootstrap` 的既有 try 开头在 ResourcePaths／资源读取／manifest writer 前调用 probe admission。只有 PROBE 或 FAULT 变量存在时检查：实际 validateOnly 分支为 true 拒绝 `HELLOMINE3D_VALIDATE_ONLY`；非空 `HELLOMINE3D_EFFECTIVE_MANIFEST_OUT` 拒绝，防止任意目标文件先被 trunc。两变量都未设置时立即返回，普通 validate-only 与 effective-manifest 输出行为保持不变。空 manifest 值不写文件，允许。此 admission 不取代后续 exact opt-in／hidden／隔离目录检查；仅把这两个实际前置入口提早拒绝，不修改 OgreMain 或渲染逻辑。最终 r5 构建已实际执行 sentinel 不变负例，以及无 probe 的 validation／manifest 正控，独立复核 153／153。前者在 Root 与 writer 前拒绝；后者只覆盖 CPU validation，保留其私有存档变化，不视为普通输入或渲染验收。

固定隔离 session 布局为：

- `HELLOMINE3D_SAVE_DIR=<session>/save-a` 与 `HELLOMINE3D_LIFECYCLE_SAVE_B=<session>/save-b`：两份实际已有存档 clone，含 world.meta 和 chunks。
- `HELLOMINE3D_CATALOGUE_DIR=<session>/catalogue` 和 `HELLOMINE3D_RENDER_LIFECYCLE_DIR=<session>/lifecycle`：启动前不存在。
- `HELLOMINE3D_ROOT=<session>/Runtime.app/Contents/Resources`：本轮独立包副本。
- session 内普通文件 `.hellomine3d-render-lifecycle-owned` 的内容精确为 `HelloMine3D owned render lifecycle session v1` 加一个 LF。

路径必须绝对、canonical、不经 symlink，固定 sibling 路径不得相互覆盖。普通窗口、缺少 SAVE/CATALOGUE、共享目录、已有输出／catalogue、其它 capture／perf／UI／motion／样板注入／resource override／forced fallback 均在 WorldManagement/World 可能写入前拒绝。入口只读取已知实际触发项，不遍历或禁止全部环境变量；ROOT、SAVE、CATALOGUE、WORLD_TIME、SEED、PLAYER_POSITION/ROTATION 和 MSAA 身份配置按探针合同保留。capture／fallback 和本轮追加 bool 触发项可显式0表示关闭；既有固定文本 fixture／输出目录名单要求为空。

入口名单使用真实 `HELLOMINE3D_MATERIAL_IDENTITY_CAPTURE_DIR` 和 `HELLOMINE3D_PAUSE_NOTIFICATIONS_DIR`，替换原错误拼写。这两个其它 capture 自身也要求 primary render-capture 开启，所以原错误拼写带来后置拒绝，而不是本轮已证实的混合PASS。`HELLO_VISUAL_CAPTURE` 是 OgreRenderCapture 的历史启动别名；primary `HELLO_RENDER_CAPTURE` 未设置／空时，非零别名必须提前拒绝，primary0时别名不激活。只存在 DIR/MS/PREFIX 等辅助值而无有效启动项不会启动 PNG，不因此禁止它们。

实际拒绝的追加触发包括 FORCE_LEGACY_TERRAIN、V10C_FALLBACK、V10D_SHADOW_FALLBACK、V10E_POST_FALLBACK，V10D_SHADOW_DIAGNOSTICS 全图 readback、DISABLE_VERTEX_AO、V10E_SETTINGS_FIXTURE 自动菜单，V10D_SHADOW_FIXTURE／V10E_POST_FIXTURE，以及 World 构造中的 TRANSPARENT／SPAWN_VALIDATION_ACTORS／ORE／CONTAINER／CRAFTING／COMBAT／HUD／CROP／VERTICAL_SLICE bool fixtures，MACHINE／P11_LIGHT／VERTEX_LIGHTING 文本 fixtures、HUD_PAGE／HUD_INSPECT_SLOT、ACTOR_VISUAL_DISTANCE 和 CONTROLLED_CRASH。当前仅出现在其它拒绝名单、没有运行时启用消费者的 TERRAIN_FALLBACK 不当作实际路线开关来宣称验证。旧 r4 严格入口漏口已经由独立 owned 包真实复现：单独 V10D_SHADOW_FALLBACK=1，隐藏 Release 自然exit0、2.2396秒、13阶段／epoch3 COMPLETE，实际 shadow forced-off；来源包与模板未变。该混合结果保留为入口FAIL，不能作为允许其它 forced fallback 的正例。原清洁 r4 Debug／Release 1790项工程证据仍有效，因为其启动环境没有这些其它触发项。追加拒绝已在最终 r5 双配置构建后重新串行验证：十个实际负例均自然 exit1、原因准确、全部 owned sentinel 不变；probe-off CPU validation 正控自然 exit0。原计数和失败门槛不变。源包、模板和已发布包身份由外部 runner 全部 hash 保护。runtime 不声明自己能证明外部 clone 来源，runner 保存 A/B 初始字节身份与实际 lsof executable 证书。

诊断只通过真实生产调用工作：同一个 Root、SceneManager 和 RenderWindow，`RenderWindow::resize(point)` 加正常窗口通知／messagePump 后，下一正常 `HdrPipeline::beforeFrame` 分配；世界关闭使用 `clearActiveWorld(true)` 的正常保存和 `SandboxRuntime::closeWorld/WorldManager::closeAllWorlds`，后者销毁 World，执行 ChunkRuntime stopLoader/join；随后 `buildTerrain(true, directory)` 重新创建实际 Sandbox/World/loader，并走正常 shadow/actor/UI-context 配置。不调用 false-save 绕过，不再次注入样板。

A→B→A 指两份独立真实目录和三个 runtime 实例 epoch，不要求 clone 的 world_id 不同，不宣称用户通过目录 UI 选择了不同历史世界。记录实际 World/Sandbox 地址、目录、磁盘 world_id 和 epoch；地址复用不是失败。正常 close 返回后的 Sandbox/World owner 均不存在；loader 的同步 join 路径以正常返回 checkpoint 记录，不伪造未暴露的线程 handle 或声称检测了 OS 所有线程。

## 固定阶段与上限

每阶段恰好一组 `begin → checkpoint → end`，release 记录可夹在当期阶段内；序号全局严格递增。固定 13 阶段如下：

1. `warm-a`：至少 12 个真实正常帧，实际 Planar active／updateCount>0／privateMaterials>0。
2. `resize-small`：真实窗口 960×540 point。
3. `resize-large`：真实窗口 1600×900 point。
4. `resize-restore`：真实窗口回 1280×720 point。
5. `clear-a1`：正常关闭 A，建立 warm-menu Manager 名称基线。
6. `load-b`：从 B 目录正常构造新 World/Sandbox，epoch2。
7. `warm-b`：实际反射居民绘制恢复。
8. `clear-b`：正常关闭 B，全部 Manager 名称回 warm-menu 基线。
9. `load-a2`：重载 A，epoch3。
10. `warm-a2`：实际反射居民再次恢复。
11. `clear-a2`：正常关闭 A，第二次回 warm-menu 基线。
12. `components-destroyed`：正常 shutdown 中先销毁 Planar/HDR；此时 SceneManager/Root 必须仍活着，反射 camera 已删除。
13. `root-shutdown`：记录同 Root 最终销毁；之后不再 query GL／Manager。

单轮最多 4096 frame／45 秒、单阶段最多 1024 frame／8 秒、journal 最多 128 条；等待只读实际生产状态，不向字段伪写“ready”。同步保存／join 无法靠 frame timer 中断，外部 runner 必须有总 90 秒 watchdog，并只终止自己已证书化的诊断 child。无 begin/end 返回的阻塞阶段不得判通过。

resize 必须记录 requested point、实际 window/viewport framebuffer 及初始 1×／2×scale；三次 actual dimensions 改变／恢复，HDR successful generation 恰好分别 +1。固定 target definition 防止 Ogre 先于 beforeFrame 自动增配。HDR physical cap 保持 8,294,400 pixels；Planar 实际向上取整半尺寸、cap 2,073,600 pixels。每个 live checkpoint 要求真实 native完整 FBO、RGBA16F resolve 单采样、colour/depth/stencil expected samples／尺寸一致，实际 owned DepthBuffer attached 且 POOL_NO_DEPTH，GL error before/after 均 0。

## 释放与观察

Facts 只包含名字／数字／bool，不能持 TexturePtr/MaterialPtr 延长被测寿命。release gap 在新 allocate 前，记录旧 resolve/draw FBO 与 colour/resolved/depth/stencil id/kind，独立 `glIsFramebuffer/Texture/Renderbuffer` 的实际 alive 值及 Manager 名字是否仍存在。旧名字复用不通过“新旧 GLuint 不同”推断。GL query 恢复原 read/draw FBO、renderbuffer 和当前 active-unit 2D binding；不清色、不保存 PNG、不读回全图。

正常 HDR remove 在移除 compositor instance 后立即使用 Ogre public `CompositorChain::_compile` 清旧 compiled RSQuadOperation/私有 material/TUS 引用，再分配新目标。单删 instance 只 markDirty，旧 compiled quad 可能保留旧 texture 到下一 draw；即时 drain 是正常生产路径修复，不能只在探针开启时修复以伪装生命周期通过。释放异常打印日志，探针必须 FAIL；析构清理不可因探针失败再次抛阶段异常。正常 remove 同时恢复主 camera 的 lastViewport 到仍存活的 owner main viewport：Ogre viewport 析构不清 camera 的 lastViewport，完整正常 main draw 会恢复，但异常中途 RTT render／cleanup 不应依赖未来 draw。此处是生命周期保证，不宣称已原生复现 UAF。HDR release 记录 camera_viewport_restored bool，snapshot 的 main_viewport_bound bool 必须在 Scene/window 存活时为true；尺寸从实际 window viewport0 获取，不能先解引用可能已退休的 camera lastViewport。

Planar resetWorld 后颜色 target／独立 depth／private materials/passes、Water sampler、binder、selection、LOD-camera 引用、作用域 listener 均为 0/false；组件镜像 camera 在 world-reset 后仍保留1，仅 component destruction 时归0。HDR 在 warm-menu 仍 active且保留自己的有限目标。主 residents 的 section/batch/dirty/render-state/live-section/material-identity-revision 缓存和 localLights count 必须为0，dynamicShadowOff 必须为true（实际 qualityOff、sunLight/sunNode空、Scene shadowNone）。snapshot cache keys为 material_identity_revisions、local_lights、dynamic_shadow_off；最后一项是bool，其余是数量。第一次 warm-menu 的 texture/material/mesh/high-level-program/compositor Manager 全部 `group:name` 名称集合为稳定基线，后两次 close 必须精确回到该集合；全局 memory/count 同时保留，不能只看 RSS。组件销毁的 release gap 校验及镜像 camera 无 named object 是严格门槛；SceneManager/Root 存活时已无组件 owned native资源，随后 Root 正常退出。

## 宿主负载与证据范围

runner 默认发现其它游戏时拒启，保留启动0的收据。显式 `--busy-host-engineering` 可另建进程内资源工程案例：记录启动前／中／后全部 ps 及其它游戏，保持隐藏、不激活、输入0、独立包／两存档／catalogue、唯一自有 Popen 清理，以及所有 45秒／4096帧、8秒／1024帧、格式／预算／释放门槛。此时 GL 对象名字空间、Ogre Manager 和 World 属于自有进程；其它负载可能使能力探测／超时失败，必须保留失败，不放宽门槛。已有默认拒启不能改写成通过，共享机器案例不声明性能隔离、P95／P99、普通输入或唯一普通窗口验收。源码核对隐藏 Cocoa 创建／resize 不进入 show/activate，运行时不 capture OIS 或光标；不读取或操作其它窗口。

## 证据格式与负控

输出 `lifecycle/journal.jsonl`（schema `hellomine3d-render-lifecycle-journal-v1`）和 `lifecycle/summary.json`（schema `hellomine3d-render-lifecycle-summary-v1`）。journal 每条含 sequence/event/phase/frame/elapsed_ms/normal_input:false、actual world_epoch/world_instance/world_directory/world_id、snapshot。snapshot 含实际 Root/Scene/window 指针、是否活着、requested/window/viewport 尺寸、HDR/Planar事实、cache、Manager count/memory/name集合、hidden/perf/input_event_count。五个真实 OIS callback 计数必须0；probe不合成输入。release 包含 owner、before facts、after objects/name alive及GL error，runtime_pass仅并列字段，独立 oracle必须自己推出释放结果。

summary 的 COMPLETE 只有所有13阶段、组件／Root有序销毁且无失败时可写；同Root/Scene身份、epoch和硬上限一并记录。runner实际进程exit0、实际二进制／资源身份、隔离 full ps、超时情况分别验证；summary不能掩盖非零退出或缺阶段。

唯一 native fault 为 `HELLOMINE3D_LIFECYCLE_FAULT=delayed-hdr-drain`，只能伴随合法 hidden probe。首个真实 HDR release gap 跳过即时 drain，恢复旧行为，读取仍 live 的真实旧 GL texture/FBO，必须产生 FAILED／非零退出；随后正常 cleanup 恢复 drain，不扩大到普通路径、不泄漏测试包。其它值或无 probe 的 fault 在启动前拒绝。独立 oracle还应保留 missing/duplicate/out-of-order阶段、假normal、错point/pixel／budget/sample／generation、保留native或Manager名字、错误world epoch/directory、组件销毁前Scene提前死亡、非零exit、超时等故障副本。

原生 Debug r2 已保留真实失败：三次 resize／旧 HDR/Planar GL 退休通过，clear-a1 后仍有 ShadowTexture0，clear-b 因 warm-menu Manager 名字不同而自然 exit1。clear-b begin 的 material50 是尚在运行的39基线＋10 Planar private＋1本 Scene shadow material，不能把这个 pre-clear 值称为材质泄漏。根因是正常关闭先 Scene shadowNone（内部 destroyShadowTextures／clearUnused），随后才移除应用 base CONTENT_SHADOW texture units；clearUnused 时旧 sampler 仍持 TexturePtr，造成 ShadowTexture0 留到下次 configure 才删除并创建新名字2。正常 destroyDirectionalShadowResources 改为先 qualityOff／strength0、切 receiver 程序并移除本应用 shadow units，再 Scene shadowNone；正常 configureDirectionalShadows 入口也采用同 base-receiver→Scene-none 次序；活跃反射视图的合法 private-pass shadow 引用会在下一帧正常 pass 重拷贝更新，这不等价于 world-close 的 resetWorld 后零引用证据。不调用全局 clear，不强删其他 Scene 的引用。精确基线门槛保持；修复后 r3 以及最终 r4 原生 Debug／Release 均完成三次 clear，各五类 Manager 的完整名称／count／memory 恰好回 warm-menu 基线，原失败记录保留。

strict Manager 名称失败打印各类型 added／removed 名称；failure journal 保存拒绝操作后的实际 snapshot（能够读取时），含 Manager 名称／数量／memory 与缓存事实。失败不得仅留下 runtime_pass，或用下一初始化重命名、忽略 Internal 组来掩盖残留。

实际结果见[完整 Goal 执行记录](../reports/reference-visual-goal-execution-2026-10-06.md)。最终 `render-lifecycle-build-r5` Debug／Release 构建 exit0；`render-lifecycle-native-r4` 两正例自然 exit0，各 13 阶段、53 记录、83 帧，独立 oracle 各 895／895。真实 delayed-drain 负例自然 exit1，四个旧 GL 对象仍活着而被严格拒绝；其清理不等于重新观察到这些首次旧 ID 已死亡。Release oracle 校准含 1 个真实正例和 22 个故障副本，23／23。十项启动负控及 probe-off CPU 正控独立复核 153／153。运行时与资源源码仍为验证冻结身份；本合同结果段后来同步，不事后改写原七文件冻结收据。

这些进程均为明确标记的 busy-host 隐藏工程案例，没有普通输入或性能隔离声明。普通窗口 resize、菜单选世界、设置切档重启、行走卸载返回、编辑和保存重开仍为 NOT_RUN；长期无泄漏与全部 Goal 完成均未声明。
