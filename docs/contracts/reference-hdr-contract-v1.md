# HDR 资源链接与回退合同 v1

2026-10-07。本合同补参考画质 Goal A 的实际 shader 资源验证，沿用[首版颜色与回退合同](reference-visual-prototype-contract-v1.md)及[抗锯齿合同](reference-spatial-aa-contract-v1.md)。不改 shader、颜色转换、HDR/AA 预算、世界、存档或设置语义。

`HdrPipeline::initialize` 在请求 linear HDR 时，先保留已有的资源、stage、backend、实际单阶段编译检查，再验证 `HelloMine3D/HdrResolve` 的实际 technique0/pass0。材质必须有该 pass、实际 vertex/fragment program 及对应 stage；实际程序 load/compile/supported 成功后，组合仍须实际链接成功。独立单阶段 compile 不足以证明 vertex 输出与 fragment 输入兼容。上述错误发生在强制回退、能力判断、浮点 FBO probe 及目标安装之前，不能被 `install` 的目标失败捕获改称 framebuffer 不支持。

链接证书使用当前 Ogre `GLSLShader` 的 public `attachToProgramObject`，包含实际预处理/编译阶段和递归 child shader libraries。证书是独立、非 separable、从不 bind 的临时 GL program，不调用持久 program handle／program manager 激活，不进入 Ogre 链接缓存。函数只在初始化执行，不新增每帧目标或持久渲染资源。Ogre 继续拥有原 shader；证书删除自动 detach 全部 attached stages。RAII 在异常路径同样删除临时 program，不删除原 shader、不清 Manager。

实际 `GL_LINK_STATUS` 失败以 `Invalid linked HDR resolve shader resources` 和 driver info log 明确拒绝；driver log 读取最多 16,384 字节。已有单阶段坏语法的失败路径保持。进入证书前的 GL error 必须明确失败，不能默默消费为成功。删除后必须实际 `glIsProgram=false`、`GL_CURRENT_PROGRAM` 前后相同、GL error 为0，否则拒绝。简洁的启动日志 `HDR_RESOLVE_LINK` 记录实际 vertex/fragment 名称、临时 program id、linked/deleted、current program 前后和 GL error 前后；它是实际查询记录，不是对完整渲染生命周期的声明。

`HdrPipeline::initialize` 的 legacy 请求仍在新增证书之前早返回，全局资源合同行为保持。合法完整旧 HDR resolve 可以没有 `spatialAaStrength`／`inverseTextureSize`；新增链接验证不要求 AA uniform。声明了 AA 接口时，原有 pixel-size auto-binding 检查保持。浮点 HDR／MSAA 能力或目标不可用仍按原合同回退；新增链接错误不构成合法能力回退。

## 当前实际证据

修复前冻结 r7 Release `664dc919…` 的独立资源克隆，仅将 fragment `postUv` 从 vec2 改为 vec3、采样保持 `.xy`，vertex 仍输出 vec2。PID76207 隐藏菜单自然 exit0、1.1200526秒，实际日志进入 `active=legacy fallback=1 reason=forced`。这是真实资源错误被强制回退隐藏的 FAIL，原件保留在 `build/reference-visual-implementation/hdr-link-fallback-calibration-r1/case-r7-r2`。

修复源码 `HdrPipeline.cpp` SHA `c72d5270b3e0aa4c7c24f614921ea47236fe96a08e58b0f0f6d81d0e204b05d3`。根代理实际双配置客户端构建通过；当前 r8 Release executable SHA `50b0615880b5b16f8aba4958e2a53f48cf9ab725466ebd78c89f4366bdab3137`，source manifest SHA `551927627fc40e5c84b274140bab8d4ce3e4d68374b9aebee64cce1ff98c2f14`、2989项。四个根代理串行实际案例如下：

| 案例 | 实际 PID／自然退出 | 实际结果 |
| --- | --- | --- |
| 正常 HDR | 78721／0，1.307324秒 | resolve link=1；RGBA16F4 color、depth24/stencil8四采样、单采样 resolve，实际 .18/2/8/.5 signal／GL0；2560×1440 HDR active，MSAA active时空间过滤关闭。 |
| 有效强制回退 | 78731／0，1.033424秒 | resolve link=1后正常 forced legacy；未执行浮点 FBO probe或目标安装。 |
| 同一坏链接＋强制回退 | 78742／1，.719245秒 | link=0，driver明确报告 postUv 类型／qualifier不匹配；FBO probe与forced fallback之前拒绝。该原生结果仍是非零退出，记为负控被拒绝。 |
| 完整旧 AA-less resolve＋强制回退 | 78773／0，.971197秒 | 使用历史冻结的完整旧 program/fragment，实际 link=1并正常 forced legacy；不要求新增 AA uniform，不虚构本路线未输出的 available 日志。 |

四次实际证书的临时 program 均已删除、current program 0→0、GL error 0→0；三个有效案例的菜单 terrain 为0，坏链接在 terrain 构建前拒绝。四次输入0，未创建 World/chunk。独立审计 `build/reference-visual-implementation/hdr-link-current-r8-calibration-r1/current-native-audit-r1.json` SHA `dd488bf99d4ed6d13394a1bf3b6a5b2ee8d357de53a7ab8fc9bc2fe520254b9a`，164项身份／日志／资源观察通过；三个有效案例与一个真实拒绝负控分开记录。原 r7 与 r8 使用逐字节相同的坏 fragment。

每个案例仅运行验证后168文件源包的独立克隆，实际 lsof 证明加载当前 copied executable，15秒自有 child watchdog；原源包、当前 HDR 源、10个既有发布包2197文件均保持。案例与旧失败从不覆盖；进程清理只作用于自有 Popen。以上是隐藏工程启动证据，没有普通输入、World保存重开、窗口resize、完整长期生命周期或性能隔离声明；普通12条路线继续单独验收。
