#!/usr/bin/env python3
import socket
import serial
import threading
import time
from serial.serialutil import SerialException

# 配置参数
TCP_IP = '0.0.0.0'
TCP_PORT = 8000
SERIAL_PORT = '/dev/ttyS2'
BAUDRATE = 115200
MAX_RETRIES = 5

# 全局变量
tcp_connection = None
connection_lock = threading.Lock()
ser = None
serial_lock = threading.Lock()
running = True

def format_hex(data):
    """将字节数据格式化为十六进制字符串"""
    return ' '.join(f'{b:02X}' for b in data)

def handle_tcp(client_sock):
    global tcp_connection
    with connection_lock:
        print("客户端连接成功:", client_sock.getpeername())
        if tcp_connection:
            try:
                tcp_connection.close()
            except: pass
        tcp_connection = client_sock
    
    try:
        while running:
            data = client_sock.recv(4096)
            if not data:
                break
            
            # 打印原始数据（十六进制和ASCII格式）
            hex_str = format_hex(data)
            
            print(f"\nTCP->UART 原始数据 [{len(data)}字节]")
            print(f"HEX: {hex_str}")

            with serial_lock:
                if ser and ser.is_open:
                    ser.write(data)
    except (SerialException, OSError) as e:
        print("TCP发送失败:", e)
    except Exception as e:
        print("TCP未知错误:", e)
    finally:
        with connection_lock:
            if tcp_connection == client_sock:
                tcp_connection = None
        client_sock.close()

def read_serial():
    global ser, running
    retry_count = 0
    while running:
        try:
            with serial_lock:
                if ser is None or not ser.is_open:
                    if retry_count >= MAX_RETRIES:
                        running = False
                        return
                    try:
                        ser = serial.Serial(
                            port=SERIAL_PORT,
                            baudrate=BAUDRATE,
                            bytesize=serial.EIGHTBITS,
                            parity=serial.PARITY_NONE,
                            stopbits=serial.STOPBITS_ONE,
                            timeout=0.5
                        )
                        retry_count = 0
                        print(f"串口已连接: {SERIAL_PORT}")
                    except Exception as e:
                        retry_count += 1
                        time.sleep(2)
                        continue
                
                data = ser.read_all()
                if data:
                    # 打印串口原始数据
                    hex_str = format_hex(data)
                    
                    print(f"\nUART->TCP 原始数据 [{len(data)}字节]")
                    print(f"HEX: {hex_str}")

                    with connection_lock:
                        if tcp_connection:
                            try:
                                tcp_connection.sendall(data)
                            except: pass
            time.sleep(0.01)
        except (SerialException, OSError) as e:
            print("串口错误:", e)
            with serial_lock:
                if ser and ser.is_open:
                    ser.close()
                    ser = None
            time.sleep(1)
        except Exception as e:
            print("未知错误:", e)
            time.sleep(1)

def main():
    global running
    serial_thread = threading.Thread(target=read_serial, daemon=True)
    serial_thread.start()

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind((TCP_IP, TCP_PORT))
            s.listen(5)
            print(f"服务器已启动: {TCP_PORT}")

            while running:
                try:
                    client, addr = s.accept()
                    print("新连接:", addr)
                    threading.Thread(
                        target=handle_tcp, 
                        args=(client,), 
                        daemon=True
                    ).start()
                except KeyboardInterrupt:
                    running = False
                except Exception as e:
                    print("连接错误:", e)
    finally:
        running = False
        with serial_lock:
            if ser and ser.is_open:
                ser.close()
        print("服务已关闭")

if __name__ == "__main__":
    main()
