# 冒险生态像素材质源

`material-sheet-v2.png` 由内置图像生成工具制作并修订，最终完整提示词、失败说明和源图哈希见 `generation-prompts.json`。这张 4×3 源图提供树皮、断面、叶片及六种地表材质；图块位置固定为图集第 8 行、列 0..11，旧槽位保持。

源图实际三行裁切范围为 y=[0,361)、[362,713)、[714,1086)，跳过混合边界像素；不能按等高三行裁切，否则雪格会混入苔石暗纹。列宽保持 362 像素。

编译使用 `tools/adventure_texture_source.py`：按 texel 中心采样为标准路径 32×32 像素簇，兼容路径 16×16；只有两种叶图中 RGB 各通道不超过 12 的黑色像素成为透明孔洞，其余像素 alpha 为 255。继承标准数组的独立 mip、线性光预乘过滤和透明覆盖率处理，不运行时读取本目录。

```sh
python3 tools/build_warm_texture_atlas.py
python3 tools/validate_warm_texture_atlas.py
python3 tools/build_warm_texture_array.py
python3 tools/validate_warm_texture_array.py
```

首版白色叶孔和非预期半透明未采用，修订由图像工具完成。当前资源检查不代表新生态已进入生成或完成实机场景评审。来源图与最终编译产物按项目材质许可使用。
