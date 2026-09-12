# 暖野图集构建

运行：`python3 tools/build_warm_texture_atlas.py`；依赖 Python 3 与 Pillow。

验证：`python3 tools/validate_warm_texture_atlas.py`。Windows 原入口默认转到同一构建器，`-Legacy` 保留前版生成流程。

母版采用 4×2 排列：草 A/B/C、泥土；石头、树皮、年轮、树叶。每格内缘最近邻采样至 16×16。树叶保留前版 Alpha；草侧面由新草与新土合成。五生态三变体只改变固定槽位内容。物品图标与未覆盖方块从冻结旧图集原样复制。

PNG 使用固定无损 stored DEFLATE 编码，无元数据、时间戳或依赖平台的压缩选择。

源图 SHA-256：

- `hellomine3d-pre-warm-atlas.png`：`a659d9037efdfeff55d3f7a371f562d1be26d1fca160e69cce3c6e95219bdd84`

- `hellomine3d-warm-natural-source.png`：`8860f3ba128554ebf8160ad57efb1887f57c266a5380aefe993fcb051ce021a8`


源图与提示词在本目录；菜单图来源见 2026-09-10 视觉方案报告。生成图授权归属与其它图集资源相同，见 `media/textures/LICENSE-HelloMine3D-Textures.txt`。
