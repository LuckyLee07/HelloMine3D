# HDR 显示修复交付

2026-10-08；基于主干 `1664f478`，在 `codex/hdr-display-fix` 实现。范围为 HDR 暗面偏黑与 HDR＋Post On 倒置。

## 结果与原因

- HDR＋Post On 曾出现世界倒置、HUD正立：共享全屏 VS 直接输出 vertex，绕过 Ogre 离屏目标方向矩阵。现在通过自动绑定 worldviewproj_matrix 变换 quad，UV不改。
- 暗面曾将旧 CPU cardinal／亮度底值重复计入 AO；现在对已知世界来源作有界 AO 估计，天空环境项由 .36 调至 .65，并补有限金属天空项。整体曝光、.035底值、火把漫射保持。
- 首轮仅改 shader 后檐下仍近黑，已保留原图：现有天光只竖直扫描，屋檐下直达天光为0。最终网格快照从六个未阻光相邻格借原始天光减3，不递归；仅零直达来源使用一跳 ambient，太阳继续原 direct，封闭洞穴不借光。
- 20³临时 shell 保证18³采样边界一致；四个 Terrain／Flora VS 解码后插值，greedy 比较有效通道，Water不编码。44B顶点、权威光、存档格式保持；编辑依赖两格、暗邻区加载/卸载均失效，Far只dirty，沿用现有构建预算。

## 必要检查

本机 macOS arm64／M1 Pro／OpenGL4.1；没有重跑完整世界、性能或普通连续输入矩阵。

| 检查 | 实际结果 |
| --- | --- |
| 最终客户端 Debug／Release | 两配置构建 exit0；复用同一 DerivedData。 |
| ResourcePack 方向资源合同 | 两配置各206/0；VS矩阵与绑定负例通过，此后该检查源文件未变。 |
| 最终 Release World HDR_DIFFUSE_SKY | 29/0，包含已有 LOCAL_LIGHT_RENDER、阻光/封闭空间、接缝、真实来源编码、水面、编辑/加载/卸载和 Far队列。 |
| 原生 CGL surface shader／真实mip | 3664/0，四生产VS的解码与插值、材质/金属补光及旧故障校准通过。 |
| 原生 CGL resolve／Post 方向 | 858/0，覆盖 legacy＋Post、HDR Off、HDR＋Post 链；旧 VS 负控复现倒置。 |
| 最终真实 Ogre 实景 | 三次 CAPTURED／native0／stderr0／reaped，无超时；檐下纹理可辨、世界与HUD正立、夜间仍暗且灯光保持暖色。 |

最终 Release SHA256：`e97d7e7247ef61373f3c3be87abfe60fd21c181c467c49e2444f66b6dd62a338`。
最终 Debug SHA256：`982160f911c41ef4a01ca0d03f53198e99436cee92a6bfa7c6fd15f8b5231a4b`。

构建实际入口为各目标 Xcode project/scheme；客户端使用 `xcodebuild -quiet -parallelizeTargets -project build/HelloMine3D/HelloMine3D.xcodeproj -scheme HelloMine3D -configuration Release -arch arm64 -derivedDataPath build/hdr-display-fix-20261007/DerivedData ARCHS=arm64 ONLY_ACTIVE_ARCH=YES CODE_SIGNING_ALLOWED=NO build`，Debug仅换configuration。
World实际运行：在bin下 `HELLOMINE3D_WORLD_SMOKE_FOCUS=HDR_DIFFUSE_SKY ./HelloMine3DWorldRuntimeSmoke`。
[构建及检查日志](../../build/hdr-display-fix-20261007/logs/)、[GPU原始结果](../../build/hdr-display-fix-20261007/gpu/)保存对应配置、输入与结果。

三次最终实景从同一既有样板存档的隔离副本恢复：seed42，RD3、FOV90、medium shadow、HDR standard、MSAA4、2560×1440。
Post On使用旧失败机位 `(208.5,68.02,-169.5)`；近景Off与夜景使用 `(211.5,68.02,-181.5)`，rotation均`(-8,0,0)`，昼6000／夜18000，5秒读回。这是隐藏工程静帧，不是正常输入验收。
[旧倒置原图](../../.local-evidence/hdr-display-fix-20261008/before-hdr-post-on.png)、[修复后Post On](../../.local-evidence/hdr-display-fix-20261008/after-hdr-post-on.png)、[修复后近景](../../.local-evidence/hdr-display-fix-20261008/after-hdr-street.png)、[夜景](../../.local-evidence/hdr-display-fix-20261008/after-hdr-night.png)。完整捕获记录在对应build目录，精选SHA见[index](../../.local-evidence/hdr-display-fix-20261008/index.json)。

失败不覆盖：首次sandbox无法建立加速GL，转原生上下文后通过；World r2/r3/r4分别为fixture缺高空段、Far高度域和Preload租期错误，原FAIL日志保留，最终只改fixture，没有删除断言或放宽容差。客户端生产代码冻结后，后续仅改World测试fixture。

## 交付边界

本地合入主干，不推送或打标签。新入口为 `bin/development/HelloMine3D HDR Fix.app`，原入口及数据保留；新包预设linear-hdr／Post Off。主干bin客户端同步为以上Release。既有7项未提交文档逐SHA保持。
一跳补光是当前檐下缺口的有界近似，不是完整GI或宽檐/深室内光传播；任意旧自定义VS读取新增负来源没有通用兼容保证。Windows及普通动态路线未在本轮执行，其他迭代范围未启动。
