# 本项目中的 SLAM3R

SLAM3R 在独立 Python 环境中运行，与 Qt 上位机和声音识别环境分开安装。本目录保留上游源码、说明和版权标注；上游安装及模型资料见 [README.md](README.md)。

## 环境

以下版本沿用本目录 README 的配置；PyTorch 的 CUDA 构建需与本机环境匹配：

```bash
conda create -n slam3r python=3.11 cmake=3.14.0
conda activate slam3r
cd SLAM3R
python -m pip install torch==2.5.0 torchvision==0.20.0 torchaudio==2.5.0 --index-url https://download.pytorch.org/whl/cu118
python -m pip install -r requirements.txt
```

可视化与数据预处理的额外依赖见 `requirements_optional.txt`。XFormers 和 CUDA RoPE 扩展是上游说明中的可选加速项；需要时按 README 编译。构建中间目录无需随源码分发，RoPE 扩展缺失时源码会退回 PyTorch 实现。

## 入口和模型

在 `SLAM3R/` 目录启动交互界面：

```bash
python app.py
```

界面支持上传图片、视频或图片目录。默认使用 CUDA。启动会加载 Hugging Face 上的 `siyan824/slam3r_i2p` 和 `siyan824/slam3r_l2w`；首次使用需能下载权重，离线运行前应先准备缓存。仓库不包含模型权重。

命令行重建自己的图像序列：

```bash
python recon.py --img_dir /path/to/images --test_name my_scene
```

命令行可用 `--i2p_weights`、`--l2w_weights` 指定本地检查点，并用 `--save_dir` 更改默认的 `results/` 输出位置。`scripts/demo_*.sh` 引用的上游示例数据需按 README 另行下载。

## 可重新生成的文件

`__pycache__/`、`slam3r/pos_embed/curope/build/` 是运行缓存和编译中间文件。`app.py` 默认将上传缓存及临时重建数据放在 `tmp/`，可用 `--tmp_dir` 修改；目录不存在时会自动创建。关闭程序后，可在保存所需结果后清理缓存。`results/` 中的命令行输出同样可以通过重新运行重建生成，但删除会丢失现有结果。
