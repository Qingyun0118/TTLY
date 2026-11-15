# 海洋动物声音识别

`soundimprove.py` 用 YAMNet 提取音频特征，再用已训练的 XGBoost 分类器识别 12 类海洋动物。常规识别直接加载脚本旁的 `animal_classifier_pro.joblib3`，不训练模型，也不需要训练数据集。类别、滑窗与特征处理、文件哈希和发布许可见 [模型卡](MODEL_CARD.md)。

当前已在 Linux CPU 上运行真实 YAMNet 和保留分类器；环境与结果见 [验证记录](../docs/VALIDATION.md)。下面的命令均从项目根目录执行。

## 安装环境

本次验证使用 **uv 0.13.1、Python 3.10.22**。主要模型依赖为 TensorFlow 2.19.0、TensorFlow Hub 0.16.1、Keras 3.10.0、tf-keras 2.19.0、NumPy 1.26.4、scikit-learn 1.6.0 和 XGBoost 2.1.4。

安装 [uv](https://docs.astral.sh/uv/getting-started/installation/) 后，可在普通 Linux 文件系统创建项目环境：

```bash
uv python install 3.10
uv venv --python 3.10 .venv
export TTLY_PYTHON="$PWD/.venv/bin/python"
uv pip install --python "$TTLY_PYTHON" -r Sound_Rec/requirements.txt
```

若源码位于 exFAT 等不能满足虚拟环境符号链接要求的文件系统，将环境放在 Linux 用户目录。这也是本次实际验证采用的布局：

```bash
uv venv --python 3.10 "$HOME/.local/share/ttly/venv"
export TTLY_PYTHON="$HOME/.local/share/ttly/venv/bin/python"
uv pip install --python "$TTLY_PYTHON" -r Sound_Rec/requirements.txt
```

[requirements.txt](requirements.txt) 固定主要依赖；scikit-learn 和 XGBoost 版本取自保留模型的内嵌信息。TensorFlow Hub 0.16.1 仍导入 `pkg_resources`，本次遇到新版 setuptools 移除该模块导致的真实启动失败，因此固定 `setuptools==80.9.0`。

需要复现本次 **Linux x86_64 / Python 3.10** 的完整已安装依赖时，使用 [requirements-linux-py310.lock](requirements-linux-py310.lock)：

```bash
uv pip sync --python "$TTLY_PYTHON" Sound_Rec/requirements-linux-py310.lock
```

如果索引访问超时，可延长超时并自行选择可访问的镜像。本次安装曾使用以下选项完成依赖下载；镜像是可选配置，代码没有写死镜像地址：

```bash
UV_HTTP_TIMEOUT=600 uv pip install --python "$TTLY_PYTHON" --default-index https://pypi.tuna.tsinghua.edu.cn/simple -r Sound_Rec/requirements.txt
```

Windows 使用 Python 3.10 和基础依赖清单，例如 PowerShell：

```powershell
py -3.10 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r Sound_Rec/requirements.txt
$env:TTLY_PYTHON = (Resolve-Path .\.venv\Scripts\python.exe).Path
```

Windows 环境未纳入本次验证；Linux 锁定清单不作为 Windows 环境清单使用。

## 准备 YAMNet

本地 `.joblib3` 只保存分类器、标准化器和标签编码器；YAMNet 是单独的特征模型。未指定本地模型时，脚本默认通过 `https://tfhub.dev/google/yamnet/1` 加载，需要能够访问该模型下载地址。

可先下载官方压缩包，解压到被 Git 忽略的缓存目录：

```bash
curl -fL 'https://tfhub.dev/google/yamnet/1?tf-hub-format=compressed' -o /tmp/ttly-yamnet.tar.gz
mkdir -p Sound_Rec/.cache/yamnet
tar -xzf /tmp/ttly-yamnet.tar.gz -C Sound_Rec/.cache/yamnet
export YAMNET_MODEL="$PWD/Sound_Rec/.cache/yamnet"
```

该目录应包含 `saved_model.pb`、`variables/` 等完整 SavedModel 文件。通过 `--yamnet "$YAMNET_MODEL"` 显式加载该目录，可以在完成安装和下载后离线推理。也可使用自己准备的其他本地目录。

TF Hub 的在线下载缓存默认位于 `Sound_Rec/.cache/tfhub/`，与上面手动解压的 `yamnet/` 目录不同。缓存位置可用 `--cache-dir` 或 `TFHUB_CACHE_DIR` 修改，命令行参数优先。

## 识别录音

先用仓库提供的合成信号检查完整链路：

```bash
CUDA_VISIBLE_DEVICES=-1 TF_NUM_INTRAOP_THREADS=2 TF_NUM_INTEROP_THREADS=1 OMP_NUM_THREADS=2 \
"$TTLY_PYTHON" Sound_Rec/soundimprove.py --yamnet "$YAMNET_MODEL" --audio examples/audio/smoke.wav
```

合成信号不含真实动物叫声，输出类别只表明模型执行成功，不能用于准确率判断。识别自己的录音或批量识别：

```bash
"$TTLY_PYTHON" Sound_Rec/soundimprove.py --yamnet "$YAMNET_MODEL" --audio /path/to/recording.wav
"$TTLY_PYTHON" Sound_Rec/soundimprove.py --yamnet "$YAMNET_MODEL" --audio first.wav --audio second.wav
"$TTLY_PYTHON" Sound_Rec/soundimprove.py --model /path/to/classifier.joblib3 --yamnet "$YAMNET_MODEL" --audio recording.wav
```

如需恢复在线加载，可不设置 `YAMNET_MODEL`，也不传 `--yamnet`。显式 `--yamnet` 优先于环境变量。

脚本将输入转为 16 kHz 单声道，按滑窗提取特征，再融合为整个音频的特征向量。结果保留 Qt 使用的文本格式，显示得分最高的两个类别，例如 `- Harp Seal (0.61)`。详细的 12 类列表和特征维度见 [模型卡](MODEL_CARD.md)。

| 退出码 | 含义 |
| --- | --- |
| `0` | 所有请求的操作成功 |
| `1` | 文件缺失、模型初始化失败或至少一条音频识别失败 |
| `2` | CLI 参数错误 |

批处理中出现单个文件失败时仍可继续处理其他有效文件，最终退出码为 `1`，错误写入标准错误。`--help` 只需 Python 标准库，不会加载 TensorFlow 或下载模型。

## 从 Qt 调用

启动 Qt 前，在同一终端设置：

```bash
export TTLY_ROOT="$PWD"
export TTLY_PYTHON="$HOME/.local/share/ttly/venv/bin/python"
export YAMNET_MODEL="$TTLY_ROOT/Sound_Rec/.cache/yamnet"
./build/tcp/ttly-tcp
```

使用项目 `.venv` 时，将 `TTLY_PYTHON` 改为 `"$PWD/.venv/bin/python"`。Qt 的声音识别进程继承这些变量，用户选择录音后调用本脚本；没有设置解释器时，Qt 会查找项目 `.venv`，然后查找 `PATH` 中的 Python。Qt 构建与输出路径见 [主 README](../README.md)。

## 重新训练

训练只在显式传入 `--train` 时发生：

```bash
"$TTLY_PYTHON" Sound_Rec/soundimprove.py --train --yamnet "$YAMNET_MODEL" --data-dir /path/to/train --model /path/to/new_classifier.joblib3
```

在数据目录中按 `CLASS_NAMES` 的英文类别名称建立文件夹，每类放置 WAV；至少需要两个有效类别。默认数据目录是项目根的 `train/`，训练录音不随公开仓库发布。默认模型输出为 `Sound_Rec/animal_classifier_pro.joblib3`，会覆盖当前文件；上面的示例为新模型指定其他路径。

本次验证使用现有分类器，没有重新训练。真实录音推理成功属于运行验证，当前文档不提供数据集准确率结论。

## WAV 工具与轻量测试

以下工具和测试只依赖 Python 标准库：

```bash
python3 Sound_Rec/cut.py recording.wav 10 25 --output-dir /path/to/parts
python3 Sound_Rec/pinjie.py first.wav second.wav -o combined.wav
python3 -m unittest discover -s tests -v
```

切割点单位为秒，输出文件名为 `part1.wav`、`part2.wav` 等。拼接文件必须具有相同声道数、采样宽度和采样率。轻量测试使用测试替身覆盖 CLI 和错误处理，真实模型推理证据另见 [验证记录](../docs/VALIDATION.md)。

## 模型与许可

作者确认保留分类器由本人训练，允许随项目采用 Apache-2.0 公开；其大小、SHA-256 及依赖版本记录于 [模型卡](MODEL_CARD.md)。YAMNet 与其他依赖保留各自上游条款，详见 [第三方说明](../docs/THIRD_PARTY.md)。训练音频及个人录音不包含在公开仓库中。
