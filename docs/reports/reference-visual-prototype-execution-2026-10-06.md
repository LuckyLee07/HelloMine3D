# 目标画质首版执行记录

## 结果与范围

2026-10-06：**Engineering Done**。从 master 的 `7b39402bb0eda4ec8e9af035601a41f2fa319324` 建立独立工作树和 `codex/reference-visual-v1` 分支，实现独立线性 HDR、石台阶和石窗框技术样件、可编辑保存的临水小屋。主工作区保持 master，本分支尚未提交、推送或合并。

首版仍使用现有石、木、道路和水面资源，画面保留原有方块风格。完整法线／粗糙度材质、建筑套件、真实水面倒影和参考图最终审美属于后续范围；不能用本次技术基础及截图声明目标画质已实现。语义见[首版合同](../contracts/reference-visual-prototype-contract-v1.md)，后续见[实现方案](../current/reference-visual-upgrade-plan-2026-10-06.md)。

## 查看版本

应用包在工作树的 `build/reference-visual-v1/HelloMine3D Reference Visual.app`。双击打开，在普通主菜单点“继续游戏”，存档名为“临水首版样板”。已在最终包副本中观察到正常主菜单显示该入口；普通键鼠点击及行走仍未验收。

样板库存前两格分别有 32 个石台阶、32 个石窗框，另有镐、火把和木板。庭院展示部件四向，小屋内含工作台、熔炉、箱子和火把。样板由正常 World 编辑及保存持有，重开不会再次注入或重建。新部件当前供技术试用库存，未增加制作配方或整套建筑美术。

包默认启用线性 HDR、中等太阳阴影、可选后处理 Off、标准视觉详情、中文及第一人称。设置中的渲染管线可切到旧版，保存后重启比较。旧配置迁移为 legacy；世界保存仍为 v12，设置格式升级为 v12。

## 实现

- `HdrPipeline` 持有独立 FP16 场景目标和 resolve。地形、阴影接收体、角色、水、天空、方块反馈、碎屑与洞边界进入线性域；曝光固定为 1，输出经 tone mapping 与显示编码，HUD 在主窗口末绘。数据纹理和 Alpha 不作颜色转换，旧 post Off／On 保持。既有颜色贴图采用显式解码，旧贴图的 mip 生成／滤波仍是兼容路径，未声称完整物理材质。
- 启动先展开宏并调用 Ogre GLSL backend 的实际 stage 编译。坏 shader 明确失败；HDR 专属能力或浮点目标不足、预算超限可退回 legacy。原生 float probe 验证大于 1 的信号，场景目标上限为 3840×2160 像素。固定 RTT 尺寸防止 Ogre 在 resize 回调中提前越过预算；交互 resize 与长循环仍待验证。
- shape v2 限制为单格内最多 8 个复合盒，边界按 1/8 m 离散。新增 BlockId 33／34 和 Material 49／50，旧 ID、shape v1 和旧 metadata 不变。新部件 metadata 0–3 保存四向；网格、真实盒碰撞、选取、反馈、手持、掉落与导航共用几何语义。窗洞可选中后方，部分形状不按整格剔除或实心层处理。
- 样板仅接受新的空存档目录。33×33 m 庭院、临水工坊和四向展示共 18499 次有界编辑；通过正常 `World.setBlock` 和保存落盘。已有存档注入被拒绝，原文件保持。

## 工程证据

环境为 macOS 15.7.3、Apple M1 Pro、arm64 原生客户端，OpenGL 4.1。未执行 Windows 验证。日志及原图保留在工作树 `build/prototype-validation/`；原图未加入 Git。

