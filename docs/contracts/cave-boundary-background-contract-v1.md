# 地下有限可见边界背景合同 v1

V09c 修复低视距洞室远端露出天空的表示缺口。固定世界 `239701883`、
机位 `390.5 78 -506.5`、朝向 `-8 90 0` 的 RD1 通道出口为复现点；
同机位 RD8 存在真实后续岩壁。不能为显示缺口修改世界生成、碰撞或保存数据。

## 世界快照

- 只在已加载且 `requiresNearRepresentation` 的 chunk 外侧产生候选。
  邻居不需要 Near 表示就是有限可见边界，包括 resident-only 的预加载邻居；
  两个 Near chunk 之间的真实洞口不产生此背景。
- 每个 section 四个水平面最多各一份 16×16 bit 遮罩。row 是本地 Y，
  ±X 面 bit 是本地 Z，±Z 面 bit 是本地 X。仅 owned 范围真实 `Air`
  且天光严格为 0 的格子置位；固体、水、有天光空气和未知外侧均不冒充暗空气。
  不查询玩家眼亮度、地貌或树形，也不生成或加载外侧块。
- `WorldMeshSnapshot` 复制有效非零面，不发布抢占式 delta。零面仍缓存，
  避免稳定地表逐帧重复查询。定位、blockRevision 与 transient chunk incarnation
  一同确定缓存有效性；正式 World 块／天光变动经现有照明队列失效受影响 section，
  卸载重载、直接加载数据和 reset 不沿用旧值。底层 raw 光 setter／rebuild helper
  不独立增加 revision；驻留编辑必须走 World 队列，不能宣称任意直接写光均会失效。
  dirty 旧值当次停止发布，不等预算补齐才移除。
- 最多缓存 2048 面；候选、替换缓存和输出各自不超过此上限。
  按距需求 section Y、距玩家需求 XZ、坐标及 face 确定优先级。
  top-K 临时表有上限，不能先积累全部候选再截断。
- 一次 `collectSectionMeshSnapshot(true)` 最多扫描 8 面／2048 owned cells。
  正常 Ogre 帧仅首份快照允许扫描；上传确认调用 `false`，不追加扫描。
  额外消费者显式调用 true 是另一次生产采样，不声称任意调用次数仍受帧预算限制。
  deferred 包含未扫描和容量截断面；高视距或超高世界不能宣称全部遮罩完成。

## Ogre 背景

- 背景位于精确 section 边界。sky queue 5 之后、普通几何 queue 50 之前，
  depth check on、depth write off、无投影、无受影。普通几何仍覆盖背景；
  空 mask texel discard，天光口与地表天空保留。
- 地下关闭视觉退场时，精确保留既有地下 fog 色 `(0.035,0.043,0.054)` 和原 mask。
  露天背景表示复用普通几何的逻辑相机 xz 中心、`viewRange` 与已平滑的
  `viewRangeStrength`：水平距离为 `max(abs(dx),abs(dz))`，覆盖率为
  `mix(1,1-smoothstep(start,end,distance),clamp(strength,0,1))`。
  `end<=start` 或 `strength<=0` 时原行为不变；零覆盖 discard，部分覆盖按既有
  authored fog 色与地下色混合。完整覆盖直接输出原地下色，避免端点重算。
  不改变天空、雾密度、曝光、terrain/actor/water shader、World 遮罩语义、
  世界网格或四层地形 batch。它表示有限区外的暗背景，不宣称远端真实地形已加载
  或增加了一堵可碰撞的墙；退场也不把已知暗空气改写为天空格。
