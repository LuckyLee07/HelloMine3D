# 参考多通道材质与灯源合同 v1

范围：完整参考画质 Goal 的 C。独立 Reference.surface-material v1；旧 Base.terrain-material v1/v2、保存和设置格式不变。工程、GPU、普通输入和画质分别验收，本合同不宣布完成。

## 通道、身份与预算

三种固定资源 ReferenceColour64.hmt、ReferenceNormal64.hmt、ReferenceSurface64.hmt，各为 HMTARRAY v1、64×64、256层、七级显式mip，layer=tileY×16+tileX。严格profile只有edge/mips与三个固定路径、三个16位FNV64内容身份；未知/重复/缺项、非法路径、尺寸、尾随或身份不一致拒绝。profile与三channel必须同一effective owner，禁止混拼。

颜色RGBA8存sRGB，真实GPU gamma=true；线性域预乘alpha过滤，裁切保留覆盖。法线为线性XYZ映射[0,1]、+U/+V沿作者图像，mip先平均再归一化，A=1。Surface线性R/G/B=roughness/metalness/emission，A=1，roughness>=.2；mip以法线平均长度损失扩大roughness避免镜面碎点。独立surface-authoring.json定义法线relief，不从albedo明暗推断真实高度。

row9新语义：浅石144、粗石145、陶瓦146、木147、金属148、灯罩149。旧语义/自然颜色/共用256×256 UI atlas范围保持，只追加空槽。正式4张源图、提示/SHA、surface-authoring和确定性export均保留。HDR ReferenceColour 仅对两个既有层作显式颜色别名：层21（OakPlank地板/结构木）复用作者木材147，层23（Cobblestone道路）复用作者粗石145的全部七级mip颜色；这两层normal/RME此前已同族且内容不变。层23修复来自当前street原图步行视角中旧黑白交叉像素纹理抢占道路的已观察缺陷；范围不扩展到草/叶或其他自然层。旧Warm颜色数组、256×256共用atlas/UI、全部几何/ID/yaw/collision/shader/recipe保持。

三通道payload共16,776,192 bytes，保留旧标准颜色5,592,064 bytes支持整套回退。CPU reload及GPU各至多四套64数组，约21.34 MiB；UI atlas另256KiB，不生成逐部件/朝向纹理。Normal/surface不decode，颜色不重复decode，HDR数据不提前clamp。

## 运行与回退

实际linear-HDR+standard array才选TerrainSurfaceFragment/TerrainShadowSurfaceFragment；世界/植物/玻璃/手持复用same layer/mip。HDR能力/resize回退或兼容档恢复完整原数组和程序。新profile损坏任何档明确失败，新production shader编译错误明确失败。

独立接口四项（profile＋三数组）全部未声明是完整旧发行兼容；任一声明后必须四项齐全，不能把缺失成员当能力回退。完整集合即使被旧包优先级压制仍先校验。有效pack首项优先、base最低：更高优先级的旧terrain profile／array／atlas／普通及阴影shader／program／material保持整套旧渠道；完整显式新集合可覆盖较低的旧集合。旧程序无Surface双variant时保持旧程序；半套声明或坏新shader依然失败。

world/repeat导数求dp/du与dp/dv，保留镜像UV、四yaw。退化determinant/tangent回退几何法线。生态调整沿已有作者颜色域，编码处理后回linear；新建筑直接linear albedo。Lambert+GGX/Schlick，roughness下限.2，metal控制漫射/镜面，灯罩emission固定<=4 radiance。

太阳shadow继续稳定现有投影；PBR direct sun重建可见率，ambient/local diffuse不乘太阳shadow。级联/聚光shadow仅在实际覆盖问题需要时扩展，不能宣称未做的验证。

## World光源索引

权威格ID/metadata不变。每section固定4096bit=512B派生emitter索引，生成/载入/直接chunk编辑/furnace metadata由section.setBlock更新，不新增持久数据。

World单锁resident-only snapshot最多3×3chunks×3vertical sections=27sections/110592cell检查、12m半径，确定性距离/格坐标取最近8，空索引跳过。不请求加载；edit/load/unload增World visualRevision；世界销毁不保留source历史。

旧传播block light拥有室内漫射，nearby sources仅提供有限方向镜面，乘真实传播local availability，避免八份重复漫射。source中心/线性色/半径/能量有界。普通/shadow/reflection private pass使用同帧snapshot，camera独立。

必要证据：生产GPU+独立参考及负例、sRGB/normal/roughness、同源坏包、四yaw/镜像/退化/远原点、灯index编辑/卸载及稳定性。正常编辑、室内外连续观察与保存重开仍由Goal最终验收。

## 2026-10-08 零直射暗面补充

HDR 多通道材质将旧组合亮度中的方位明暗与 AO 估计分开：已知世界网格来源时扣除原 cardinal 系数与 brightness floor，AO 估计限制在既有三步范围 `[.64,1]`；缺失来源或手持曝光继续原路径。直射太阳仍使用原始天光及实际阴影，不把侧向天空近似当作太阳穿透。

网格快照另派生一跳漫射天空可用量。对不阻光接收格，`diffuse = max(direct, 不阻光六邻居的 max(direct - 3, 0)) / 15`；阻光接收格不借光。邻居只读原始 direct，不递归传播。20³临时原始方块／天光 shell 支持全部18³网格光照样本，随后只保留派生快照；沿用现有锁内捕获与锁外 mesh build，不加载邻区、不写天光权威、不新增每帧传播任务。未加载区仍沿用既有 Air／sky15 的查询回退，不能将近似称为实际天空可见性。

仅零 direct 且有侧向天空的 Terrain／Flora 角点将 `FaceLightSources.x` 编为 `-2-diffuse`；当前配套普通／阴影四个 VS 先解码为 direct0 与 `terrainDiffuseSky` 再插值。正 direct 角的有效 diffuse 仍为 direct，greedy 比较实际解码后的有效通道。正值天空来源、block light、组合亮度及 manual `(-1,-1)` 保持，现有44B顶点布局不变；Water 保留原 direct 来源。HDR 材质环境项与有限金属天空反射使用 diffuse，太阳项继续 direct，legacy 继续原明暗。光照编辑的 mesh dirty 依赖扩大到半径2，仍使用原队列去重、revision 与构建预算；暗区块加载／卸载时也有界失效相邻最多8个已加载区块的现有 section；Near 使用原队列、revision 与预算，Far-only 只置 dirty，留待进入 Near 构建。44B布局不变不表示任意旧自定义 VS 均兼容；新来源编码只保证当前四个配套 shader，旧只读组合亮度的程序保持原输入。

这是紧邻开天空格的有界近似，不能声称解决宽檐、多格封闭空间或完整全局照明；昼夜仍受环境曝光约束，火把方块光保持独立。最终检查与实景范围见[修复报告](../reports/hdr-display-fix-2026-10-08.md)，历史证据不改称当前版本重跑。
