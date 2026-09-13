# R1 树冠升级：旧版与新版实机对比

归档说明：这是后来被用户否定轮廓的**上一 R1 叶色候选**，其正式性能
林地 High FAIL 仍有效，不代表当前方块橡树样板。当前 v6 方叶树形、同机位
对照与新一轮验证见 [R1 方块橡树记录](r1-voxel-oak-acceptance-2026-09-13.md)。

本报告比较同一游戏世界的旧叶包与新叶包，供用户判断树冠是否接近封面所指的
细碎叶簇、明暗层次和林地氛围。封面中的 `WarmWildernessMenu.png` 是独立
静态美术图，不能用它代替实时游戏画面或作逐像素 A/B。

新候选只改变标准纹理数组的 16 个橡树叶层。两张原创 RGBA 源图经线性光预乘
缩小，派生第三个翻转变化并保留 7 级独立 mip。旧版立方叶体积、生成地形、
碰撞、采集、存档和兼容图集均不变；没有采用此前失败的叶壳或交叉卡片方案。

## 可复核画面

两包均为 seed `20260807`、terrain v5、正午 `6000`、朝向 `0 0 0`、
RD8、FOV90、1280×720 逻辑窗口／2560×1440 实际截图，High 阴影与后处理 On。
下图为实际游戏 10 秒帧，均取自独立包的开发者固定机位诊断。

| 机位 | 旧叶 R1 包 | 新叶正式候选包 |
| --- | --- | --- |
| 林中近景 `(1024,140,1024)` | [原始 PNG](../../build/goal-r1-canopy-20260913/visual-ab/forest-old-high/frames/capture_10000ms.png) | [原始 PNG](../../build/goal-r1-canopy-20260913/visual-ab/forest-final-high/frames/capture_10000ms.png) |
| 密林近景 `(928,110,928)` | [原始 PNG](../../build/goal-r1-canopy-20260913/visual-ab/dense-old-high/frames/capture_10000ms.png) | [原始 PNG](../../build/goal-r1-canopy-20260913/visual-ab/dense-final-high/frames/capture_10000ms.png) |
| 草地远景 `(1056,106,928)` | [原始 PNG](../../build/goal-r1-canopy-20260913/visual-ab/grassland-old-high/frames/capture_10000ms.png) | [原始 PNG](../../build/goal-r1-canopy-20260913/visual-ab/grassland-new-high/frames/capture_10000ms.png) |

实际观察：林中与密林的新叶面从旧版较大、重复的灰暗绿块变成更细的像素
叶簇，亮绿斑点与深绿内层的分布更接近封面右侧近树的观感。林下仍有暗面，
日间 Off 阴影没有整冠变成单色；夜间未见明显黑边；兼容画质仍显示旧图集。
远景颜色略暖、较饱和，但树冠**主轮廓仍是原有的两层方块平台**，无法
仅靠叶片贴图达到封面中更丰富的树形高低和冠幅。这是留给用户的审美判断，
不以开发者静态检查代替用户确认。

补充原图：
[新版林地 Off](../../build/goal-r1-canopy-20260913/visual-ab/forest-new-off/frames/capture_10000ms.png)、
[新版林地夜间](../../build/goal-r1-canopy-20260913/visual-ab/forest-new-night/frames/capture_10000ms.png)、
[新版兼容画质](../../build/goal-r1-canopy-20260913/visual-ab/forest-new-compat/frames/capture_10000ms.png)。
每组 `capture.json` 记载包身份、种子、设置、命令与原始 PNG SHA-256。
固定机位原图不证明正常游玩中镜头移动时的闪烁情况。

## 身份与验证

R1 起始 commit：`b155f5345950f844e2ccc5aeeb28aa58824b28a7`。
新数组 SHA-256：`cced1d43ea6f7813eaad889c190bd9db739f3f06e1461c391e208ab62a45c107`。
正式候选为 `build/goal-r1-canopy-20260913/HelloMine3D-R1-Canopy-Final.app`，
109 个发行文件逐个哈希核对通过；发行清单 SHA-256：
`c068ba74d96dbb5dbe161c5f9c14b3319891010a7c7cd466698cfb41fa12cc43`。
样板包与正式包的可执行文件、纹理数组相同；林中与密林新版图已从正式包
直接重采，草地远景仍来自同资源样板包。`package-verify.json` 证明新旧 R1 包除构建身份文件外
只改动纹理数组；`validation/leaf-only-diff.json` 证明数组 7 个 mip 均只改
16 个叶层，其余 240 层不变。

暖野图集、数组/Alpha/mip、字节一致重建、15 个坏资源负例、
macOS Xcode Debug/Release 全门禁与真实窗口探针均 PASS；
原始记录在 `build/goal-r1-canopy-20260913/validation/`。
正式 36 次成对性能矩阵中，林地 High P95 为 **1.831×，FAIL**（门槛 ≤1.10）；
其他五组 P95/P99 通过。随后固定六轮旧 R1／新叶诊断的林地 High P95/P99
为 1.043×／1.013×，说明正式结果受同档帧时双峰影响，但诊断不改写正式 FAIL。
正常菜单/创建/暂停/设置/保存重开部分 PASS；持续移动、转头、采集及移动视频
因输入工具限制为 BLOCKED。用户树冠审美确认与独立包外验收尚未取得。
完整逐项状态与原始数据见
[R1 总验收报告](goal-r1-m1-acceptance-2026-09-12.md)的新版段落。
