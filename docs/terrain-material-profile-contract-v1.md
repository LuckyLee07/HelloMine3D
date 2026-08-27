# HelloMine3D 地形材质参数合同 v1

本文冻结 Stage 10 `V10B1` 的地形图集、tile、颜色修正、资源包覆盖和启动失败边界。
合同版本只描述运行时资源接口，不写入世界、玩家或设置存档。

最后更新：2026-08-27。

## 状态与范围

- Windows 实现、VS2017/v141 Debug/Release 门禁、真实 Ogre/GLSL 启动、固定截图和开发者检查
  已完成，V10B1 工程状态为 `Done`。
- 本批修改了 terrain fragment shader；当前机器没有 macOS 目标，因此 macOS Release 编译和
  真实窗口冒烟保持 `Verify`，不得写成 `PASS`。该跨平台子状态不阻止 Windows 上继续 V10B2，
  但必须在 VISUAL-RC 前补齐。
- 本批只建立参数管线并保持旧画面兼容，不制作正式新图集，不改变 save v11、terrain v3、
  settings v4、32 字节 terrain vertex 或性能 comparison schema 3。
- V10A 的 AO 强度、`shapedLight` 最暗值 `0.24`、environment exposure 与最终合成顺序保持不变；
  V10A 的性能例外不扩展到本合同。

## 权威资源

权威参数文件为 `media/materials/Base.terrain-material`，首行必须精确为：

```text
# HelloMine3D terrain material parameters v1
```

除空行和 `#` 注释外，文件必须且只能包含下面八个不重复的 `key=value`：

| 参数 | 类型与范围 | v1 默认值 | 语义 |
| ---- | ---------- | --------- | ---- |
| `atlas_texture` | 固定逻辑路径 | `media/textures/DefaultPack.png` | v1 不允许任意文件路径；资源包可覆盖该逻辑路径所指向的内容。 |
| `atlas_pixels` | 整数 `[16,8192]` | `256` | 正方形 PNG 的宽高像素数。 |
| `tile_pixels` | 整数 `[2,atlas_pixels]` | `16` | 每个正方形 tile 的边长。 |
| `tiles_per_row` | 整数 `[1,256]` | `16` | 每行/列 tile 数。 |
| `colour_saturation` | 有限浮点数 `[0,1]` | `0.62` | 原色与亮度灰度之间的混合比例。 |
| `green_suppression` | 有限浮点数 `[0,1]` | `0.22` | 对超出红/蓝的绿色分量进行抑制。 |
| `green_red_shift` | 有限浮点数 `[0,1]` | `0.07` | 把部分绿色超额移入红色。 |
| `tone_gamma` | 有限浮点数 `[0.5,2]` | `1.05` | 颜色修正后的逐通道幂指数。 |

同时必须满足：

1. `atlas_pixels % tile_pixels == 0`；
2. `tiles_per_row == atlas_pixels / tile_pixels`；
3. 有效图集必须是可读的 PNG/IHDR，宽高都精确等于 `atlas_pixels`；
4. 每个方块的 top/side/bottom atlas 坐标都在
   `[0, tiles_per_row - 1]` 的二维范围内。

未知 key、重复 key、缺失 key、空值、尾随数据、非有限浮点数或不支持的 header 一律拒绝，
不静默回退到默认值。

## 冻结与启动顺序

启动顺序固定为：

1. 从 `media/resource-manifest.txt` 加载 65 项基础要求；
2. 冻结资源包优先级和 effective resource view；
3. 对缺失或空资源执行通用 startup preflight；
4. 从同一个 effective view 解析并冻结 terrain material profile；
5. 再构造方块和其他数据注册表，使方块 tile 坐标使用已冻结的动态范围；
6. Ogre 初始化资源组后，把同一份 profile 同步到 solid、transparent 和 flora 三个材质 pass。

进程内 profile 只允许冻结一次。headless 代码在没有 Ogre bootstrap 时使用兼容默认值；真实客户端
不得在资源包冻结后重新加载或热改 profile。

失败信息必须指出资源路径、参数名或越界范围。`tools/validate_startup_errors.ps1` 使用真正的
Windows 客户端覆盖资源缺失、非法分格、合法缩小分格导致的方块 tile 越界，以及 shader 接口
漂移；这些失败发生在进入可玩世界前，并写入 Windows startup error report。

## CPU/GPU 坐标兼容

CPU 侧 `BlockTextureCoordinates::get` 使用：