| 检查 | 结果与证据 |
| --- | --- |
| 客户端最终 r5 Debug／Release 构建 | PASS，两配置项目源码警告均为 0；`Debug_HelloMine3D_build_r5.log`、`Release_HelloMine3D_build_r5.log`。第三方既有警告保留 |
| WorldRuntimeSmoke 双配置构建 | PASS，`Debug_WorldRuntimeSmoke_build_r4.log`、`Release_WorldRuntimeSmoke_build_r4.log` |
| Release 完整世界回归 | PASS，2887 checks／0 failures，`Release_World_full_run_r4.log` |
| HDR 设置相关 Debug／Release | PASS，各 51 checks，迁移、往返、取消、重启语义和坏配置拒绝 |
| 新形状世界相关 Debug／Release | PASS，各 36 checks，碰撞、洞内选取、四向、编辑失效、掉落支撑、导航和保存重开；`Debug_REFERENCE_SHAPE_run_r4.log`、`Release_World_REFERENCE_SHAPE_run_r4.log` |
| Debug 导航相关 | PASS，72 checks，`Debug_NAVIGATION_run_r4.log` |
| 独立形状 Debug／Release | PASS，各 777 checks；`build/reference-shapes-v1-debug-r4/`、`build/reference-shapes-v1-release-r2/` |
| ResourcePackSmoke Debug／Release | PASS，各 201 checks；`Debug_resources_run_r4.log`、`Release_resources_run_r1.log` |
| Release MeshDirty／SaveLoad／Recipe | PASS；`Release_mesh_run_r1.log`、`Release_save_run_r1.log`，配方 126 checks／0 failures |
| 资源及图集 | PASS，95 项资产；全量 34 个方块定义／51 个材质验证保持旧像素与身份，`assets_r4.log` |
| 独立生产 HDR GPU | PASS，814 checks／0 failures、GL error 0；`hdr-gpu-r1/receipt.json`、`summary.txt`。包括完整 resolve、多曝光单调性、透明线性混合、颜色／数据分离和 5 个可执行坏 shader 校准 |
| legacy GPU 回归 | PASS：水面、阴影 108、角色 200、反馈 413；`legacy-*-gpu-r1.log` |
| 既有存档注入保护 | PASS，明确退出 1，35 个文件哈希不变、未创建目录账本；`final-existing-save-guard/receipt.json` |
| 最终坏 HDR shader 原生负例 | PASS，最终 r5 包副本追加非法 GLSL，强制回退同时打开；实际 driver compile 报错、退出 1、没有 legacy 回退；`final-invalid-hdr-shader-r2/receipt.json` |
| 最终包及源码一致性 | PASS，143 个托管文件、2974 个源码文件无缺失或哈希变化；`final-package-identity-r5.json` |

r5 最后一处修复只修改 Ogre HDR 启动的实际 stage 编译，随后重建客户端并重跑原生健康／坏资源／回退观察；World 核心与独立 GPU 输入未变。814 样本的 20 个 shader 输入哈希与最终源码一致，因此未无理由重跑完整世界和独立 GPU。stage 编译预检不包含所有 stage 接口的提前 link 校验。

既有 `verify_xcode.sh` 总门禁未运行：其中完整世界固定计数仍为旧 2118，本次按实际目标构建和当前输出核验，未放宽或改写旧断言。配置切换时 Xcode 重建了第三方依赖，其日志与第一方警告分别记录。

## 最终原生画面与回退

最终画面共 9 个隔离运行、17 张原图，另有普通主菜单 2 张原图。统一 Release 二进制、固定镜头／时间；常规观察使用 1280×720 窗口点、Retina 2、实际 2560×1440、中等阴影和标准详情。索引为 `final-capture-index-r5.json`，每次的 `capture.json` 记录完整命令、配置、环境、二进制及图像哈希。全部为后台诊断，`normal_input=false`。

| 场景 | 实际证据 |
| --- | --- |
| 室外同机位 HDR／legacy | `r5-street-hdr/`、`r5-street-legacy/`，均 CAPTURED；已人工查看 HDR 原图 |
| 室内同机位 HDR／legacy | `r5-interior-hdr/`、`r5-interior-legacy/`，均 CAPTURED；已查看 HDR 原图 |
| 四向台阶及窗框 | `r5-details-hdr/`，CAPTURED，四向轮廓和窗洞可见；已查看原图 |
| HDR 加可选 post On | `r5-hdr-post-on/`，CAPTURED |
| 强制能力回退 | `r5-hdr-forced-fallback/`，CAPTURED，日志 `active=legacy fallback=1 reason=forced` |
| 目标预算回退 | `r5-hdr-budget-fallback/`，CAPTURED；5120×2880 请求在场景 RTT 分配前拒绝，`allocated=0 allowed=0`，随后 legacy |
| 保存普通数据重开 | `r5-reopen-street/`，CAPTURED，从已保存世界加载，无样板注入；不代表正常菜单输入验收 |
| 普通主菜单绘制 | `r5-preview-menu/receipt.json`，退出 0、2 张图、无 save override／scene injection；已查看“临水首版样板”继续入口 |

