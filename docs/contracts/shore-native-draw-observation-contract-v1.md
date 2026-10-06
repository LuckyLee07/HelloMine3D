# 水岸实际绘制绑定观察 v1

V06f 在现有水岸编辑诊断上追加可选的实际绘制观察。它补足原上传存储一致之后的
native VAO／attribute／EBO 证据，不引入渲染效果或新的玩法、世界与存档语义。
旧 `visual-water-depth-v1.md`、`water-motion-polish-contract-v1.md` 的水深、编辑失效、
权威碰撞和预算保持；严格配对性能仍按当前 Goal 延期。

## 入口与范围

- `tools/capture_visual_macos.py --shore-edit --shore-native-draw` 明确设置
  `HELLOMINE3D_SHORE_NATIVE_DRAW=1`。单独 flag、继承该环境变量或非既有合法隐藏水岸会话
  在启动前拒绝；运行时也须拒绝脱离水岸入口的该变量。未设置时保留原存储诊断和普通客户端路径。
- 沿用冻结seed42、time7000、RD1、FOV90、第一人称、地图256m、1280×720点、
  standard／compatibility与自然river目标 `(220,64,-204)`；不改变机位、目标或原选择集合。
- 六个既有编辑阶段、生产Place／Break命令、诊断恢复、当前原上传CPU副本、公开锁内
  live revision、World／HUD细历史／Flat地图与原PNG保持。诊断输入始终与普通输入分记。

## 实际绘制的证据

`ShoreEditCapture` 使用既有 `RenderObjectListener` 与实例 `NativeDrawObserver`。
listener确认主相机、scene与真实pass；pre／post回调包住该原对象的一次正常 `_render`。
不得调用 `_render` 重放，不强制对象可见、不改剔除、顶点、索引、材质或shader。

开始帧时复制实际上传绑定；结束帧时重新取得当前绑定。只有同一真实render frame里
全部原选对象实际完成post，且对象、upload serial、parts、revision及CPU副本仍一致，才记录该阶段。
serial仅为诊断对象生命周期标识，不能代称World incarnation或解决ABA。

- 自有 `GL_PRIMITIVES_GENERATED` query只在无该类活动query时开启，在配对post中结束；
  记录实际query ID、target、开始／结束、冲突及结果。支持的原indexed triangle操作须为
  single instance、single pass iteration，实际结果为 `index_count / 3`。
- 实际post的VAO必须非零，其EBO必须与该 `RenderOperation` 原IBO相同。GL3Plus首次
  `updateVAO`分支会在post前解绑；VAO零保留OPEN并等后续真正draw，不能重绑VAO补证据。
- 读取当前linked native程序，核对其实际VS／FS与原主相机pass；geometry、tessellation、
  compute或无法可靠解释的program pipeline明确不在支持范围，保持OPEN。
- 反射该程序全部实际active inputs，再按生产semantic/index映射 `vertex`、`uv0`、`uv1`、
  `uv2`、`uv3` 检验。location来自实际反射，不能硬编码。Water没有uv3，优化掉的输入不强制存在。
- 每个active location记录enabled、buffer、GL type、size、stride、normalized、integer、
  divisor与pointer offset。必须来自原source0 VBO，float、divisor0、stride44，真实声明offset
  0／12／20／28／40、size3／2／2／3／1。位置shader vec4与VAO FLOAT3按GL默认w=1匹配。
  记录实际GL major／minor和long状态的证明方式：GL4.3以上直接查询ARRAY_LONG；GL4.1／4.2
  仅以实际ARRAY_TYPE为GL_FLOAT排除未转换double，标明`DERIVED_GL_FLOAT_EXCLUDES_DOUBLE`。
  这不是ARRAY_LONG查询值；任何非FLOAT属性不接受。该query由[GL4.3 F.2](https://registry.khronos.org/OpenGL/specs/gl/glspec43.core.pdf)新增，
  [GL4.1 §2.8](https://registry.khronos.org/OpenGL/specs/gl/glspec41.core.pdf)要求VertexAttribLPointer类型为DOUBLE。
- vertex_start／index_start、数量、u32类型、实际缓冲ID／字节范围与原操作对应。
  post通过COPY_READ读取该实际原VBO／IBO，和本帧arm的CPU副本逐字节比较并保留四个raw原件；
  旧阶段原存储观察继续独立保留，不能拿上帧或帧尾不同对象的raw替代draw时原件。

## 状态、寿命与容量

COPY_READ是存储读取唯一改变的buffer binding；不bind ARRAY／ELEMENT_ARRAY／VAO。
inspection前后context严格恢复，记录GL错误，query结束后不残留观察器自有query。
销毁或替换任何原对象之前detach observer及query；取消帧、退出世界、renderer reset和观察器
析构同样清理。observer默认null，原preRender仍返回true。

六阶段、最多八对象／阶段、每对象最多四原parts、CPU＋IBO每对象16MiB及累计输出256MiB
保持。实际draw raw和旧storage raw共享累计写预算，不将双份输出分开计数来放宽容量。
等候仍受原10秒／阶段及60秒／会话限制；未完成draw、query冲突、初次解绑或unsupported
保存具体OPEN原因，超过界限保存FAIL，不能把超时作为成功。
不完整帧不得与下一帧拼接；原文件拒绝覆盖，必要重试使用有界内存或另名原件并计真实预算。

## 记录与独立检查

旧phase packet保留原schema与全部存储字段，另加 `native_render_frame_id` 和
`native_draw_observation`（schema `hellomine3d-shore-native-draw-v1`）。native frame与UI frame
分别记录，不伪造相等；该观察的UI frame对应原packet frame。
只有本mode成功时，`inner_draw_vertex_fetch`标明 `SCOPED_ACTUAL_WARM_VAO_BINDINGS`。
默认mode仍为 `OPEN_NOT_OBSERVED`。

新独立oracle先执行原六阶段208项存储／World／地图检查，再独立检验新增字段、
同帧绑定、实际query、active inputs及post原缓冲。需要两个真实材质模式、默认关闭路径回归、
GL0与完整退出；新元数据故障副本重算外层SHA后仍须被语义拒绝，原件保持。
正常Debug／Release客户端按实际构建入口验证；diagnostic code不改World规则，不因此要求
重复未受影响的完整WorldRuntime或旧冻结包全量扫描。

该PASS只证明被观察主相机实际warm draw的绑定链与原存储。未采初次解绑分支、shader输出／
地形像素归因、World原子快照／incarnation／ABA、普通鼠标编辑、连续玩法及完整Goal仍分别待验。
