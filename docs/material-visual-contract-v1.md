# V10B2 原创材质表现合同 v1

## 状态与范围

V10B2 已完成 Windows 工程实现、自动证据和授权开发者视觉检查，状态为 `Done`。本批只替换
已有 atlas 像素、补齐资产合同和 HUD/手持
参数链，不改变 block/material id、玩法、save v11、terrain v3、32 字节 terrain vertex、V10A
AO 或 V10B1 shader 合成顺序。

由于自动化原生窗口在创建阶段可能抢占用户焦点，本批使用
`RuntimeReadback + HELLOMINE3D_WINDOW_HIDDEN=1` 取得真实 Release 图像。项目所有者于
2026-08-27 明确授权 Codex 承担本批视觉判断；审阅者随后逐张按原尺寸检查三张固定图。V10B2
只改变静态材质资产，不包含动画或时间稳定性退出条件，因此按开发者视觉合同以固定图关闭，
没有打开可见窗口，也不把单帧证据扩张为闪烁/运动结论。

## 材质语言

- 像素密度固定为每个 runtime tile `16 x 16`；母版只提供可追溯生成源，最终图集必须由
  `tools/build_fs3_texture_atlas.ps1` 最近邻裁切，禁止手改 `DefaultPack.png`。
- 表面细节以运行时 1-3 像素的色块为主，避免把单像素随机噪声作为主要结构。草/叶使用橄榄与
  森林绿，土使用暖棕，石使用冷中性灰，沙使用暖浅赭，木使用金棕，水使用低频水平波纹。
- 明度按深色强调、阴影、基色、高光、发光强调五级组织；普通地形不追求满量程黑白，路标核心
  可以使用青色发光强调，但不得让铁矿、工具或玻璃失去身份。
- `opaque` 地形裁掉母版展示用外框，并由构建器合成到语义填充色上，最终 256 个像素全部不透明；
  连续地表不允许出现黑框棋盘。
- `cutout` 植物/树叶与 `icon` 物品使用阈值化一像素轮廓，最终只能有 Alpha 0/255；
  `translucent` 水/玻璃保留有界半透明。未声明的 219 个 tile 必须全透明。
- 顶部/侧面/底面身份由方块定义固定，不使用视角相关猜测；世界、热栏、容器和手持物共享
  `Material::iconCoordinate` 与 V10B1 冻结 profile 的 atlas/tile 尺寸。

## 图集布局合同

机器可读权威为 `media/materials/Base.terrain-atlas`。它固定 37 个互不重复的
`semantic|x|y|alpha|fill_rgb|en-US|zh-CN` 条目：

- 第 0 行：草顶/草侧/土/石/树皮侧/年轮/叶/沙/水/仙人掌/玫瑰/高草/枯灌木/煤/铁/路标；
- 第 1 行：箱子/工作台/熔炉/有框玻璃/无框玻璃/木板；
- 第 2 行：种子、小麦、木/石/铁工具、铁锭、面包、三把剑和五个 N10 资源经济图标。

关键分面保持：

| 方块 | top | side | bottom |
| --- | --- | --- | --- |
| Grass | `grass_top (0,0)` | `grass_side (1,0)` | `dirt (2,0)` |
| OakBark | `oak_bark_top (5,0)` | `oak_bark_side (4,0)` | `oak_bark_top (5,0)` |
| 其余实心/透明 cube | 对应 `TexAll` | 同左 | 同左 |

遗迹和营地不新增仅用于画面的方块或 terrain 身份。遗迹由 stone/iron/glass/chest 组合验收，
营地由 dirt/oak bark/oak leaves/coal/chest 组合验收；路标继续使用唯一的青色核心。

资源包 v1 可以覆盖 `DefaultPack.png`，但必须保持 v1 坐标和 V10B1 尺寸合同；
`Base.terrain-atlas` 刻意不可覆盖，防止图集布局与 block/HUD 代码在运行时分叉。缺失 layout 在
Ogre 构造前失败，尝试从资源包覆盖 layout 会以 `stale or unsupported override` 失败。

## 生成链与来源

当前运行链：

```text
V10B2 imagegen 透明母版 + N10 经济图标母版
  -> Base.terrain-atlas（坐标、Alpha、填充色、双语名）
  -> build_fs3_texture_atlas.ps1（最近邻、Alpha 量化、完整性检查）
  -> DefaultPack.png（256x256，16x16 tile）
```

V10B2 使用 Codex 内置 OpenAI 图像生成工具，没有使用 CLI/API key。提示词分三次单点迭代：

