# 2026-11-15 项目清理记录

保留 `TTLY/TTLY4.2tcp/`、`TTLY/TTLY4.3serial/`、设备端代码、新版声音识别，以及独立三维重建源码和说明。

## 删除与归档

清理共完成 72 项文件或目录操作：19 项删除、53 项移出项目归档。按文件实际字节统计，删除约 **6.51 GiB**，移出项目约 **5.12 GiB**。目录所在文件系统的小文件分配空间较大，这些数值不等于磁盘工具显示的分配空间。

删除内容包括两个旧 `.conda/`、重复的嵌套音频数据、空测试脚本、SLAM3R 临时数据、Python 缓存及 CUDA 扩展编译中间文件。

项目同级目录 `../ttly-archive-20261115/` 保存了其余历史文件，完整清单在归档目录的 `manifest.json` 中。清单的 `source` 为原项目内路径，`destination` 为归档目录内路径。

| 归档子目录 | 内容 |
| --- | --- |
| `legacy/` | 旧 Qt 版本、旧声音模块及压缩包；`TTLY4.2/` 只有旧构建成品，也保存在这里 |
| `builds/` | 保留两版 Qt 的旧构建产物 |
| `metadata/` | 两版子工程的 Git 元数据及 Qt Creator 本机配置 |
| `binaries/` | 旧打包 EXE 和协议测试成品 |
| `data/` | 训练集、测试音频、比赛结果、三维场景及对应压缩包 |
| `models/` | 未使用的 `.joblib1`、`.joblib2` 分类器 |
| `third-party/` | Instant-NGP、COLMAP、FFmpeg 及其 Windows DLL 和插件 |
| `notes/` | 比赛流程、本机编辑器设置和原需求书 PDF |

需要某份历史数据时，根据清单把相应文件复制到新的工作目录。两个已删除的 `.conda` 不再作为运行依赖恢复，应按模块的依赖清单重新建立环境。

原 `voice_rec.zip` 作为历史快照保留在归档目录，其中也包含旧环境压缩内容；它没有被放回源码仓库。

## 路径和依赖修正

- 声音模块统一为 `Sound_Rec/`，保留一份 `animal_classifier_pro.joblib3`。模型参数未修改，原始内容 SHA-256 为 `b9219e82cd5af63a37f883672bd4fb1876ec0c4c0831865be827bd3704a8385f`。后续发布时采用 gzip 无损封装，实际发布哈希与大小见 [模型卡](../Sound_Rec/MODEL_CARD.md)；原始文件另存于归档的 `models/Sound_Rec/animal_classifier_pro.original.joblib3`。
- Qt 共用 `TTLY/common/runtimepaths.h`，从 `TTLY_ROOT`、程序/工作目录祖先或编译时源码根定位项目；Python 使用 `TTLY_PYTHON`、项目 `.venv` 或 `PATH`，不再调用旧 `.conda/pythonw.exe`。
- 输出默认位于 `Result/`；控制、视频、音频默认 TCP 端口对齐设备端的 `8000/8001/8002`。MCU 控制帧协议保持原样。
- 声音依赖版本取自原环境的实际 pip 元数据，常规识别不依赖训练集。YAMNet 模型支持在线缓存或本地 SavedModel。
- InstantNGP 转换脚本通过 `PATH` 或 `--colmap_binary`、`--ffmpeg_binary` 使用外部程序，不再假定项目自带 `external/`。
- 协议帧生成和串口测试源码移入 `tools/`；Windows 重命名工具改为显式传入目标音频目录。

本机原 Windows 工具仍可从归档的 `third-party/` 查找。Instant-NGP 的配置同时保留在项目 `InstantNGP/configs/` 和归档的程序目录中，以便单独运行归档成品；历史场景在 `data/InstantNGP/`，需要时显式打开相应场景。

## 验证范围

已运行 8 项标准库测试，覆盖声音 CLI 路径、退出码、离线模型参数、批处理行为及 WAV 切割/拼接；真实分类器与 TensorFlow 用测试替身隔离。另已检查 Python 语法、Qt 工程/UI/资源引用、第三方工具路径解析和协议帧生成器。

初次清理时本机只有 Python 3.14，尚未完成 Qt 编译或真实模型推理。后续已用 uv 建立 Python 3.10 环境、准备 Qt 5.15.18，并完成两版完整编译、窗口冒烟检查和真实模型推理；结果见 [构建与真实模型验证](VALIDATION.md)。实体硬件联调仍需在机器人设备上进行。
