# 沙盒 HUD 素材来源

使用内置 OpenAI `imagegen` 工具生成。用户认可的是基于实际游戏画面的低饱和、轻边框方向；
设计草图不是实机结果。交付素材位于 `media/textures/`，原始 PNG 未做离线像素编辑。

- `SandboxPanel.png`：安静的深绿／灰石底板。生成器在轮廓外绘出了棋盘格；Alpha 修正尝试也未得到透明 PNG，因此只采样已检查的深色矩形 UV `.064..936`。九宫格切分 `.064/.15/.85/.936`，运行时维持角部尺寸。未把假棋盘格当作 Alpha 使用。
- `SandboxGlyphs.png`：RGBA 图集，左半为叶片，右半为可着色心形；使用 nearest 采样，绘制后恢复线性采样。像素图形表达任务／生命，文字与真实数值不在图片内。
- 小地图没有替换成生成图片：仍绘制真实驻留地表和地标，细外圈、四向方牌与刻度采用程序绘制，方位／区域名／比例尺继续使用本地化文字。

`provenance.json` 记录所用原图的 SHA-256、尺寸、通道和提示词。最终提示词为
`context-design-prompt.txt`、`quiet-panel-prompt.txt` 和 `glyphs-prompt.txt`。
其余 `panel/compass[-pixel]-prompt.txt` 是被用户否定的写实及厚木框方向，保留用于说明设计排除项，
对应图片仅归档在本地 `build/visual-upgrade-20260917/hud-interface-r35/rejected-art-*`。
正式实机截图和性能证据另存该批次产物目录，不将设计图充当验收截图。
