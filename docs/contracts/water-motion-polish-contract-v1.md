# 水域运动精修 V06b

在 terrain v26 岸形基础上改善水域运动。生成、save v12、地图v4、settings v11及碰撞不变，
旧世界也使用新版表现；不增加流体模拟或给玩家施加水流力。

- 河流表面细纹方向取已有v17+下降水系的真实曲线切向；多支流按距离平滑混合，不能随区块原点
  翻转。旧地形或自建／地下水没有可用自然河道时，用平静的风纹回退，不凭空推断河床深度。
- 湖泊保持低波幅，湿地更静，海面保留克制起伏；岸线仍由四个共享角点的真实驻留水／实体岸
  约束。流向只是表现提示，深度和浸入继续来自原有≤8格驻留水柱／相机介质。
- 快照只对水位64存在真实水块的列查询自然水域运动，每快照最多18×18次纯查询；增加固定
  18×18二维向量≤2592字节。不加载邻区块、不存世界引用；离开自然水位使用微风回退。
- 复用水材质uv0携带速度，uv1仍为深度／岸线，32字节顶点和面数不变。几何波幅有界，
  共享世界角点输出相同；细纹按屏幕导数衰减远景频率，亮度调制≤3.5%，收敛刺眼高光。
- 原设置关闭水面细节后保持静态平面／简单水色；不改变水下雾和穿水面抗裁剪过渡。

## 修改前冻结的检查

1. 三冻结seed沿真实河道中心线检查下游一致性、弯道连续、支流和负坐标边界、非零流向、
   极值／跨线程及缓存驱逐；海／湖／湿地／回退有明确有界速度区别。
2. 生产快照及网格检查四类水域、版本身份、正负区块接缝、竖向section接缝、未知邻域、
   岸线编辑重新生成、旧快照独立、干快照零运动查询及每快照查询／内存上限。
3. 双配置水深／岸面／新运动专项及MeshDirty、资源；Release完整世界、双配置客户端。
   新接口不得改变生成：修改前后v26 T0生产CSV逐字节对照。既有旧版生成路径没有改写。
4. 实际GLSL检查河流细纹位移方向、四档幅度、共享边位置／法线、相位和水面穿越连续、
   远纹衰减／日照高光有界及关闭回退；旧shader或取消滤波的故障负例须能被检查检出。
5. 四水域近岸／远水及昼昏多帧，保留2–3张升级后原图。普通入水／编辑通过公开输入另查，
   诊断相机和GPU样片不冒充普通玩法。严格配对性能与人工核验按Goal约定延期。

## V06c 自然河流实际编辑观察（2026-10-05）

以上32字节记录保留V06b历史语义；后续光源与rootTag演进后的当前顶点为44字节。V06c工程诊断
使用当前terrain30、seed42的真实驻留自然河流，目标为`(220,64,-204)`，原块为Water64／Water63／Sand62，
上方Air65。已观察的Sand岸为`(217,64,-204)`，水平距离3米；它提供附近河岸背景，未覆盖直接相邻岸条件。
初始相机和两件Sand库存由诊断设置；放置／破坏沿原`PlayerBlockInteractionCommand`、World更新和网格上传路径，
两次恢复通过显式诊断`setBlock`，不声明普通鼠标选中或公开输入。

| 阶段 | 实际动作与目标列 | 原水面四角uv1深度 | Sand库存 |
| --- | --- | --- | --- |
| 0 | 原Water64／Water63／Sand62 | 2 | 2 |
| 1 | 生产Place在y63放Sand，Water64保留 | 1.75 | 1 |
| 2 | 诊断恢复Water63 | 2 | 1 |
| 3 | 生产Place在y64放Sand | 目标顶面为Sand，无该Water顶面 | 0 |
| 4 | 生产Break移除Sand64，Air64／Water63保留 | 1 | 1 |
| 5 | 诊断恢复Water64并保存 | 2 | 1 |

阶段1目标水柱实际深度为1；四个顶点各从相邻四列平滑取值，因此原uv1为1.75。阶段4不会自动回填Water64，
实际水面降至Water63顶部。独立oracle只读每阶段真实3×3列的y56..65块id／metadata，从原u32索引恢复
Water顶面三角形，在44字节顶点offset20读取uv1深度，并检查当前声明、有限值、索引界限、几何位置和Sand顶面。
不调用生产pack／水深helper重建预期输出。

CPU对照来自实际上传输入的锁内副本。Water沿原section直接上传，Sand观察原Terrain竖向批次的完整有序parts；
读取原`ChunkSectionRenderable`的GL3Plus source0 VBO／u32 IBO，与CPU逐字节一致。每阶段要求
`cpuReadyTotal==0`，各part当前上传revision等于live revision、`GpuResident`且不再`CpuReady`，编辑与恢复后
上传revision更新。`ShoreEditCapture`只绑定`GL_COPY_READ_BUFFER`读取原存储，恢复并比较实际上下文状态，
保留读取前后的GL错误及失败输出；6阶段、最多8对象／阶段、4parts／批次、原VBO＋IBO最多16MiB／操作、
观察器写出最多256MiB／会话，接线等待最多10秒／阶段和60秒／会话。原窗口PNG由Bootstrap同帧另存。

