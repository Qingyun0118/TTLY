# 辅助工具

这些工具不参与上位机的正常启动。

- `protocol_frame_generator.c`：交互式生成 MCU 连接确认、IMU/深度、PID 数据帧。用 `gcc protocol_frame_generator.c -o frame_generator` 编译，再运行生成器。
- `serial_test.py`：手动测试串口，使用前修改脚本中的设备路径；依赖 `pyserial`，可从 `PICode/requirements.txt` 安装。
- `rename_audio.bat`：Windows 音频文件批量重命名工具，显式传入目标目录，例如 `rename_audio.bat "C:\recordings"`。它会修改目标目录中的音频文件名，按需对副本使用。

WAV 切割和拼接工具保留在 `Sound_Rec/cut.py`、`Sound_Rec/pinjie.py`，参见声音模块说明。
