# V10E 轻量后处理合同 v1

本文冻结 Stage 10 `V10E` 的后处理范围、设置迁移、能力回退、性能和开发者视觉证据。
实现提交为 `8f45aca`。Windows 状态为 `Done`；当前没有目标 macOS 机器，真实 Release
编译与窗口冒烟保持 `Verify`，不得用 Windows 证据替代。

## 范围与非目标

V10E 只增加可关闭的 tone curve、确定性低幅色带抖动和八采样极轻 bloom。它不改变
save v11、terrain v3、世界生成、方块/物品身份、32 字节 terrain vertex 或既有阴影合同，
也不引入自动曝光、运动模糊、景深、SSAO、体积光或 HDR/PBR 重写。

- `Off` 不安装 Ogre compositor，输出保持 V10D 路径。
- `On` 使用单个有界 compositor pass；bloom 增益受源像素剩余亮度限制，不能把明暗阶梯的
  多个高亮级同时推成纯白。
- HUD、十字准星、快捷栏和 ImGui 在主窗口 `postViewportUpdate` 中绘制，位于后处理之后，
  不参与 tone curve、抖动或 bloom。
- GPU 能力不足、资源创建失败或验证强制回退时，设置仍保留 `On` 请求，但运行时原子退回
  `Off` 并显示双语说明；后处理失败不能阻断世界启动。

## 设置与资源合同

`bin/config.txt` 升为 settings v6，新增 `postprocessingquality off|on`。新安装默认 `Off`；
legacy v0 与显式 v1-v5 都固定迁移为 `Off`，旧版本混入新字段、未知档位、重复字段和未来
版本均严格拒绝。设置页可即时应用/取消/恢复默认值，保存失败保持旧运行状态。

基础资源新增 `HelloMine3D.compositor`、`HelloMine3DPostProcess.vert` 和
`HelloMine3DPostProcess.frag`，并纳入 manifest、资源包 base-only 规则和启动前接口校验。
最终 manifest 为 77 项，两份本地化目录各 360 个严格对齐的 key。

Stage 10 补充性能身份新增 `stage10_post_processing`。`Off` 与 `On`、不同 shadow 档或其他
图形身份不可伪装成可比较样本；比较器在核心 schema 3 之外先验证补充身份一致。

## 自动与真实客户端证据

2026-08-28 在 VS2017/v141 下完成：

- Debug/Release V10E 聚焦世界断言各 `22/22`；Release 完整世界 `743/743`；
- Debug/Release 资源包各 `80/80`，manifest `77/77`；
- 启动失败矩阵 `15/15`，Stage 10 性能补充合同 `11/11`；
- Release GTX 1050 Ti / OpenGL 4.6 隐藏真实客户端报告
  `requested=on active=on fallback=0 reason=supported passes=1`；强制回退报告
  `requested=on active=off fallback=1 reason=forced passes=0`，两者均正常退出。

最终同场景三次样本保持 1280x720、seed `20260820`、位置 `(264,96,8)`、世界时间 6000、
361 chunks 与 1833 sections。以下为三次中位数，frame P95/P99 均未触发护栏，因此没有消耗
性能例外：

| 档位 | FPS | frame P50 | frame P95 | frame P99 | render P95 |
| ---- | ---: | --------: | --------: | --------: | ---------: |
| Off | 156.7 | 5.08 ms | 14.28 ms | 17.13 ms | 5.40 ms |
| On | 156.1 | 5.30 ms | 12.99 ms | 16.45 ms | 5.97 ms |

每组各存在一次系统调度离群样本，因此合同使用预先约定的三次中位数，不以单次 FPS 代替
frame percentile 判断。

## 开发者视觉记录

项目所有者已明确授权 Codex 完成 V10E 视觉检查。以下均为提交前最终 Release 原图，已按原
尺寸逐张检查：

| 场景 | SHA-256 |
| ---- | ------- |
| On 明暗阶梯 | `EC10487F090C8E42B2008F32AFC69093BBFBE0577C9680E3D2DA2EBC257AB7F8` |
| On 正午 | `55C587B3BD6FBD9BF1E7B3B1A3B15F5A717E6DC6B131B6925692C8B5AB9608C8` |
| On 夜晚 | `1100F7B7891D677A885DB4C449E8E9C7437C666B8C06E8E5FAE49D637D77A19E` |
| On 设置页 1024x768 | `D9C5E03A1DB8450E88C352E6BE482FE5C01443D8EBB1BC79014D6A1BA5D71743` |
| Off 正午控制 | `9BC68652F3B7CAE15E6E3206D413E7C3E4D150E81E7BB46AFE9CEA838BFD461F` |
| Off 夜晚控制 | `EE8E5AFEB6B728456BA606F129441777F87AA479B00AF8DF72F7C0CF07AB41D2` |

阶梯高亮区 15 个相邻级别全部保持严格递增，只有最终白色达到 255。正午没有过曝或材质
糊成一片，夜晚地形、植被和海岸仍可读；1024x768 设置页完整位于窗口内。HUD、十字准星、
快捷栏和菜单边缘保持锐利，未被后处理污染。开发者视觉检查结论为 `PASS`。

## 退出结论

Windows 实现、双配置受影响门禁、完整 Release 世界回归、资源/失败负例、真实支持/回退
客户端、同档性能和开发者视觉检查均通过，V10E 可标记为 `Done（Windows；macOS Verify）`。
下一批为 `VISUAL-RC`；其中仍须以最终代码身份重跑正式 Q1/Q3、干净包与 bundle，并补齐
macOS shader/窗口证据和独立产品体验边界。
