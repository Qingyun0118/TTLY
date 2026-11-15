import cv2
import socket
import struct
import time
import threading
import os

class VideoStreamer:
    def __init__(self, host, port, camera_index=0):
        self.host = host
        self.port = port
        self.camera_index = camera_index
        self.running = False
        self.socket = None
        self.cap = None
        self.client_conn = None
        
    def initialize_camera(self):
        """初始化USB双目相机，强制使用V4L2后端"""
        # 强制使用V4L2后端
        os.environ['OPENCV_VIDEOIO_PRIORITY_V4L2'] = '1'
        os.environ['OPENCV_VIDEOIO_BACKEND'] = 'V4L2'
        
        # 尝试打开相机
        self.cap = cv2.VideoCapture(self.camera_index, cv2.CAP_V4L2)
        # 设置MJPG格式（关键步骤）
        self.cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc('M','J','P','G'))
        if not self.cap.isOpened():
            print("Error: Could not open camera.")
            return False
            
        # 设置相机参数（根据实际相机调整）
        self.cap.set(cv2.CAP_PROP_FPS, 30)
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, 2560)  # 双目的总宽度
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)  # 双目的高度
       
        
        # 验证设置是否成功
        actual_width = self.cap.get(cv2.CAP_PROP_FRAME_WIDTH)
        actual_height = self.cap.get(cv2.CAP_PROP_FRAME_HEIGHT)
        actual_fps = self.cap.get(cv2.CAP_PROP_FPS)
        print(f"Camera initialized: {actual_width}x{actual_height} @ {actual_fps}fps")
        return True
        
    def start_streaming(self):
        """开始视频流传输"""
        if not self.initialize_camera():
            return
            
        try:
            # 创建新的socket并设置地址重用
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            
            self.socket.bind((self.host, self.port))
            self.socket.listen(1)
            print(f"Server started on {self.host}:{self.port}")
            
            while not self.running:
                print("Waiting for connection...")
                self.client_conn, addr = self.socket.accept()
                print(f"Connected by {addr}")
                self.running = True
                
                try:
                    while self.running:
                        ret, frame = self.cap.read()
                        if not ret: 
                            print("Failed to grab frame")
                            break
                        
                        # 编码为JPEG
                        _, buffer = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, 70])
                        frame_data = buffer.tobytes()
                        
                        try:
                            # 发送帧长度（4字节网络字节序）和帧数据
                            self.client_conn.sendall(struct.pack('!I', len(frame_data)))
                            self.client_conn.sendall(frame_data)
                        except (ConnectionResetError, BrokenPipeError):
                            print("Client disconnected")
                            self.running = False
                            break
                            
                        # 控制帧率
                        # time.sleep(0.033)  # ~30fps
                        
                except Exception as e:
                    print(f"Streaming error: {e}")
                    
        except Exception as e:
            print(f"Server error: {e}")
        finally:
            self.cleanup()
            
    def cleanup(self):
        """清理资源"""
        print("Cleaning up resources...")
        self.running = False
        
        if self.client_conn:
            try:
                self.client_conn.close()
            except:
                pass
                
        if self.cap:
            self.cap.release()
            
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
                
        print("Server stopped")

if __name__ == "__main__":
    HOST = '0.0.0.0'  # 监听所有接口
    PORT = 8001
    
    # 检查端口是否被占用
    try:
        test_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        test_socket.bind((HOST, PORT))
        test_socket.close()
    except socket.error as e:
        print(f"Error: Port {PORT} is already in use. Please kill the process using it or choose another port.")
        exit(1)
    
    streamer = VideoStreamer(HOST, PORT, camera_index=0)
    
    try:
        streamer.start_streaming()
    except KeyboardInterrupt:
        print("\nUser interrupted")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        streamer.cleanup()
