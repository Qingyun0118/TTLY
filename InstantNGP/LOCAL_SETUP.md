# 本项目中的 InstantNGP 辅助脚本

此目录保留 Instant-NGP 的数据转换脚本、配置和上游说明。运行程序、DLL、COLMAP、FFmpeg 以及历史场景数据放在源码仓库之外；这里不包含可直接启动的 `instant-ngp.exe`。上游资料和版权说明见 [README.md](README.md) 与 [LICENSE.txt](LICENSE.txt)。

## 安装依赖

1. 按上游 README 安装适合本机 GPU 的官方 Instant-NGP。
2. 安装 COLMAP；若从视频抽帧，同时安装 FFmpeg。将程序加入 `PATH`，或在转换命令中传入 `--colmap_binary` 和 `--ffmpeg_binary`。Windows 的 COLMAP 分发目录应整体保留，并优先使用其 `COLMAP.bat` 入口，保留 DLL 和 Qt 插件的查找配置。
3. 在独立 Python 环境中安装脚本依赖：

```bash
conda create -n ttly-nerf python=3.11
conda activate ttly-nerf
python -m pip install -r InstantNGP/requirements.txt
```

以上命令在项目根目录执行；也可使用自行创建的虚拟环境。`colmap2nerf.py` 不再自动从本目录的 `external/` 查找或下载二进制程序。

## 转换自己的数据

在存放场景数据的目录运行，脚本路径使用本机项目位置：

```bash
python /path/to/ttly/InstantNGP/scripts/colmap2nerf.py --video_in capture.mp4 --video_fps 2 --run_colmap --aabb_scale 32
```

已有图片时，改为 `--images images --run_colmap`。工具不在 `PATH` 时，附加实际安装路径，例如 Windows：

```powershell
python C:/projects/ttly/InstantNGP/scripts/colmap2nerf.py --images images --run_colmap --colmap_binary "C:/tools/COLMAP/COLMAP.bat"
```

视频输入还可附加 `--ffmpeg_binary "C:/tools/ffmpeg/bin/ffmpeg.exe"`。生成的 `transforms.json` 与它引用的图片必须一起保留，再用安装好的 Instant-NGP 打开该场景目录。

转换会生成 `images/`、`colmap.db`、`colmap_sparse/`、`colmap_text/` 和 `transforms.json`；默认会询问是否覆盖已有处理结果。原始视频、图片及已有场景需要自行保存，历史实验文件不属于脚本运行依赖。上游 README 的自带 fox 等示例需另行取得示例数据。
