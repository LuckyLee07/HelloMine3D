# 暖野 M1 材质来源与重建

当前新增运行资产来自 `pixel-revision/`，树叶除外。用户再次反馈后，全部树叶语义地址恢复暖野 v1 像素与 Alpha；三张新叶源图保留为被替换的设计。2026-09-12 用户否定首个自然手绘候选，
要求保留旧版清晰的方块质感；该目录保存重新逐张生成的 19 张像素源图、提示词与原始路径。
本目录根部的 19 张自然手绘源图及早期母版保留为被替换的方案，不再作为默认构建输入。

用 Python 3、NumPy、Pillow 运行：

```sh
python3 tools/build_warm_texture_array.py
python3 tools/validate_warm_texture_array.py
```

构建器将源图转换为 128×128 母版和标准 64×64 独立纹理层，
生成 `media/textures/WarmWilderness64.hmt` 与 `pixel-revision/array-build.json`。
JSON 记录源图/母版/数组 SHA-256、120 个语义地址的来源、136 个空槽、每级 Alpha 覆盖。
泥土、石头、树皮和沙的双源图在同一语义地址中合成；生态草地保留独立变体。
其他方块与物品沿用暖野 v1 图集，逐项标为 `retained`；UI 继续使用该独立图集。

每层以预乘 Alpha 的线性颜色空间过滤，结果存为 sRGB RGBA8。
透明边缘延展可见颜色，叶片/草丛保持裁剪覆盖，空槽所有 mip 均为零。
运行时保留既有颜色计算方式，不额外开启硬件 sRGB 解码；放大/缩小时取最近像素，
仅在 mip 等级间线性混合，避免把像素风格模糊掉。

HMTARRAY v1 使用 36 字节小端头：8 字节魔数、五个 uint32
（版本、边长、层数、mip 数、载荷字节数）、uint64 FNV-1a 校验，后接按 mip 排列的 RGBA8 层。
加载器严格检查版本、完整长度、边界、层数与校验；标准 profile 只接受 64 像素。
`--edge 128 --output <file> --report <file>` 用于开发对照，不能覆盖标准交付文件。

标准数组像素载荷 5,592,064 字节，另保留 262,144 字节 v1 图集。
128 对照为 22,369,280 字节，超过本轮纹理预算，未作为运行设置提供。
许可见 [纹理许可证](../../../media/textures/LICENSE-HelloMine3D-Textures.txt)。
