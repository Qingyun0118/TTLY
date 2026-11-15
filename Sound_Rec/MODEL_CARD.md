# 海洋动物声音分类器模型卡

## 文件与来源

| 项目 | 内容 |
| --- | --- |
| 文件 | `animal_classifier_pro.joblib3` |
| 发布文件大小 | 10,746,309 字节，gzip 无损封装，`joblib.load` 自动读取 |
| 发布文件 SHA-256 | `356d6c58b6865492d5d37da3c31ef19e2833466ad65155806964505531643701` |
| 解压后的原始大小 | 28,579,201 字节 |
| 原始内容 SHA-256 | `b9219e82cd5af63a37f883672bd4fb1876ec0c4c0831865be827bd3704a8385f` |
| 内容 | XGBoost 分类器、scikit-learn `StandardScaler` 和 `LabelEncoder` |
| 内嵌版本信息 | scikit-learn 1.6.0、XGBoost 2.1.4；运行依赖见 [requirements.txt](requirements.txt) |
| 作者与发布许可 | 项目作者确认模型由本人训练，允许以项目的 [Apache-2.0](../LICENSE) 许可公开 |

模型沿用整理前的已训练分类器，没有重新训练。为缩小发布文件，原始序列化内容使用 gzip 无损封装，文件名仍为 `.joblib3`；joblib 根据文件头自动识别压缩格式，无需手工解压。两种封装加载后，12 个类别、2048 维特征、scaler 数组和 Booster 参数均已核对一致。其来源说明基于现有文件、[soundimprove.py](soundimprove.py) 和作者确认。训练音频未随公开仓库发布；本卡不提供未验证的训练/验证拆分统计或准确率。

## 输入与特征

识别脚本将音频读为 **16 kHz 单声道**。窗口长度为 15,600 个采样点（0.975 秒），帧移为 5,000 个采样点（0.3125 秒）；短音频补零到一个窗口，长音频使用完整滑动窗口。

每个窗口经 [YAMNet](https://tfhub.dev/google/yamnet/1) 得到 1,024 维嵌入。脚本沿嵌入的时间维分别取均值和标准差，再拼接为 **2,048 维特征**。长音频推理会对各窗口特征取均值，随后依次通过保存的 `StandardScaler` 与 XGBoost 分类器。训练代码则以各窗口的特征及对应录音类别训练分类器。

YAMNet 是单独的特征提取模型，不在这个 `.joblib3` 文件中，也不随源码仓库分发。首次运行默认从上述模型页下载；离线使用方法见 [声音模块说明](README.md)。

## 类别与输出

当前代码定义以下 12 个类别，英文名称同时用作重新训练时的数据子目录名称：

| ID | 类别 |
| --- | --- |
| 0 | Bearded Seal |
| 1 | Beluga White Whale |
| 2 | Harp Seal |
| 3 | Humpback Whale |
| 4 | Killer Whale |
| 5 | Long-Finned Pilot Whale |
| 6 | Northern Right Whale |
| 7 | Pantropical Spotted Dolphin |
| 8 | Ross Seal |
| 9 | Sperm Whale |
| 10 | Walrus |
| 11 | White-beaked Dolphin |

脚本输出得分最高的两个类别及分类器的 `predict_proba` 数值，例如 `- Harp Seal (0.95)`；数值显示为两位小数。这是当前 12 类范围内的分类结果，代码没有“未知动物”类别或额外拒识步骤。

## 使用与验证

```bash
python Sound_Rec/soundimprove.py --audio /path/to/recording.wav
python Sound_Rec/soundimprove.py --yamnet /path/to/yamnet_saved_model --audio /path/to/recording.wav
```

模型环境按 [requirements.txt](requirements.txt) 安装，详细参数见 [声音模块说明](README.md)。重新训练必须显式传入 `--train`；需保留当前模型时，使用 `--model` 为新模型指定其他文件名。

实际构建、模型推理的环境与执行结果统一记录于 [项目验证记录](../docs/VALIDATION.md)。完成单次推理不等同于数据集准确率评估，本卡不报告未经测量的准确率或泛化指标。
