# E3 terrain v9 内陆林缘与草甸开窗执行记录

状态：`Engineering Done / 自动与窗口证据 PASS，正常玩法按用户要求延期`。实际起始 HEAD
`699eacf491955da9eff297d55bbaff1a13b85ef7`，起步工作区干净。E2 同进程批量验收
代码及真实性能 FAIL 已另做本地**检查点**提交，不是 E2 完成版。E3 合同和 v8 冻结身份见
[E3 合同](../contracts/ecology-inland-meadow-v9-e3-contract-v1.md)、
[冻结清单](ecology-e3-v8-production-baseline-2026-09-16.json) 与项目内
`build/ecology-e3-20260916/baseline/v8-baseline-manifest.json`。

## 本批实现

新世界默认 terrain v9。`TerrainFoundation::sampleV9` 先取完整 v8 列，仅在高度
81..134 的疏林/温带林内用 seed 与世界 XZ 的两个连续低频轮廓打开草甸；高度、海岸、
沙漠、山地、原有草地保持 v8。草甸列规划为 Grassland/Grass，生成区块、公开高度/生态
查询、树木、草花、洞口和地点规划走同一版本列。草甸树木锚点被跳过，让玩家能穿行；
旧 v1–v8 仍有独立采样路径和保存身份。存档格式、方块、区块加载、水面、渲染不变。

## 当前自动证据

| 项目 | 结果 | 原始证据 |
| ---- | ---- | -------- |
| 旧版 v1–v8 生产身份 | `PASS` | `production-debug-final/v1..v8/` 的真实 C++ 采样/区块 CSV 与 E2 候选 23 的 16 项 SHA-256 逐字节一致；`production-debug-final/validation.json`。 |
| v9 地形与生态规划 | `PASS` | 同报告 463056 个生产样本：v8/v9 高度变化 0、非目标生态变化 0、最大坡度 3；48,864 个内陆森林采样中 13,329 个开窗，比例 `27.278%`，在冻结的 5%..30% 内；32 个固定生成区块中 1 个真实方块指纹变化。 |
| 定向真实草甸区块与地点覆盖 | `PASS（原始过严谓词 FAIL 保留）` | 从已冻结生产采样[选定 16 个区块](ecology-e3-meadow-chunk-sites-2026-09-16.json)：八 seed 各一组正/负坐标，4096 个目标列、16/16 真实生成区块指纹变化，高度变化 0、草甸树根 0；Debug/Release 的 `sites.csv`、`columns.csv`、`plans.csv` 三表逐字节一致。最终 52 个非草块顶层全是两座 v9 RaiderCamp 的 Dirt 地基，地点覆盖外为 0。原冻结“最终草甸地面必须全是 Grass”给出 `FAIL`，原件不删；[判定说明](ecology-e3-meadow-chunk-adjudication-2026-09-16.json)保留该误判并要求规划地表 Grass、无地点覆盖处的最终地面 Grass。[独立汇总](/Users/lizi/Desktop/Workspace/HelloMine3D/build/ecology-e3-20260916/targeted-production-validation-r2.json) `PASS`，SHA-256 `5c4044eec19fb0a0d0b442fd154f8095b2b0c7182754f12b10998aefd1ad2268`。 |
| 连续性、边界、树花、出生资源/地点与存档 | `PASS` | Debug/Release `E3_MEADOW` 最终各 24/24；定向网格比例 `21.036%`，八 seed 中七个找到至少 16 格连续开窗，八 seed 正逆区块生成一致，真实区块检查 268 个开窗列的树花承载，v9 新世界玩家改块保存重开；八 seed 自然出生、安全落脚、木石煤干沙草及三类地点确定/可接近 PASS。最终 `e3-grassland-final-focus-{debug,release}.log`。 |
| v8 天然草地稀疏树木不被 v9 开窗规则抹去 | `PASS` | v8 生产采样中 seed 0 的连续高草地真实生成区块找到橡树树根 `(1815,574)`，v9 同一树根仍在；Debug/Release 的原始聚焦日志 SHA-256 分别为 `2b11ec70e8e511fd22c438e10ce29c6235ba00976e1a090da99686f3646e848a`、`01fddceb6c27f94897dfb09864c0af1d5f8e06ec2002ca7dd2cd5acd536e44cd`。 |
| 世界完整回归 | `PASS` | 新天然草地树根回归纳入完整运行后，gmake Debug/Release `HelloMine3DWorldRuntimeSmoke` 均 `1091/1091`，见 `e3-grassland-final-world-full-{debug,release}.log`；SHA-256 分别为 `cc1b0d6ea3756f6ed1cc074dd3f07be023bdb4eb57e6f6503af132b490e8794e`、`294587c32a4f192f48a8a8e53b71eed1908c79be99c6af830063c93d15cc3c5f`。 |
| 客户端双配置编译 | `PASS` | gmake Debug/Release `HelloMine3D`，见 `client-build-*.log`。 |
| 世界目录与事务存档 | `PASS` | gmake Release `WorldCatalogueSmoke` 60/60、`StorageTransactionSmoke` 18/18，见 `catalogue-release.log` 与 `storage-release.log`。 |
| 窗口性能 | `PASS` | 同一 Release 工作客户端、同一 PID 的 12 段：草甸常驻 P95/P99 为 `1.006×/1.014×`，快速流送为 `0.995×/0.979×`，均低于冻结的 `1.10`；身份、世界夹具和顺序均 PASS。比较 SHA-256 `7e0a59822e7c611cc8b039a317b76bdbbd639b2ebaf7764d14b51cdbfc3c6a77`。 |
| v8/v9 固定画面 | `PASS` | seed `20260807` 正负坐标各两版、5 秒与 10 秒共 8 张原图：v9 分别形成完整开阔草甸和沿坡可步行林缘，未见高度接缝、等宽直带或逐格噪点。视觉判定 SHA-256 `de742c078e99c90ddcc0b8558ede8001bb9591a05fe7eb6c72459b86cc56b6fb`。 |
| 中文菜单步行、采集与重开 | `NOT_RUN / 用户要求延期` | 未把固定机位或自动存档测试冒充正常玩法。 |

