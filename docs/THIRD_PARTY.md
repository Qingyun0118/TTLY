# 第三方代码、模型与依赖

本项目自有代码及文档采用根目录 [Apache-2.0 许可证](../LICENSE)。适用范围包括 `TTLY/`、`PICode/`、`Sound_Rec/`、`tools/`、`tests/` 中的自有部分和本项目新增文档；第三方内容保留各自原有许可与版权标注，范围说明同时见 [NOTICE](../NOTICE)。

## 随仓库保留的三维重建代码

| 内容 | 上游与本地证据 | 本仓库中的边界 |
| --- | --- | --- |
| `SLAM3R/` | [SLAM3R 上游](https://github.com/PKU-VCL-3DV/SLAM3R)；[本地 README](../SLAM3R/README.md)；例如 [models.py 文件头](../SLAM3R/slam3r/models.py) 标注 Naver Corporation 和 `CC BY-NC-SA 4.0 (non-commercial use only)` | 保留上游及继承代码中的逐文件版权、许可标注；独立运行，不属于 Qt 已接通的三维按钮功能。当前目录未附单独的完整 LICENSE 文件，不能将项目根 Apache-2.0 解释为替代这些标注。 |
| `InstantNGP/` | [Instant-NGP 上游](https://github.com/NVlabs/instant-ngp)；[本地 NVIDIA Source Code License-NC](../InstantNGP/LICENSE.txt)；[上游 README 的本地副本](../InstantNGP/README.md) | 保留数据转换脚本、配置和文档。现有许可证第 3.3 节写明非商业研究或评估用途；本项目没有将这些第三方内容改为 Apache-2.0。 |
| `InstantNGP/scripts/flip/` | [FLIP 上游](https://github.com/NVlabs/flip)；例如 [本地文件头](../InstantNGP/scripts/flip/__init__.py) | 源码头附有单独的 BSD 式再分发条款与 NVIDIA 版权说明，按各文件现有文本保留。 |

上述目录中的第三方许可不能因为仓库公开或项目自有部分采用 Apache-2.0 而被统一替换。安装方式与本项目使用入口分别见 [SLAM3R 本地说明](../SLAM3R/LOCAL_SETUP.md) 和 [InstantNGP 本地说明](../InstantNGP/LOCAL_SETUP.md)。

## 声音分类模型与外部特征模型

- `Sound_Rec/animal_classifier_pro.joblib3`：项目作者确认这是本人训练的分类器，允许随项目以 Apache-2.0 公开。文件、类别、特征处理与版本信息见 [模型卡](../Sound_Rec/MODEL_CARD.md)。这项确认针对保留的分类器；训练音频未随仓库发布。
- YAMNet：由声音模块从 [Google YAMNet 模型页](https://tfhub.dev/google/yamnet/1) 下载，或通过 `--yamnet` 加载用户提供的 SavedModel。上游实现见 [TensorFlow Models / YAMNet](https://github.com/tensorflow/models/tree/master/research/audioset/yamnet)。YAMNet 权重和缓存不包含在源码仓库中，保留其模型页及上游自身的许可范围。
- SLAM3R 权重：运行时按上游说明取得 [Image-to-Points](https://huggingface.co/siyan824/slam3r_i2p) 和 [Local-to-World](https://huggingface.co/siyan824/slam3r_l2w) 模型，仓库不包含这些权重。来源与下载入口见 [SLAM3R README](../SLAM3R/README.md)。

## 分别安装的运行依赖

Qt、Python 软件包和外部程序通过各自官方渠道安装，不因被本项目调用而改用项目许可证。主要依赖包括 [Qt](https://www.qt.io/)、[TensorFlow](https://github.com/tensorflow/tensorflow)、[TensorFlow Hub](https://github.com/tensorflow/hub)、[librosa](https://github.com/librosa/librosa)、[scikit-learn](https://github.com/scikit-learn/scikit-learn)、[XGBoost](https://github.com/dmlc/xgboost)、[PyTorch](https://github.com/pytorch/pytorch)、[COLMAP](https://github.com/colmap/colmap) 和 [FFmpeg](https://ffmpeg.org/)。本仓库的依赖清单用于重建运行环境，不替代各软件自身的许可文件。

历史 Windows EXE、DLL、Qt 插件、COLMAP/FFmpeg 分发包及实验场景已移到仓库外，不随源码发布。清理范围见 [清理记录](CLEANUP.md)。训练及测试录音也不包含在公开仓库中。
