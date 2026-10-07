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