全部原始路径均位于 `build/ecology-e3-20260916/`；`validate_ecology_e3.py` 只聚合 C++
生产 CSV，没有复制生成算法。v8 候选 23 的原游戏与证据包及 SHA-256 收据继续封存，
本批未回写 E2 包。

## 窗口批次准备（采集前冻结）

图形会话解锁后，先冻结 E3 的真实窗口协议，再刷新唯一项目工作客户端。seed `20260807`
的正坐标草甸区块 `(1536..1551,800..815)` 在 v8 为温带林、v9 为完整草甸，256 列高度均
为 86 且没有地点覆盖；负坐标区块 `(-896..-881,-800..-785)` 同样 256/256 列转换，
高度 87..92 且无地点覆盖。固定机位、画质、版本顺序及 `1.10` 性能门槛见 E3 合同的
“窗口证据冻结补充”。批量工具以 `--profile e3` 复用既有同进程生命周期，E2 默认 phase
集合和比较规则不变；运行时只把显式诊断 manifest 的允许版本扩至 9、场景标签扩至 meadow。
计划一次启动完成 12 段性能与 4 段原图，不用分散启动的 Pilot 冒充正式证据。

## Release 窗口结果

正式工作客户端由干净提交 `e8a686f9d41278efba36924894bb2cac938dab0c` 原位刷新，109 项
分发清单 `PASS`；最终 Release 二进制 SHA-256 为
`7c306b27eeec8ca6b7ed245860fa671fef4a740d3e71675184b4f4eea202062c`。批次
`window-batch-release-r2/` 在唯一 PID `47901` 内按冻结顺序写出 32 个开始/完成事件，
16/16 phase 均 `CAPTURED`。批次状态和事件 SHA-256 分别为
`fa74609009c46c0b4e2d6f74598f66d48e3606d5ddc02773eeea00d93c478fdc`、
`cb3fbcea02d1fb49b8237c5ce74b73e747554d181e8e58cc534bd1d3c78907fc`。

