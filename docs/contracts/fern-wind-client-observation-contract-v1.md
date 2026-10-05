# Fern 生产风摆与客户端观察合同 v1

属于现有 V02 的风摆、根部稳定及生产路径验证，沿用
[植被精修合同](vegetation-polish-v24-contract-v1.md)。不追加物种、生成版本或存档字段。

## 生产行为与观察边界

- 现有 Fern 是 TallGrass／metadata 2，仅替换内置标准 Cross；继续使用六叶面、注册高度
  scale 1、Grass Top 生态格及 Flora 批次。自定义资源形状沿用原回退。
- 根稳定沿用既有 shader 验证的绝对世界位移 `<=1e-5 m`。`repeatV=1` 是本轮评估过的
  严格属性候选，现有合同未要求它；UV1 同时参与纹理重复，不能仅为通过新增候选断言改 UV。
  原候选失败保留，并与真实根位移、可见缺陷分别记录。
- Fern 的 `castShadows=false` 排除投射路径；方向阴影开启时，Flora 使用实际阴影接收
  vertex／fragment。普通及接收程序应保持相同风场和连续时钟；时间零不是关闭风摆。
- 正常入口默认没有观察器。显式诊断只接受隐藏的新 save、catalogue、output 同目录会话，
  拒绝既有路径和其他夹具；自然源来自真实已驻留 World，不写入诊断植物。
- 可选 instance 回调沿用 Renderable 的正常绘制行为，`preRender` 始终返回 true。
  Engine 在 pre 后完成 GPU 参数绑定，post 后才能读取实际程序／uniform。回调只证明
  调用尝试；真实提交须有原生查询结果，不以日志或布尔标志自证。
- 原生查询仅在没有同类活动查询时启用；已有查询记录 OPEN，不能抢占。保持原生绑定，
  在 renderable／观察器／context 销毁前清理自身查询及非拥有指针。
- 原操作及原生存储读取不等于绘制内部的 attribute 绑定观察。GL3Plus 可在 post 回调前
  解除 VAO；当前观察器不进入 Engine 绘制内部，该项明确 OPEN。

## 证据

1. 真实 World 更新／生产 mesher／公开 ChunkSectionRenderable 的原 44 字节 VBO、u32 IBO、
   声明、起点和 node world matrix。软件导出不称原生 GPU 上传或普通客户端材质绑定。
2. 原字节的生产 GLSL 重放：根位移、叶片位移、世界空间相位、连续时间、普通／接收路径、
   section／batch 原点等价及全部属性传输。重放手动设置参数，与实际 Ogre AutoParam 证据分开。
3. 原三角形的中性 nearest 基级纹理参考来自冻结 resolver 的实际 PNG／HMT；记录全部 RGBA
   采样与透明／不透明探针数。全不透明源只证明本次 alpha 通道，不关闭透明 cutout 轮廓或远 mip。
4. 自然 Fern 的实际客户端帧：实时 live／upload revision、GpuResident、源坐标／
   metadata、原缓冲范围及字节、实际 attached shader／program／clock／world／WVP、原生 primitive
   查询与状态保持。公开锁内 live 快照不含 incarnation，记录 unknown；不得追加未加锁的
   ChunkManager 查询。帧前后端点一致不自动证明全程原子性或 ABA 安全。
5. 客户端标准／兼容各四帧，普通两时点及 High 接收两时点；每帧至多四个原操作、每操作原缓冲
   至多 16 MiB、会话载荷至多 256 MiB、最多十二帧，正常运行四帧，45 秒上界。保留所有失败。
6. 必要正常双配置构建及当前实际原缓冲检查；有界错误时钟对照须 shader 编译／链接成功后
   被行为断言拒绝。错误声明的重放不冒充真实 Ogre parser／binder 的故障验证。

原始图像、固定机位和原生提交不替代普通输入、对象像素归因、连续闪烁、林间通行或完整 V02 验收。
未影响 World 生成、shader、资源或存档时，不为观察器重跑这些领域的完整门禁。
