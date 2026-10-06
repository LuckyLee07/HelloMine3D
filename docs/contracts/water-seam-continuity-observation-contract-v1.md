# 水面共享边连续观察 V06i

本批补当前真实渲染的连续证据，不新增水面效果或修改世界。V06b 水域运动及 V06f 原绘制绑定合同保持。
普通客户端默认关闭；入口不提供游戏输入，也不关闭普通入水、编辑或完整四水域验收。

## 隔离与来源

`capture_visual_macos.py --water-seam` 使用新隐藏包副本、全新保存与目录、seed42／terrain31／time7000、
RD1／FOV90／first、1280×720点、关闭阴影和后处理、显式 standard 或 compatibility。
运行时与外层拒绝其他夹具、既有保存、性能及其他观察模式；单独 MODE／ROI 环境变量非法。
新机位为 `216 70 -216`／`40 0 0`，标为 `NEW_FROZEN_DIAGNOSTIC_VIEW`。
旧 `212 70 -192`／`10 90 0` 机位的共享边视野外结论保持，不称同景配对。

选择 `(13,4,-14)`／`(13,4,-15)` 两个原 Water 对象；只复制各原实际上传输入，不创建、重放、
强制可见或改写原对象。锁内公开 section 快照须显示当前 uploaded revision、GpuResident 与非 CpuReady。
原 node／object upload serial 不是 World incarnation，ABA 仍开放。
前后各读取36列（x207..224，z−225／−224）的 y56..65，共360次公开零加载块查询和36次生产纯 biome 查询，
记录两 owner 的读取前后 revision；不是原子世界快照，不把 Water64 自动称 River。

## 实际帧与容量

最多10秒暖帧等待；观察区间从第一实际暖帧或暖帧限时后的 OPEN 帧开始，每实际 render frame 均记录，
不得省略缺 draw、缺当前 owner、unsupported 或长帧。开始在真实 uploader 后，结束在 frameRenderingQueued
交换前，不调用 `_render` 重放。区间至少10秒、首／中／末三 checkpoint、最多480帧及60秒会话。
Bootstrap在入口后的55秒开始保留终态，给外层进程60秒上限留收尾余量；外层超时强制结束仍是FAIL。
到限前正常生成 OPEN；GL、原存储、身份或格式损坏保留 FAIL。处理后子进程正常退出不代表验收通过。

每帧沿原 V06f 回调读实际主相机 warm VAO／active inputs／EBO／VSFS／primitive query，并逐字节比较
实际 post 原 VBO／IBO 与本帧上传输入。另读取 linked program 的真实 globalTime、waterDetailStrength、world、
worldView、worldViewProj、cameraPosition；不能用 CPU 计算波浪代替 GPU 输出。
raw 只在首／中／末 checkpoint 写出；每帧写无损原 framebuffer ROI，同三个 checkpoint 另存同帧完整 PNG。
PNG 编码／写出不插入绘制，保持 framebuffer／read-buffer／pixel-pack／上下文及 GL0。

ROI 默认从实际 indexed 平顶共享输入及当前主相机投影计算；缺可见共享输入记录 OPEN，独立消费者再按
真实 native WVP 核验。可显式冻结 framebuffer bottom-left ROI，但不能免除真实共享输入投影检查。
两原上传输入各在复制前检查packed字节最多2MiB；超界不复制，不打断真实uploader，保留source超界事实及缺当前binding的OPEN。
此上限不是ChunkMesh内部cache的总heap声明。observer 自有活跃缓冲最多16MiB，ROI ≤8MiB，所有原件共享256MiB写预算、
预留1MiB索引。分配／编码／写出前检查，失败的部分写入按已知计划字节保守记账；codec内部活跃分配不可见，
保持 `OPEN_NOT_EXPOSED`，不宣称总进程内存上限。

## 验证与声明

正常 Debug／Release 客户端构建；新入口约束和 consumer 边界检查；旧六阶段 native 观察各595项、默认关闭各208项回归。
当前新建世界已为terrain31，旧shore工具新增显式`--shore-edit-terrain-version 31`只断言实际保存版本；
省略仍严格期待30，孤立参数／其他版本拒绝。它不设置生成版本，不放宽旧消费者或回写历史v30结果。
实际两模式捕获后由 `water_seam_capture_oracle.py --capture <capture.json> --output <新报告>`核对外层原件 SHA、
真实源列、三次原 raw 共享输入／水深／UV／光源、每帧真实 binding／uniform／时间／原 ROI 与 full PNG 对应。
合法不完整证据为 OPEN，损坏证据为 FAIL；CAPTURED 完整链不放宽。消费端故障副本保留并重算外层 SHA 后
须被语义拒绝。正常菜单客户端交付、保存及退出另核；无关的完整生成／World 测试不因只读诊断重复运行。

原属性连续及 native 绑定／uniform 是 scoped 工程证据；未读取 displaced vertex output、未作每对象像素归因，
不得据此关闭 GPU 位移输出或“全部水面无刺眼闪点”。原连续图像审阅另记，普通输入、四水域、三种子与整 Goal 保持原范围。
严格配对性能继续 `DEFERRED_BY_USER`；崩溃、明显卡顿、无界增长、容量或保存失败必须保留和处理。
