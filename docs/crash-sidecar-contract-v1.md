# 崩溃 sidecar 与离线符号合同 v1

本文定义 H2 的本地、脱敏崩溃上下文和 Windows 离线符号工具骨架。它扩展 H1 的本地
minidump，不引入网络、上传服务或自动发送。

## Sidecar schema 1

每个成功写出的 `.dmp` 最多对应一个同名 `.crash.txt`。文件采用严格的 `key value`
文本格式，最多 16 KiB，必须且只能包含以下 13 个字段：

- `schema 1`；
- dump 文件名和主模块文件名；
- `pdb-<32 位小写 GUID>-<age>` 构建身份；
- PE 时间戳、映像大小、PDB GUID 和 age；
- 异常代码、异常 RVA、受控符号探针 RVA 和线程 id；
- `upload_enabled 0`。

文件名不得包含斜杠、反斜杠或冒号，因此不能写入绝对路径、盘符、用户名或源码目录。
解析器拒绝未知字段、重复字段、尾随内容、错误数字范围、非小写 GUID、模块外探针和任何
启用上传的值。失败 sidecar 不影响已经成功生成的 dump，也不会伪造一个可解析文件。

## Windows 生成边界

崩溃后端在安装时读取主 EXE 的 PE/RSDS 信息，并在专用 dump 写线程中生成 sidecar。
受控异常可能来自系统模块，因此 `exception_rva` 允许为 0；`symbol_probe_rva` 指向当前
项目的 `CrashDiagnosticsPlatform::triggerControlledCrash`，用于 Alpha 阶段证明本地
符号链路确实匹配本次构建。

写入使用 Win32 文件 API 并显式 flush。普通退出、验证启动、三帧启动和被捕获的启动错误
均不得产生 dump 或 sidecar；代码库没有上传端点和联网分支。

## 离线符号工具

入口命令为：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File tools\symbolize_crash_sidecar.ps1 `
  -SidecarPath <file.crash.txt> `
  -ExePath <HelloMine3D.exe> `
  -PdbPath <HelloMine3D.pdb>
```

工具先用 DbgHelp 分别读取 EXE/PDB 身份，要求时间戳、映像大小、PDB GUID 和 age 全部与
sidecar 完全一致，再加载符号并解析探针地址。输出只包含源码文件名和行号，不输出源码
绝对路径。匹配成功返回 0；身份不匹配返回 3 并报告
`symbol-identity-mismatch`；格式或解析错误使用非零退出码。

该入口复用无渲染依赖的 `HelloMine3DCrashDiagnosticsSmoke.exe --symbolize`。符号器读取
实际 minidump 的模块、线程、内存、异常和上下文流，先尝试 `StackWalk64`，无法继续时再
对保存的栈内存做有界候选扫描；输出会明确标记 `minidump-stack-hybrid` 和
`unwind=stack-scan`，不会把回退结果冒充完整原生 unwind。所有帧都限制数量和文本长度，
源码只保留 basename，并且至少要解析出一个当前项目帧。

`tools/archive_windows_symbols.ps1` 依据 sidecar 的精确构建身份，在发行包之外生成独立符号
目录和确定性 ZIP。归档包含客户端/符号器的 EXE、PDB、包装脚本、说明和 manifest；创建前
会用匹配 dump 做一次隐藏符号化验证。错误 PDB 仍返回 3。

## H3 下次启动提示

启动时 `CrashReportInbox` 最多扫描 64 对严格有效、同目录、非空且未确认的
`.dmp`/`.crash.txt`，忽略畸形、孤立或已有 `.ack` 的条目，并按最新优先排序。主菜单或
游戏内显示阻塞式本地提示，玩家可以打开目录、复制脱敏详情或忽略；忽略仅原子写入持久
`.ack`，不删除 dump。提示期间 Escape 不会穿透到游戏 UI。实现没有上传端点，剪贴板文本
固定包含 `upload=0`。

## 自动验收

`HelloMine3DCrashDiagnosticsSmoke` 的 21 项检查覆盖 H1、sidecar、错误身份以及 H3 inbox
的有效/无效/孤立/已确认状态、排序和脱敏文本。`tools/validate_crash_diagnostics.ps1` 还在
隐藏 Release 客户端上验证：

1. 普通验证和三帧运行不生成产物；
2. 受控崩溃只生成一份非空 dump 和一份 sidecar；
3. sidecar 不泄漏项目或用户绝对路径，且上传关闭；
4. 匹配符号解析实际 dump 的有界混合栈并包含受控触发帧，错误符号明确拒绝；
5. 符号归档与发行包分离，归档自身可离线复现符号化；
6. 崩溃前保存可以重开，没有 `.pending` 候选；
7. 下次启动恰好发现一份本地报告，打开/复制/忽略均不联网。

2026-08-20 的最终 RC 门禁生成 124,513 字节 dump，混合栈解析到
`CrashDiagnosticsPlatform::triggerControlledCrash`、
`triggerControlledCrashIfRequested`、`OgreBootstrap::frameEnded` 等当前项目帧；错误 PDB
返回退出码 3。七项符号归档和下次启动唯一 pending 报告均通过，H2/H3 状态为 `Done`。