地图检查要求HUD256／step4的目标回复晚于编辑帧，回复在进入Flat前更新已有2米fine历史；Flat的生产解析和
实际提交矩形继续使用该新值。本批RD1下Flat实际step2且坐标精确匹配目标，不声明Flat粗4米覆盖。
独立oracle读取原PNG，核对目标中央矩形实际Water／Sand颜色变化及恢复，西／北fine样本和对应矩形像素保持。
真实3×3 World邻列也保持；西／北fine点在该3×3列之外，其世界表面没有独立重算。所有引用原件实际SHA均核对。

standard与compatibility各六阶段通过，恢复后保存成功。原件及回执位于
`build/visual-experience-polish-20260928/v06c/shore-standard-r1/`和`shore-compatibility-r1/`；
[双模式独立报告](../../build/visual-experience-polish-20260928/v06c/dual-mode-oracle-r1.json)的结论为
`PASS_SCOPED_STORAGE_WORLD_MAP_PIXELS`。原raw／facts副本的NaN44B、stride32、OOBindex、staleUV、missingHUDfine
五项在更新副本SHA后均被语义检查拒绝，失败包保留于
[故障回执](../../build/visual-experience-polish-20260928/v06c/raw-fault-suite-r1/faults.json)。这些是证据消费端故障挑战；
失效规划／上传／地图生产源码的故障变体在V06c当批为`NOT_RUN`；后续实际校准见V06d。

本次原存储一致性不证明native内部VAO取数、World incarnation／ABA或地形draw像素归因；地图像素证据仅覆盖
已核对的实际矩形。普通鼠标选中／输入和原World列原子快照仍未验证。
恢复态另用 `tools/tests/shore_world_reopen_test.cpp` 对两套实际capture/save各58项通过：严格解析已改目标chunk和持久地图，
移除强制状态后公开World重开→save→再重开；Water64／Water63 metadata、库存与observe前canonical历史保持。
未改procedural邻块本就不写save文件，目标已改chunk必须存在；原错误要求9个保存文件的失败保留。
此项不声明中间编辑态保存、普通UI或GPU重开通过，详见执行报告V06c。
V06b既有GLSL样片与历史检查继续保留，不能替代上述当前客户端边界。

## V06d 实际水深生产者故障校准（2026-10-06）

在独立源码副本中将生产水深固定为基线2.0，实际编译／链接并替换单一生产对象后，标准／兼容
客户端各完整捕获六阶段，native均正常exit0、无signal／超时。原uploader CPU输入与原生GPU
存储仍逐字节相同，但阶段1实际World四角预期为1.75，两个原存储均为2.0；未修改的
`shore_edit_capture_oracle.py`均由`phase1/actual-World-four-corner-water-depth`拒绝，分类为
`SOURCE_PRODUCER_CAPTURED_THEN_REJECTED_BY_INDEPENDENT_ORACLE`。正常生产代码同条件正控
标准／兼容各208项通过。此项实际挑战生产深度来源，不沿用V06c修改捕获数据副本的结论。

HUD源码省略同列细历史刷新另有standard实测：生产step4已回复Sand64，既有fine仍Water64，
记录13条不一致；原有有界就绪保护拒绝继续，native正常exit1、无signal／超时，保留三张原图。
由于只完成阶段0–2，独立oracle为`NOT_RUN_INCOMPLETE_CAPTURE`；该结果属于就绪保护拒绝，
不是完整捕获后的oracle拒绝，详见[地图合同](adventure-exploration-map-contract-v1.md)。

本批仅为pixel ratio1、1280×720的专项校准，先前ratio2正控失败保持，不证明当前Retina2视觉验收。
5508项原输入、15个保护包共2110个完整文件及用户包135项托管文件保持；故障包实际source／binary
与名义正式参考身份分记。跳过上传源码故障、内部VAO、incarnation／ABA、原子World快照和地形像素
归属仍待验，普通出入水／编辑与连续路线继续待验。原件、正常退出与分类依据见
[生产者回执](../../build/visual-experience-polish-20260928/v06d/producer-source-negatives-r1.json)和
[最终分类复核](../../build/visual-experience-polish-20260928/v06d/final-classification-review-r1.json)。

本批随后通过普通菜单实际创建42／20260807两个terrain30世界，基地保存与同进程切世界隔离
有原图及地图文件校验支持，见[地图合同](adventure-exploration-map-contract-v1.md)。
这项普通UI进展不包含出入水、水岸改块或连续路线，也不改变上述水域验收边界。
