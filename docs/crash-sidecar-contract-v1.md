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

该入口复用无渲染依赖的 `HelloMine3DCrashDiagnosticsSmoke.exe --symbolize`，避免改变已
冻结的工程目标图。它是单帧骨架，不宣称已实现任意 dump 的完整线程/栈遍历。

## 自动验收

`HelloMine3DCrashDiagnosticsSmoke` 的 16 项检查包含原 H1 的 12 项，以及 sidecar 往返、
路径脱敏、未知字段拒绝和构建身份不匹配四项。`tools/validate_crash_diagnostics.ps1` 还在
隐藏 Release 客户端上验证：

1. 普通验证和三帧运行不生成产物；
2. 受控崩溃只生成一份非空 dump 和一份 sidecar；
3. sidecar 不泄漏项目或用户绝对路径，且上传关闭；
4. 匹配符号解析至少一个项目帧，错误符号明确拒绝；
5. 崩溃前保存可以重开，没有 `.pending` 候选。

2026-08-17 的 Alpha 检查点最终生成 127,425 字节 dump，匹配符号解析到含
`triggerControlledCrash` 的当前项目链接符号；本次 PDB 未提供源码行时如实报告
`source=unknown:0`。错误 PDB 返回退出码 3。H3 的下次
启动提示、打开/复制/忽略交互、最终符号归档和干净包验收不属于本合同。
