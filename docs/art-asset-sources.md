# 美术资源来源记录

本文记录项目内自制或生成式美术资源的来源、加工链路和许可边界，供资源回归、打包和后续替换时核对。

## FS3 历史基础体素图集

- 历史运行时资源：`media/textures/DefaultPack.png`（已由 V10B2 像素替换）
- 可追溯生成源：`docs/art-sources/hellomine3d-atlas-imagegen-source.png`
- 确定性打包脚本：`tools/build_fs3_texture_atlas.ps1`
- 生成日期：2026-08-24
- 生成方式：Codex 内置 OpenAI 图像生成工具，对项目原有 `DefaultPack.png` 做两次编辑；模式依次为 `precise-object-edit` 和 `background-extraction`。
- 生成源 SHA-256：`03cc58092a6f436a80851d2a28fbcdcfbd242b8d9c588c6853ca6f5e801667ef`
- 最终图集 SHA-256：`51ebb2cbfef8101843715707e7b15678cb1951a28d12daef93f4c3dbb7456efa`

第一轮提示词要求保留 256×256、16×16 网格和低饱和像素风格，并按既有坐标重绘草地、泥土、石头、树皮、树叶、沙、水、植物、矿石和路标；第二行补充箱子、工作台、熔炉、玻璃和木板；第三行补充种子、小麦、木/石/铁工具、铁锭、面包和武器。空余格必须透明，不得出现文字、徽标或水印。

第二轮提示词仅做背景提取：把灰白棋盘格和空白单元转换为真正的 Alpha 透明区域，保持已有图标的形状、位置、颜色和画布尺寸，不重绘、不移动、不裁切，也不增加文字或水印。

该母版和哈希继续作为 FS3 历史证据保留，不再是打包脚本的默认地形来源。V10B2 仍复用其
坐标语义，但从新的原创母版生成当前运行时像素。

本资源为本项目定向生成和加工的来源记录，没有复制 `F:\env1_trunk` 或其他第三方项目的美术文件，也没有引入第三方商标、徽标或署名素材。若未来改用外部素材包，必须在本文件新增作者、原始链接、许可证文本、允许的修改/分发方式和版本哈希；在这些信息齐备前不得进入发行包。

## V10B2 原创材质资产

- 最终运行时资源：`media/textures/DefaultPack.png`
- 可追溯生成源：`docs/art-sources/hellomine3d-v10b2-materials-imagegen-source.png`
- 机器可读布局：`media/materials/Base.terrain-atlas`
- 确定性打包脚本：`tools/build_fs3_texture_atlas.ps1`
- 独立验证脚本：`tools/validate_terrain_atlas.ps1`
- 随包许可证：`media/textures/LICENSE-HelloMine3D-Textures.txt`
- 生成日期：2026-08-27
- 生成方式：Codex 内置 OpenAI 图像生成工具；一次 `precise-object-edit` 统一材质语言，一次
  `precise-object-edit` 只修安全边距，一次 `background-extraction` 提取真实 Alpha。
- 生成源 SHA-256：`177AFDB8247C5D8E3ED5888CD00B335BB605DB47E503E0BB77533D8D291623CD`
- 最终图集 SHA-256：`82A592F3EA957B40D6EDE041EF92DD834A875C064AA847C33D5CFB53D82D933D`
- 许可证 SHA-256：`3226F73709778571AA53F7683B9C959A8E985C50B54A87144E43DAB8855EB6DD`

完整材质表现、提示词、分面、Alpha、资源包和固定截图合同见
`docs/material-visual-contract-v1.md`。最终 atlas 不直接采用生成图：构建脚本裁去方块展示外框，
对 opaque/cutout/translucent/icon 执行不同 Alpha 规则，并逐项消费 37 条布局清单；独立验证器
再重建临时图集并以 SHA-256 比对，未使用 tile 必须全透明。

## N10 资源经济图标

- 可追溯生成源：`docs/art-sources/hellomine3d-economy-icons-imagegen-source.png`
- 生成日期：2026-08-25
- 生成方式：Codex 内置 OpenAI 图像生成工具；先用 `precise-object-edit` 生成 RawMeat、
  CookedMeat、CactusSalad、TrailRation、PlantFiber 五个像素图标，再用
  `background-extraction` 提取真实 Alpha。
- 生成源 SHA-256：`7373ad0ba60d2f47d921a5fb050ffd2f6861b58160ff80da345e2d5b92cce4ae`
- N10 图集 SHA-256：`4997b668024456ebd9704f5b89cc1923f64bb047ccdc115083e3e84062e274fd`

`tools/build_fs3_texture_atlas.ps1` 现在从 V10B2 母版重建方块和既有物品单元，只从 N10 来源的
固定包围框裁切五个经济图标到第三行 x=10..14。所有 item 使用最近邻、无平滑并在 14×14
可视范围内居中；N10 的语义和坐标没有改变。

## P11-0 原创火把资产

