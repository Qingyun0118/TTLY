# 构建与真实模型验证

验证日期：**2026-11-15，Asia/Shanghai**。本记录包含真实 Qt 编译和真实 TensorFlow/YAMNet/XGBoost 推理；没有替换模型，也没有重新训练分类器。

## 环境

| 项目 | 实际版本或配置 |
| --- | --- |
| 操作系统 | Ubuntu 26.04.1 LTS，Linux x86_64 |
| Qt / qmake | Qt 5.15.18 / qmake 3.1 |
| 编译器 | G++ 15.2.0，C++17，Release，`make -j2` |
| uv / Python | uv 0.13.1 / CPython 3.10.22 |
| TensorFlow / Hub | 2.19.0 / 0.16.1 |
| Keras / tf-keras | 3.10.0 / 2.19.0 |
| NumPy / scikit-learn | 1.26.4 / 1.6.0 |
| XGBoost / joblib | 2.1.4 / 1.5.0 |
| setuptools | 80.9.0 |
| 推理设备 | CPU；`CUDA_VISIBLE_DEVICES=-1` |

精确 Linux 依赖见 [requirements-linux-py310.lock](../Sound_Rec/requirements-linux-py310.lock)。项目位于 exFAT 分区，Python 环境建立在 Linux 用户目录，Qt 通过 `TTLY_PYTHON` 使用它。

本机的 `sudo` 需要交互密码，因此实际 Qt 验证使用从官方 Ubuntu 软件仓库下载并在项目 `.local/qt5/` 解包的同版工具链；没有修改系统包数据库。README 的 `apt-get install` 命令是常规系统安装方式。

## Qt 两版完整编译与启动

两版均执行 qmake 和完整编译、链接：

```bash
mkdir -p build/tcp build/serial
cd build/tcp
qmake ../../TTLY/TTLY4.2tcp/TTLY.pro CONFIG+=release CONFIG-=debug
make -j2
cd ../serial
qmake ../../TTLY/TTLY4.3serial/TTLY.pro CONFIG+=release CONFIG-=debug
make -j2
```

| 验证项 | TCP 版 | 串口版 |
| --- | --- | --- |
| 可执行文件 | `build/tcp/ttly-tcp` | `build/serial/ttly-serial` |
| 文件大小 | 2,544,784 字节 | 2,544,832 字节 |
| 完整 Release 编译、链接 | 通过 | 通过 |
| 从项目外目录以 offscreen 启动并保持事件循环 | 通过 | 通过 |
| 打开主窗口、视频窗口、声音窗口 | 通过，退出码 0 | 通过，退出码 0 |
| 默认端口 8000 / 8001 / 8002 | 通过 | 通过 |
| `Result/image`、`images`、`3d`、`voice` 路径 | 通过 | 通过 |

另用源码路径与构建路径都含空格的小工程实际执行 qmake、G++ 编译和运行，确认 `TTLY_SOURCE_ROOT` 宏及编译时路径回退有效。MCU 控制协议没有修改。日志摘录见 [TCP UI 检查](validation/qt-tcp-smoke.log)、[串口 UI 检查](validation/qt-serial-smoke.log) 和 [空格路径检查](validation/qmake-space-check.log)。

此处验证的是 Linux 构建和无显示窗口逻辑。实体手柄、串口、摄像头、麦克风、机器人网络通信和 Windows 构建没有在本次测试中验证。

## 真实声音推理

YAMNet 从 Google 官方模型入口取得，作为本地 SavedModel 加载；分类器使用 `Sound_Rec/animal_classifier_pro.joblib3`。原始序列化内容 SHA-256 为：

```text
b9219e82cd5af63a37f883672bd4fb1876ec0c4c0831865be827bd3704a8385f
```

发布时原始内容做了 gzip 无损封装，文件由 28,579,201 字节缩小为 10,746,309 字节，仍可直接由 joblib 读取。发布文件 SHA-256 为 `356d6c58b6865492d5d37da3c31ef19e2833466ad65155806964505531643701`。已实际比较两种封装加载后的类别、特征数、scaler 数组和 Booster 原始数据；Booster UBJ SHA-256 均为 `c1c10c8de3164140cf4d1711406ba59e91ecb87a8617fa263016276d5736b9ba`。压缩后的最终文件也重新通过了公开 `smoke.wav` 的真实推理，输出与下表一致，退出码为 0。

每个文件都实际经过音频读取与 16 kHz 重采样、YAMNet 特征提取、2048 维特征构建、StandardScaler 和 XGBoost 分类。两次批处理均以 **退出码 0** 完成。

| 音频标识 | 时长 | 最高分类及分值 | 第二分类及分值 |
| --- | ---: | --- | --- |
| 历史 `marine_animals/Harp Seal/01.wav` | 2.402 秒 | Harp Seal，0.61 | Long-Finned Pilot Whale，0.31 |
| 历史 `marine_animals/Humpback Whale/01.wav` | 2.044 秒 | Humpback Whale，0.99 | Bearded Seal，0.00 |
| 历史 `train/Harp Seal/01.wav` | 25.542 秒 | Harp Seal，0.92 | Humpback Whale，0.04 |
| 历史 `train/Humpback Whale/01.wav` | 26.331 秒 | Humpback Whale，0.96 | Northern Right Whale，0.01 |
| 仓库 `examples/audio/smoke.wav` | 1.000 秒 | Bearded Seal，0.41 | Sperm Whale，0.13 |

前四份音频是项目历史数据，类别依据原目录标签，其中两份来自 `train/`；它们没有随开源仓库发布。最后一份是公开的合成正弦信号，用于新用户执行链路检查，没有动物类别标签。以上是功能冒烟测试，未进行独立数据集评测，不据此报告准确率或泛化性能。

可从项目根目录复现公开样例：

```bash
CUDA_VISIBLE_DEVICES=-1 \
TF_NUM_INTRAOP_THREADS=2 TF_NUM_INTEROP_THREADS=1 OMP_NUM_THREADS=2 \
"$TTLY_PYTHON" Sound_Rec/soundimprove.py \
  --yamnet "$PWD/Sound_Rec/.cache/yamnet" \
  --audio examples/audio/smoke.wav
```

环境和 YAMNet 准备方式见 [README](../README.md)；完整的脱敏输出见 [短音频日志](validation/inference-short.log) 和 [长音频与公开样例日志](validation/inference-long.log)。CPU 测试时 TensorFlow 输出 CUDA 初始化诊断和 `pkg_resources` 弃用提示，但推理完成且退出码为 0。

## 实测发现的依赖修正

1. 已训练分类器内嵌的 scikit-learn / XGBoost 版本分别为 **1.6.0 / 2.1.4**，优先于旧环境中其他版本的包记录。
2. 首次真实推理失败于 `No module named 'pkg_resources'`。TensorFlow Hub 0.16.1 仍导入该 API，而当时安装的新 setuptools 已移除它；固定为 **setuptools 80.9.0** 后重新运行通过。
3. PyPI 大包下载在当前网络超时；使用清华大学 PyPI 镜像与 `UV_HTTP_TIMEOUT=600` 后完成安装。镜像只影响下载位置，包版本仍由清单约束。

## 其他检查

- Python 3.10 下的 8 项 CLI / WAV 单元测试全部通过。
- `uv pip check`：59 个已安装包，依赖约束全部满足。
- Qt `.ui` / `.qrc` 与源码资源引用检查通过。
- 模型内容未改变，编译缓存、环境和模型下载缓存均不进入 Git 仓库。