实际 Ogre float probe 读回 `0.180054,2,8,0.5`，RGBA16F、24 位深度，PASS。2560×1440 HDR 场景颜色目标为 29491200 bytes。HUD、场景和室内在原图上可见，无明显破图；截图不能证明连续输入、动态舒适度或长期资源稳定。

## 保留的失败及修复

1. 初次 Release 构建错误：样板调用私有 preload 方法。改用已有公共入口，首次失败日志保留为 `Release_HelloMine3D_build.log`，后续双配置构建通过。
2. 完整世界 r3 出现两项本地化失败：新增材质缺翻译且固定数量断言过旧。补齐中英两键及对应精确断言，r4 全量 2887／0；`Release_WorldRuntimeSmoke_full_r3.log` 保留。
3. 首次坏 HDR shader 原生校准 FAIL：Ogre `load()` 只预处理，forced fallback 绕过真正编译，客户端继续进入菜单。仅终止该隔离测试进程，记录退出 -15；`final-invalid-hdr-shader/` 保留。修复为实际 `GLSLShader::compile(true)` 后，r2 明确失败、无回退，校准 PASS。
4. 较早的重开截图使用默认林地位置，落到样板外；该设置错误的运行保留。最终 `r5-reopen-street` 明确使用保存样板位置。早期 r4 原图索引也保留，不冒充最终二进制画面。

## 未覆盖项

**普通输入验收 BLOCKED**：Computer Use 初始化连续失败，最终错误为 `Sky Computer Use service startup request failed`。因此未声明普通主菜单点击、连续行走、采挖／放置、设置重启操作或编辑后正常退出重开的 AI PASS；本次未用脚本合成按键替代。

实际交互 resize、反复切世界／重启的长循环、长时间资源增长、Windows 和音频验收为 NOT_RUN。严格性能沿用既有延期，人类审美／乐趣／手感为 NOT_CLAIMED。现有工程碰撞、导航及保存往返与原生诊断证据支持可交付技术首版，未关闭这些独立验收项。

## 冻结包身份与复现

- Release 游戏可执行 SHA-256：`fbd721d9d4e51e856c1ceba913000d06dec567ae32d31dc007edd4b533163667`。
- `source-tree-sha256.txt` SHA-256：`831420c3635dc22464eb9d45989583d4c9c1ac45897658d49e65461c7458b860`。
- `distribution-sha256.txt` SHA-256：`c2b60d16e2456a0ae9943116f2558f7969fa5cf0ad02b96e034bdab943670148`。
- 9 组最终 capture 和坏 shader r2 使用以上游戏二进制。应用启动器是单独脚本，不将其哈希误写为游戏二进制。

构建使用 `xcodebuild -workspace build/HelloMine3D.xcworkspace -scheme HelloMine3D -configuration Release -derivedDataPath build/prototype-validation/DerivedData -destination platform=macOS,arch=arm64 -quiet build`，Debug 同理。Release 文件在切换 Debug 前复制到 `build/prototype-validation/Release-binaries/HelloMine3D`。

打包入口为 `python3 -B tools/package_macos_release.py --output 'build/reference-visual-v1/HelloMine3D Reference Visual.app' --configuration Release --binary build/prototype-validation/Release-binaries/HelloMine3D --bundle-id local.hellomine3d.reference-visual-v1`；已有未验收工作包使用 `--refresh-existing` 保留可变存档。二进制及冻结资源可离开源码运行，普通样板存档另包含在包内，未修改旧玩家存档。

固定画面复现用 `tools/capture_visual_macos.py --render-pipeline linear-hdr --reference-scene street`（室内 `interior`、部件 `details`），完整其余参数见最终索引的 command。重开运行只传已保存世界、机位，不传 `--reference-scene`。坏 shader 负例只改隔离包副本，向 HdrResolve.frag 追加非法 token，设置 `HELLOMINE3D_HDR_FALLBACK=1`，等待上限 30 秒；健康资源及正式包保持。