- 最终运行时资源：`media/textures/DefaultPack.png` 的 `(6,1)` 单元
- 可追溯生成源：`docs/art-sources/hellomine3d-p11-0-torch-imagegen-source.png`
- 机器可读布局：`torch|6|1|cutout|000000|Torch|火把`
- 确定性打包脚本：`tools/build_fs3_texture_atlas.ps1`
- 随包许可证：`media/textures/LICENSE-HelloMine3D-Textures.txt`
- 生成日期：2026-08-29
- 生成方式：Codex 内置 OpenAI 图像生成工具，一次原创生成；没有使用第三方游戏贴图或商标。
- 生成源 SHA-256：`9C2FD55DF0FBE0236F6AD5BF21DDAC82D271EE9A42507CEFCD46D77592226D19`
- 最终图集 SHA-256：`FB6C4953AF8E8C6CE3DA44676595651266FECC5FDE0F0E1A32F068A1B697364B`

提示词要求在真正透明的 1:1 画布上生成一个居中的正面像素火把：窄深棕木柄、暖色琥珀/橙黄
火焰、硬边像素和清晰轮廓，适合交叉面方块与物品图标；四周留透明安全边距，不含地面、阴影、
场景、边框、文字、徽标或水印。生成源为 1254×1254 RGBA；构建脚本使用固定包围框
`496,64,279,1073`，以最近邻缩放到 14×14 可视区并用 Alpha 64 阈值固化 cutout，保证图集可
逐字节重建。

## N12A 中文界面字体

- 最终运行时资源：`media/fonts/NotoSansSC-VF.ttf`
- 随包许可证：`media/fonts/NotoSansSC-OFL.txt`
- Google Fonts 目录：`https://github.com/google/fonts/tree/main/ofl/notosanssc`
- 字体下载地址：`https://raw.githubusercontent.com/google/fonts/main/ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf`
- 许可证地址：`https://raw.githubusercontent.com/google/fonts/main/ofl/notosanssc/OFL.txt`
- 上游项目：`https://github.com/notofonts/noto-cjk`
- 上游版本：Noto Sans SC 2.004，commit `523d033d6cb47f4a80c58a35753646f5c3608a78`
- 许可证：SIL Open Font License 1.1；Copyright 2014-2021 Adobe，保留字体名 `Source`
- 字体 SHA-256：`A3041811A78C361B1DE50F953C805E0244951C21C5BD412F7232EF0D899AF0DA`
- 许可证 SHA-256：`1C05C68C34F9708415AADA51F17E1B0092D2CEA709BF4A94CD38114F9E73D7D9`
- 下载日期：2026-08-25

文件直接取自 Google Fonts 官方 `notosanssc` 目录，未修改字体名称、轮廓或表结构。项目只在
运行时构建 ImGui 字形图集；字体文件本身保持原版并与 OFL 原文一起进入资源清单和发行包。

## N12B 原创采样音效

- 最终运行时资源：`media/audio/samples/*.wav`（9 个 cue）
- 确定性生成脚本：`tools/generate_n12b_audio_samples.ps1`
- 随包许可证：`media/audio/samples/LICENSE-HelloMine3D-Audio.txt`
- 生成日期：2026-08-26
- 生成方式：项目脚本使用固定采样率、固定种子和固定包络/分层波形离线生成，经 N12B cue
  映射筛选后提交固定 WAV；运行时不合成这些声音。
- 格式：44,100 Hz、mono、PCM16 RIFF/WAVE；9 个文件共 312,626 字节，解码 PCM 为
  312,230 字节。
- 许可证：Copyright (c) 2026 HelloMine3D contributors，MIT License；没有第三方采样、
  商标、语音或需署名素材。
- 生成脚本 SHA-256：`284484D886C334733B5E8F54DBDFF578A48ACDBBBF0C5341C43AF26B2E1BFCBA`
- 许可证 SHA-256：`AF64D609430DE21C78BF0B72B8E3BA62F1C0A0FD03CBC45ED140B1302208FB7D`

逐文件 SHA-256：

| cue 文件 | SHA-256 |
| --- | --- |
| `ambient-wind.wav` | `4556C128DEA877888C086CC96C5380FC78CA4F5B22AC8DAFC98AB86DE65BA12B` |
| `block-break.wav` | `E45325C54FE291DF408AC5C28C3418E83F24FAB8771C04D29286CF0A1E6ACE5B` |
| `block-place.wav` | `5FDC6C27327FA60E4F91ED2880E3F9AE17BDF309B4B54A60EE242CDECF055700` |
| `combat-guard.wav` | `ACC3F99056429DAEB4D9BFF8DF9A6A1364BC262761F1D14C7272538B22F2309F` |
| `combat-hit.wav` | `326EC462B46478C3B7C9EBD00CF4244C24618C915F8AE037A9E39C2835B3EB1D` |
| `combat-windup.wav` | `9FB159E62DF2ABD991FDB3847797FA2789F97F0924395DB46EA3670491556004` |
| `craft-success.wav` | `04BF873C721EC66478F0B57F73F2ACF6C6597122EF1C055B704E644146204FD7` |
| `item-pickup.wav` | `AACE2548AC2BEC6CA49CB4A28900EB79A0CBBEC6E960E8666EC29978444A3E43` |
| `ui-click.wav` | `9EC13A5DC26B7BFDB96256107A6383B9AA1BBDC791224B8A45B88BF37BA21454` |