```text
tile_span = 1 / tiles_per_row
pixel = 1 / atlas_pixels
min = tile_index * tile_span + 0.5 * pixel
max = min + tile_span - pixel
```

GPU 侧按 `tileIndex * tilePixels + 0.5 + fract(repeat) * (tilePixels - 1)` 计算 atlas 像素中心，
再除以 `atlasPixels`。两端都避开 tile 边界采样，并由同一份 profile 驱动。

默认 `256/16/16` 的首 tile 与末 tile 坐标保持旧实现逐值兼容；另有 `512/32/16` 夹具证明
参数变化后仍使用正确的半像素中心，而不是残留 `256` 或 `16` 魔数。

## Shader 接口

`HelloMine3D/TerrainFragment` 必须声明并实际使用七个浮点 uniform：

```text
atlasPixels
tilePixels
tilesPerRow
colourSaturation
greenSuppression
greenRedShift
toneGamma
```

`HelloMine3D.program` 保留与 profile 相同的默认值，使脚本单独解析时也有确定兼容值；Ogre
bootstrap 随后把冻结的 effective profile 同步到 `HelloMine3D/Terrain`、
`HelloMine3D/Transparent` 和 `HelloMine3D/Flora`。缺少任一声明会在启动预检中失败，不能等到
首帧才以黑材质或驱动日志静默降级。

## 资源包语义

`material-profile` 是允许覆盖的资源类别。资源包可以覆盖 profile、固定逻辑路径下的图集和
terrain shader，但最终 effective view 必须作为一个整体通过上述检查：

- profile 与 PNG 尺寸不一致时拒绝；
- profile 与 shader uniform 接口不一致时拒绝；
- profile 缩小 tile 范围但方块定义仍引用高坐标时拒绝；
- 未覆盖的资源继续从基础包解析，覆盖顺序保持既有 deterministic precedence。

V10B2 可以替换 `DefaultPack.png` 的实际像素并保持本合同；若未来需要多 atlas 路径或非正方形
图集，必须升级合同版本，不得在 v1 下放宽解析。

## 2026-08-27 验证证据

- VS2017/v141 Debug/Release 的客户端、ResourcePackSmoke 和 WorldRuntimeSmoke 受影响目标均
  构建成功。双配置资源包回归为 `54/54`，`V10B1` 世界聚焦为 `4/4`；Release 完整世界回归为
  `718/718`。
- 资源清单检查为 65 项；真实 Release 隐藏客户端打印
  `[TERRAIN_MATERIAL] frozen=1 version=1 ...`，GL3Plus 成功解析 program/material、加载
  `256x256` 图集并退出 0。
- `validate_startup_errors.ps1` 覆盖通用缺失资源与 V10B1 非法 profile、方块 tile 越界和 shader
  接口漂移，全部返回预期的非零退出码和 Windows error report。
- `docs/screenshots/validation-v10b1-default-forest-noon.png` 使用 seed `296595`、位置
  `(2766,102,2905)`、旋转 `(20,118.4,0)`、世界时间 `6000` 的 Release RuntimeReadback。
  与 V10A AO 基线比较时，右侧静态前景区域 216,000 个像素完全相同；全图 6,575 个差异像素
  位于 FPS、队列等动态 HUD 文本。默认材质路径据此判定像素兼容。
- 一次短暂可见 Release 窗口确认约 60 FPS、完整远景、AO、tile 边界和透明植被正常；随后使用
  完全隐藏的真实 Ogre 窗口执行 5 分钟连续观察和定时读回，避免再次打断桌面焦点。可见窗口的
  `ShowWindowNoActivate` 只能 best-effort，今后的前台检查必须先取得用户明确同意。
- Debug/Release 共用 `bin` 会让普通增量构建在 Debug 覆盖 exe 后误判 Release 已是最新；本次
  通过强制 `Release Rebuild` 恢复 8,566,784 字节客户端，并以窗口内 `Release` 标识复核身份。

## 后续边界

- V10B2 只改原创资产、生成脚本、manifest、许可和材质分面，不改变本合同的颜色默认值或
  V10A 光照曲线。
- V10B3 才引入受控生态 tint 与坐标确定性 tile 变体；变体必须进入 greedy merge key。
- VISUAL-RC 使用最终资产和 shader 身份重跑相关 Q1/Q3、完整 macOS 门禁和正式产品体验；
  本批的固定截图与开发者检查不替代最终人工产品验收。
