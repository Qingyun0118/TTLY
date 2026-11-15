# 机器人端服务

这些 Python 脚本运行在连接串口、USB 双目相机和音频输入设备的 Linux 设备上，由 Qt 上位机通过 TCP 连接。

| 脚本 | 默认端口 | 用途 |
| --- | --- | --- |
| `Control.py` | 8000 | TCP 与 `/dev/ttyS2`（115200 波特率）双向透传 |
| `VideoTrans.py` | 8001 | OpenCV/V4L2 采集双目视频，发送 4 字节大端帧长度和 JPEG 帧 |
| `VoiceTrans.py` | 8002 | PyAudio 采集并发送 44.1 kHz、16 bit、单声道 PCM |

## 环境与运行

使用 Python 3.10 创建独立环境，并安装 `PICode/requirements.txt`。PyAudio 需要系统提供 PortAudio；没有可用 wheel 时，还需 PortAudio 开发头文件及编译工具。OpenCV 需要可用的 V4L2 相机；串口和录音设备需要当前用户具有相应访问权限。

在项目根目录分别启动三个服务：

```bash
python -m pip install -r PICode/requirements.txt
python PICode/Control.py
python PICode/VideoTrans.py
python PICode/VoiceTrans.py
```

启动前按实际硬件核对脚本中的串口路径、相机索引、音频设备索引及端口。视频默认请求 2560×720、30 fps；音频默认输入设备索引为 `2`。相机是否支持该分辨率和帧率由硬件决定。

依赖使用版本范围，因为原目录没有可核对的设备端环境清单；安装完成并完成硬件联调后，可保存设备实际使用的版本。