1. `precise-object-edit`：以旧 FS3 母版为编辑目标，在不复制 Minecraft、Luanti、Minetest 或
   任何纹理包的约束下，按既有三行语义顺序重绘为中低饱和、五级明度、1-3 像素色块、左上
   统一光向的原创像素材质；禁止文字、商标、签名和水印。
2. `precise-object-edit`：只调整占用区域的缩放与间距，让全部首行语义和最右路标落入安全边距；
   不重绘、不改色、不增删、不换序，暂时保持背景不变。
3. `background-extraction`：只把棋盘格/空白转为真实 Alpha，保持所有精灵的像素、轮廓、颜色、
   位置、尺寸和顺序；禁止 matte、halo、文字和水印。

生成源 SHA-256：
`177AFDB8247C5D8E3ED5888CD00B335BB605DB47E503E0BB77533D8D291623CD`。

最终 `DefaultPack.png` SHA-256：
`82A592F3EA957B40D6EDE041EF92DD834A875C064AA847C33D5CFB53D82D933D`。

许可证清单为 `media/textures/LICENSE-HelloMine3D-Textures.txt`，SHA-256：
`1D91CDD12F1A0F66B802AE751B3364116A6C5366EE80C126EAF612463D428B1C`。这些资产为本项目
定向生成/加工的原创输出，没有复制参考项目源码或第三方纹理；外部素材未来只有在作者、链接、
许可证、修改/分发权和版本哈希齐全后才能进入发行包。

## 自动证据

- `tools/validate_terrain_atlas.ps1`：106/106；严格 header/字段、37 个坐标/双语名、四类 Alpha、
  37 个唯一 tile、219 个空 tile、22 个 block 分面、34 个 Material 图标、HUD profile 接线和
  确定性重建均通过。
- 资源清单：67 项；缺失 atlas layout 的真实客户端启动负例使总数增至 13 类。
- VS2017/v141：受影响客户端和 ResourcePackSmoke 的 Debug/Release 均通过；Release
  ResourcePackSmoke 56/56，WorldRuntimeSmoke 718/718。
- `tools/validate_resource_packs.ps1`：resolver 28、startup 2、entries 67 全通过；layout 覆盖被拒绝。
- `tools/package_windows_release.ps1 -SkipRealWindow`：85 文件干净包通过，archive SHA-256
  `1064966A2AB324D1A54DE5830C12076ABED36237B3C2444C51797FF920899D6A`。
- 三次隐藏真实 GL3Plus Release 启动均正常退出，日志没有实际 error/exception/failed；截图观察
  未见 tile 接缝、黑材质、透明漏色或 HUD/手持坐标漂移。

## 固定画面

| 画面 | 身份 | SHA-256 |
| --- | --- | --- |
| `validation-v10b2-forest-hud-noon.png` | seed 296595，time 6000，position `2766 102 2905`，rotation `20 118.4 0`，HUD fixture | `0B09FD97A870CBD70EED8A65D3DAA4128B927614EA5101D7645EE7B1D77A30DE` |
| `validation-v10b2-ruin-noon.png` | seed 20260807，terrain v3 ruin `(222,70,400)`，position `222 76 390`，rotation `15 180 0` | `74B62E98A19C55BDEC415BC027286A105158B24A4A4D79C34452AE40D1560BD8` |
| `validation-v10b2-camp-noon.png` | seed 20260807，terrain v3 camp `(436,102,37)`，position `436 108 27`，rotation `15 180 0` | `84BCE952FB5C7DEB14F67C647DC1F06DF913359819B5B41C300DEBBC308AD2C2` |

森林图与 V10B1 固定森林图使用同一 seed/机位，构成 before/after；FPS 数字和流送状态不参与
像素身份。遗迹图中的自然树干遮挡属于 terrain v3 固定输出，不为截图修改世界生成。

## 开发者视觉结果

- 记录：`docs/developer-visual-record-v10b2.txt`，`result=PASS`，可由
  `tools/validate_developer_visual_record.ps1 -RequirePass` 校验。
- 森林图中草/土/石分面清楚，HUD 五格图标、选中态与手持石剑身份一致；遗迹图中石材、树干、
  叶片和水边界可辨；营地图中沙、土、原木与树叶可辨。
- 三图均未见 atlas 接缝、黑材质、透明漏色、HUD/手持 UV 漂移、严重过亮/过暗或材质身份混淆。
- 近景草地与沙地仍有规则重复感，这是 V10B3 确定性 tile 变体的已规划问题，不构成 V10B2
  资产合同失败。
- V10B1 遗留的真实 macOS Release shader/窗口证据仍为 `Verify`；V10B2 没有修改 shader 或
  顶点接口，不新增独立 macOS shader 门禁。
