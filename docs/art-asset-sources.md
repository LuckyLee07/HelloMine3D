# 美术资源来源记录

本文记录项目内自制或生成式美术资源的来源、加工链路和许可边界，供资源回归、打包和后续替换时核对。

## FS3 基础体素图集

- 最终运行时资源：`media/textures/DefaultPack.png`
- 可追溯生成源：`docs/art-sources/hellomine3d-atlas-imagegen-source.png`
- 确定性打包脚本：`tools/build_fs3_texture_atlas.ps1`
- 生成日期：2026-08-24
- 生成方式：Codex 内置 OpenAI 图像生成工具，对项目原有 `DefaultPack.png` 做两次编辑；模式依次为 `precise-object-edit` 和 `background-extraction`。
- 生成源 SHA-256：`03cc58092a6f436a80851d2a28fbcdcfbd242b8d9c588c6853ca6f5e801667ef`
- 最终图集 SHA-256：`51ebb2cbfef8101843715707e7b15678cb1951a28d12daef93f4c3dbb7456efa`

第一轮提示词要求保留 256×256、16×16 网格和低饱和像素风格，并按既有坐标重绘草地、泥土、石头、树皮、树叶、沙、水、植物、矿石和路标；第二行补充箱子、工作台、熔炉、玻璃和木板；第三行补充种子、小麦、木/石/铁工具、铁锭、面包和武器。空余格必须透明，不得出现文字、徽标或水印。

第二轮提示词仅做背景提取：把灰白棋盘格和空白单元转换为真正的 Alpha 透明区域，保持已有图标的形状、位置、颜色和画布尺寸，不重绘、不移动、不裁切，也不增加文字或水印。

图像生成源不是运行时图集。打包脚本按固定像素矩形裁切并以最近邻方式缩放到严格的 16×16 单元，恢复既有方块坐标，补充独立交互方块与物品图标，并跳过生成源中的重复铁剑。修改生成源后必须重新运行脚本，再运行资源清单和 `FS3/*` 图集断言。

本资源为本项目定向生成和加工的来源记录，没有复制 `F:\env1_trunk` 或其他第三方项目的美术文件，也没有引入第三方商标、徽标或署名素材。若未来改用外部素材包，必须在本文件新增作者、原始链接、许可证文本、允许的修改/分发方式和版本哈希；在这些信息齐备前不得进入发行包。

## N10 资源经济图标

- 可追溯生成源：`docs/art-sources/hellomine3d-economy-icons-imagegen-source.png`
- 生成日期：2026-08-25
- 生成方式：Codex 内置 OpenAI 图像生成工具；先用 `precise-object-edit` 生成 RawMeat、
  CookedMeat、CactusSalad、TrailRation、PlantFiber 五个像素图标，再用
  `background-extraction` 提取真实 Alpha。
- 生成源 SHA-256：`7373ad0ba60d2f47d921a5fb050ffd2f6861b58160ff80da345e2d5b92cce4ae`
- N10 图集 SHA-256：`4997b668024456ebd9704f5b89cc1923f64bb047ccdc115083e3e84062e274fd`

`tools/build_fs3_texture_atlas.ps1` 继续从原 FS3 来源重建全部既有单元，只从 N10 来源的固定
包围框裁切五个新图标到第三行 x=10..14。脚本使用 SourceCopy、最近邻、无平滑，并在 14×14
可视范围内居中；因此新增内容不会重绘或漂移已冻结的方块和物品图标。
