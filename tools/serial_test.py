import serial

# 配置串口
ser = serial.Serial(
    port='/dev/ttyS2',    # 根据实际设备名修改
    baudrate=115200,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS,
    timeout=1
)

# 发送数据
ser.write(b"Hello GD32!\n")

# 接收数据
while True:
    if ser.in_waiting > 0:
        data = ser.readline().decode().strip()
        print(f"Received: {data}")