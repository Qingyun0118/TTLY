# 推理链路示例

`smoke.wav` 是本项目生成的 1 秒、440 Hz 正弦信号，16 kHz、单声道、16 位 PCM，采用项目 Apache-2.0 许可。它用于检查模型加载、特征提取和分类输出是否可执行。

这个信号没有海洋动物类别标签，输出类别不用于评估识别准确率。真实音频验证结果见 [验证记录](../../docs/VALIDATION.md)。

在项目根目录运行：

```bash
"$TTLY_PYTHON" Sound_Rec/soundimprove.py --yamnet "$PWD/Sound_Rec/.cache/yamnet" --audio examples/audio/smoke.wav
```