三轮中位数比较的身份、夹具、顺序和两组性能全部 `PASS`：草甸常驻 P95/P99
`1.006×/1.014×`，快速流送 `0.995×/0.979×`。正坐标原图中 v8 的真实树冠碰撞让玩家
停在冠层，而 v9 在同一强制起点落到高度未改变的草甸地面；没有补偿相机或裁图。负坐标
两版都在近地视角，清楚显示密林转为沿坡展开的连续草甸。两组 v9 均保留远近树线，形成
可读的林缘；动态敌人警告和血量变化留在原图，不影响地形结论。窗口汇总 SHA-256
`8fc406aa32ed18829a6260c55622cad39c4cc61cef7dd1a213bedd9a863fc79b`；136 个文件的完整性
收据自检 `PASS`，SHA-256
`937b28c682b9d74cba1d3929ff1cdb4ea491de1fba08fa0a3cb109bf83f23b43`。

## 保留的失败与剩余风险

- 首次阈值的 `production-debug/validation.json` 报草甸转换 `33.389% FAIL`；第二次
  `production-v9-r2/samples.csv` 为 `30.149% FAIL`。两次原始样本、区块 CSV 和
  定向日志均保留，最终阈值重采整套 v1–v9 后为 `27.278% PASS`，未调宽门槛。
- E2 回归初跑 `e2-regression-debug.log` 中八 seed 出生 FAIL，原因是测试夹具给
  v8 预设存档写了有效但位于原点的玩家状态，导致未触发出生点规划；改为
  `hasPlayerState=false` 并使用对应 seed 后 `e2-regression-debug-r2.log` 八 seed
  出生/木石煤沙草及三类地点均 PASS。原 FAIL 不删除。
- 天然草地树木复核的首个夹具错误地要求 v8 高草地列是
  `Surface::Grass`，`e3-grassland-before-fix-focus.log` 因找不到这样的列而
  `FAIL`；生产地表规划表明该海拔的天然草地保留 `Original/Dirt/Sand`，
  `Grass` 是 v9 新开窗的明确表层。修正夹具后从真实 v8 区块找到稀疏树根，
  v9 对照 PASS；**生成规则没有因这次误判改动**。原始 FAIL 哈希为
  `7b0380cdda74d7b767cfddf8c60c3f9ff02655aa14d9556c429dab128be4d076`。
- 定向草甸区块首轮原冻结“4096 个转换列的最终顶层全是 Grass”在
  `targeted-production-debug/` 记录 52 格 `FAIL`，原始 `sites.csv` SHA-256
  `b63e6fcfea1676d1fb6121b1871784885db4123bf3a72e5f4fdf2995e1ca12e9`。
  追加**生产 C++ 地点规划**后发现 seed 0、424 的 v9 RaiderCamp 分别覆盖 8、44 格；
  52 格全为合法 Dirt 地基，覆盖外 0。原冻结文件未改，另存判定说明 SHA-256
  `a6f4cbec5ff1f27a87b0c6fbe3e086379a8ebc3f8812e5e5ffac46353b558068`，
  保留初次 FAIL、Debug/Release 三份原始表及修正后的独立校验。没有因为误判改
  地形或营地规则，也没有放宽 16 区块、12 区块变化、高度与树根门槛。
- 首次窗口批次 `window-batch-release-r1/` 的同一 PID 完整跑完 16 段，但 summary 报
  `build_configuration=Debug`，与正式 Release 身份不符，整批 16 项均判 `FAIL`。原因是
  Debug/Release 共用输出路径，较新的 Debug 可执行文件让普通 Release 增量构建没有重新链接。
  失败二进制、批次状态、事件和 identity 已用收据封存，SHA-256
  `775f6c092c1ed52426087d12b5cc29e1918b880f3604f8ba24ce891c16aa6e8f`；删除共享输出后强制
  Release 链接，并在刷新前用二进制字符串和 109 项清单双重确认。失败批次没有改写成 PASS。
- 32 个常规固定生成区块以非目标生态为主，只有一块指纹变化；定向 16 个区块和两处窗口
  机位补足了开窗内域与可见林缘证据，但还不是对全部 seed 的人工审美抽查。正坐标 v8/v9
  最终相机高度因真实树冠碰撞不同，负坐标对照承担近地同构图判断。E2 匹配夹具的反序岸边
  流送 P99 `1.134× FAIL` 仍独立挂账，E3 的 v8/v9 草甸性能 PASS 不关闭该问题。

本批达到 `Engineering Done`，不冒充用户人工验收通过，不推送、发布或打标签。按用户最新
顺序，正常中文菜单步行、采集与重开继续标记 `NOT_RUN / 用户延期`；E2 缺项和这项人工观察
不阻塞可执行的后续开发。
