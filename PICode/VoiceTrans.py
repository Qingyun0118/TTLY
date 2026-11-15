import socket
import threading
import pyaudio
import wave

CHUNK = 2048  
FORMAT = pyaudio.paInt16
CHANNELS = 1
RATE = 44100

class AudioServer:
    def __init__(self, host='0.0.0.0', port=8002):
        self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server.bind((host, port))
        self.server.listen(5)  # 增加待处理连接队列长度
        self.is_streaming = False
        self.p = pyaudio.PyAudio()  # 全局只初始化一次

    def handle_client(self, client):
        stream = None
        try:
            # 使用ALSA设备而非PulseAudio（避免Connection refused）
            stream = self.p.open(
                format=FORMAT,
                channels=CHANNELS,
                rate=RATE,
                input=True,
                input_device_index=2,  # 改为正确的ALSA设备索引
                frames_per_buffer=CHUNK,
                start=False,
            )
            
            stream.start_stream()
            self.is_streaming = True
            while self.is_streaming:
                try:
                    data = stream.read(CHUNK, exception_on_overflow=False)
                    client.sendall(data)
                except (ConnectionResetError, BrokenPipeError):
                    print("客户端断开连接")
                    break
                except Exception as e:
                    print(f"数据传输错误: {e}")
                    break
        except Exception as e:
            print(f"客户端处理异常: {e}")
        finally:
            # 仅关闭当前客户端的资源
            if stream is not None:
                if stream.is_active():
                    stream.stop_stream()
                stream.close()
            client.close()
            print("客户端资源已释放")  # 调试信息

    def start(self):
        print("Server started. Waiting for connections...")
        try:
            while True:
                client, addr = self.server.accept()
                print(f"Connected by {addr}")
                thread = threading.Thread(target=self.handle_client, args=(client,))
                thread.daemon = True  # 设为守护线程
                thread.start()
        except Exception as e:
            print(f"服务器异常: {e}")
        finally:
            self.stop()

    def stop(self):
        print("正在关闭服务器...")
        self.is_streaming = False
        self.server.close()
        self.p.terminate()  # 仅在最终退出时终止PyAudio

if __name__ == "__main__":
    server = AudioServer()
    try:
        server.start()
    except KeyboardInterrupt:
        server.stop()
        print("Server stopped.")