- Bootstrap 在既有环境同步中向 renderer 的真实 owned material clone 复制这四个
  fragment 参数：`viewRange`／`viewRangeCentre` 为 float2、`viewRangeStrength`
  为 float1、`fogColour` 为 float3；Ogre elementSize 分别 2／2／1／3，arraySize 为 1。
  全部四项缺席的完整旧资源保留原行为；完整四项才启用，partial 或 type／size 不符
  明确拒绝。新增参数组的完整新 VS／FS／program 须一致提供世界位置与自动 world 矩阵。
  本组件不新增玩家眼亮度或 World 查询，只消费既有退场强度；初始参数尚未同步时
  使用关闭退场默认值。反射 private pass 从当帧真实 owned clone 深拷贝同一参数，
  不把反射眼、显示相机或诊断相机替换成需求中心。
- 固定 2048 GPU slots，每面一 quad，独立 20 字节 position+UV 顶点与 uint16 索引。
  R8 atlas 为 512×1024、无 mip、nearest；16×16 tile 对应一面。
  顶点 160KiB、索引 24KiB、atlas 512KiB，总固定 696KiB，另计驱动和对象开销。
- 一帧至多激活 8 个新／变动面，tile patch 至多 2048 字节；先完整上传 tile，
  后激活 quad。旧 owner／revision／mask 的 quad 立即退化；删除无需清 tile，
  新 owner 不能在自己的 tile 成功前显示。删除退化顶点写入另计，不冒充八次上限。
  第二份确认只清理／保留有效面，不激活新面。固定索引只初始化一次，
  不逐帧重建全部 geometry 或 atlas，不加入原八 section 上传统计。
- 换世界／退出释放或清空 GPU mirrors；返回菜单不保留前世界可见遮罩。
  malformed face、冲突重复身份和资源接口缺失显式拒绝。

## 验证

- 双配置真实 `CAVE_BOUNDARY` World fixture：四面与负坐标、暗空气／非空气／
  有天光空气、内侧 Near 与 resident-only 邻居、修改／照明失效、重载身份、
  RD 切换、独立消费者、zero 缓存、reset、checkerboard、2048 面容量和八面扫描。
- 保留相关 MeshDirty、MESH_INPUT、LOCAL_LIGHT_RENDER、TREE_ROOT_TAG、
  RENDER_BATCH 与 streaming focus，Release 完整世界及双配置客户端。
  资源检查正例与缺文件／接口漂移／背景深度和阴影状态负例。
- 实际生产 GLSL 检查空／满／checkerboard 与 atlas 边沿 slots、地下色、
  不写深度和普通后续几何覆盖，移除 discard 的故障必须拒绝。
- 本次视距扩展须重验实际生产 GLSL 的 strength0 远域原颜色与深度精确保持、
  strength1 近域不变与远域零覆盖、半覆盖 fog 混合、负坐标／对角／逻辑中心，
  以及 legacy／HDR16F；移除视距 coverage／discard 的故障均须被真实像素检查拒绝。
  原 mask 条件、空格丢弃和深度门槛不得放宽。Ogre fixture 要读取真实 owned clone
  的参数类型、size 和当帧值，保留 696KiB／八面更新／清理门禁；完整旧接口、
  partial／wrong-type／wrong-size 资源分别验证，静态准备不计 native PASS。
- 冻结旧客户端与新包同机位昼夜 RD1／2／8 原始画面，另外检查地表与真实天光口。
  连续视距／移动、普通菜单入口和保存重进分记；诊断机位不能充作普通地下探索。
  检查新缓冲固定上限、真实 deferred 与清理，严格配对性能仍 `DEFERRED_BY_USER`。
- 当前版本另以同 binary／资源／保存／机位请求验证临水建筑的 RD1／RD3 露天
  对照和近处房屋室内、门口、地表、真实洞口；原 V09c 通道机位的 RD1 昼夜与 RD8
  后续岩壁仍须保留。普通输入从室外进入暗室／洞室并返回、实际视距设置切换与
  保存重进另验。诊断原图、RD3 完整建筑或参数 fixture 不替代这些普通路线。

terrain30、save12、map4、settings11 保持。无生成序列改动，不将旧 T0 或旧 GPU
结果冒充本批执行；本批不关闭 V09 的普通三 seed 入口／采集／返回或整合 Goal。
