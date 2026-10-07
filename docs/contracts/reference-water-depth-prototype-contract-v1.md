# 有界水面深度排序原型合同 v1

2026-10-07。仅验证已观察到的岸边三角／透明覆盖排序，属于参考画质D的默认关闭工程诊断；未进入普通材质，不代表画质或普通输入通过。

`HELLOMINE3D_REFERENCE_WATER_DEPTH_PREPASS=1` 必须配套实际HDR、hidden1、capture1／exit1、3500/7000ms两个检查点和5000ms延迟上限，使用绝对自有ROOT/save/catalogue/frames路径；额外编辑／驻留／跨水／设置／生命周期draw observer禁止。运行入口只检查路径为绝对值，目录归属由隔离启动器的独占目录、完整保护清单和实际进程映像证明，不冒称此入口检查了归属标记或符号链接。普通run路线缺省或空值完全不装监听，非法值在Root前拒绝；--validate不执行此原型或提供其入口拒绝证据。指定原型却缺shader接口或实际HDR时明确失败，不篡改旧资源兼容路径。

仅在实际主camera／HDR scene RTT／IRS_NONE，对原Water queue8 priority100单pass执行。先尊重已有RenderableListener拒绝和technique替换；每帧至多1024个已驻留Water renderables、唯一私材质／technique／depth pass，原VBO/IBO不复制，不增加RTT、流送或World状态。每帧从真实colour pass完整复制参数／纹理，然后只改colour_write false、depth_write true、replace blend及waterDepthOnly1。

通过公开RenderQueueGroup::addRenderable直接加入同group priority99，避免递归listener。实际Ogre按priority升序完成整个99，再执行100；原colour pass0、透明排序及Planar绑定不变。Planar先拒Water再调用previous，因此反射／阴影不增加原型depth。每帧检查全部depth完成才开始colour，末端queued/depth/colour数量相等。原型shader只存在隔离clone；最终原Alpha为0／非有限或legacy时depth discard，colour0保持源RGBA表达式。

wall-time≤30秒、frame<2048。首四Root frame及每120frame用现有native draw observer记录实际linked GL program、depthOnly/HDR uniform、depth与colour写mask、depth test/LEQUAL、primitive query实数和全局顺序；最多21采样frame，无截图性能宣称。监听在每个post draw解除，query与私材在Scene/HDR/Root前释放；禁止与其它draw observer竞争。普通World版本和用户包／保存原样。

必要证据是两配置实际构建、当前隔离包，原型正例／缺接口或入口坏值阴性、完整原图实际查看、全局深度先于颜色的实际draw及有限额外提交、正常保存与退出。不能把单个对象depth→colour当全局顺序，也不能把资源文本当实际driver状态。额外成本为Nwater depth draw，0新geometry／RTT，严格配对性能仍延期。

预绘制会影响水后玻璃等透明合成；Alpha接近0仍可能写深度。此限制和旧VS覆盖、legacy、水下／其它水位必须另验，未通过前不推广正式资源或把该原型称为正常水修复。普通12路线继续NOT_RUN。
