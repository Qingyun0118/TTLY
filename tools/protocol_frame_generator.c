#include <stdio.h>
#include <stdint.h>
#include <string.h>

// IEEE754浮点转换联合体
typedef union {
    float value;
    uint8_t bytes[4];
} FloatConverter;

// 计算校验和（小端序）
uint16_t calculate_checksum(const uint8_t *data, size_t len) {
    uint16_t sum = 0;
    for(size_t i=0; i<len; i++) {
        sum += data[i];
    }
    return sum;
}

// 发送连接确认帧（功能码0x01）
void send_connection_ack() {
    uint8_t packet[] = {0xAA, 0x01, 0x22, 0x00, 0x00, 0xDD}; 
    const size_t data_len = 3; // AA + 01 + 22
    
    // 计算校验和
    uint16_t checksum = calculate_checksum(packet, data_len);
    packet[3] = checksum & 0xFF;
    packet[4] = (checksum >> 8);
    
    printf("send connection ack frame:");
    for(int i=0; i<sizeof(packet); i++) 
        printf("%02X", packet[i]);
    printf("\n");
}

// 发送IMU和深度数据（功能码0x02）- 完整版本
void send_imu_data() {
    float acc_x, acc_y, acc_z;
    float pitch, roll, yaw;
    float pitch_vel, roll_vel, yaw_vel; // 新增角速度
    float depth;

    // 用户输入部分
    printf("input imu and depth data (space separated):\n");
    printf("acceleration X Y Z (m/s^2): ");
    scanf("%f %f %f", &acc_x, &acc_y, &acc_z);
    printf("pitch roll yaw (deg): ");
    scanf("%f %f %f", &pitch, &roll, &yaw);
    printf("angular velocity pitch roll yaw (deg/s): "); // 新增角速度输入
    scanf("%f %f %f", &pitch_vel, &roll_vel, &yaw_vel);
    printf("depth (cm): ");
    scanf("%f", &depth);

    // 数据转换
    FloatConverter conv_acc_x = {acc_x}, conv_acc_y = {acc_y}, conv_acc_z = {acc_z};
    FloatConverter conv_pitch = {pitch}, conv_roll = {roll}, conv_yaw = {yaw};
    FloatConverter conv_pitch_vel = {pitch_vel};  // 新增角速度转换
    FloatConverter conv_roll_vel = {roll_vel};
    FloatConverter conv_yaw_vel = {yaw_vel};
    FloatConverter conv_depth = {depth};

    // 数据包缓冲区（根据协议调整长度）
    uint8_t packet[45] = {0}; // 1+1+(4*9)+2+1=45
    packet[0] = 0xAA;        // 帧头
    packet[1] = 0x02;        // 功能码

    // 填充加速度数据（小端序）
    memcpy(&packet[2], conv_acc_x.bytes, 4);   // 2-5
    memcpy(&packet[6], conv_acc_y.bytes, 4);   // 6-9
    memcpy(&packet[10], conv_acc_z.bytes, 4);  // 10-13

    // 填充姿态角数据
    memcpy(&packet[14], conv_pitch.bytes, 4);  // 14-17
    memcpy(&packet[18], conv_roll.bytes, 4);   // 18-21
    memcpy(&packet[22], conv_yaw.bytes, 4);    // 22-25

    // 新增：填充角速度数据
    memcpy(&packet[26], conv_pitch_vel.bytes, 4);  // 26-29
    memcpy(&packet[30], conv_roll_vel.bytes, 4);   // 30-33
    memcpy(&packet[34], conv_yaw_vel.bytes, 4);    // 34-37

    // 填充深度数据
    memcpy(&packet[38], conv_depth.bytes, 4);     // 38-41

    // 计算校验和（覆盖帧头到深度数据）
    uint16_t checksum = calculate_checksum(packet, 42); // 2+40=42字节
    packet[42] = checksum & 0xFF;   // 校验和低字节
    packet[43] = (checksum >> 8);   // 校验和高字节
    packet[44] = 0xDD;              // 帧尾

    // 输出数据帧
    printf("send imu and depth data frame:");
    for(int i=0; i<45; i++) 
        printf("%02X", packet[i]);
    printf("\n");
}
// 发送PID参数（新增PID类型选项）
void send_pid_parameters() {
    int pid_type;
    printf("select PID type:\n");
    printf("1. Pitch PID (0x03)\n");
    printf("2. Yaw PID (0x04)\n");
    printf("3. Depth PID (0x05)\n");
    printf("4. Roll PID (0x06)\n");
    printf("5. Side PID (0x07)\n");
    printf("input your choice: ");
    scanf("%d", &pid_type);

    // 根据类型设置功能码
    uint8_t func_code;
    switch(pid_type) {
        case 1: func_code = 0x03; break;
        case 2: func_code = 0x04; break;
        case 3: func_code = 0x05; break;
        case 4: func_code = 0x06; break;
        case 5: func_code = 0x07; break;
        default: 
            printf("invalid PID type!\n");
            return;
    }

    // 输入PID参数
    printf("input pid parameters (space separated):\n");
    float kp, ki, kd;
    printf("Kp Ki Kd: ");
    scanf("%f %f %f", &kp, &ki, &kd);

    // 转换数据
    FloatConverter conv_kp = {kp}, conv_ki = {ki}, conv_kd = {kd};
    
    // 构造数据包
    uint8_t packet[17] = {0};
    packet[0] = 0xAA;
    packet[1] = func_code;  // 动态功能码
    
    // 填充PID参数
    memcpy(&packet[2], conv_kp.bytes, 4);
    memcpy(&packet[6], conv_ki.bytes, 4);
    memcpy(&packet[10], conv_kd.bytes, 4);
    
    // 计算校验和（AA + 功能码 + 数据字段）
    uint16_t checksum = calculate_checksum(packet, 14); // 2+12=14字节
    packet[14] = checksum & 0xFF;
    packet[15] = (checksum >> 8);
    packet[16] = 0xDD;

    printf("send PID parameters frame:");
    for(int i=0; i<17; i++) 
        printf("%02X", packet[i]);
    printf("\n");
}

int main() {
    printf("========= MCU data frame generator =========\n");
    
    int choice;

    while(1) {
        printf("\ninput your choice:\n");
        printf("1. send connection ack\n");
        printf("2. send imu and depth data\n");
        printf("3. send pid parameters\n");
        printf("4. exit\n");
        printf("input your choice:");
        scanf("%d", &choice);

        switch(choice) {
            case 1:
                send_connection_ack();
                break;
            case 2:
                send_imu_data();
                break;
            case 3:
                send_pid_parameters();
                break;
            case 4:
                return 0;
            default:
                printf("invalid option!\n");
        }
    }
    return 0;
}