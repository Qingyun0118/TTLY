#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "QSerialPortInfo"
#include <QSerialPort>
#include <QMessageBox>
#include <QDateTime>
#include <qdebug.h>
#include <QtEndian>
#include <qdesktopservices.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // 初始化网络模块
    ui->lineEdit_ip->setText("10.0.0.1");
    ui->lineEdit_port_client->setText("8000");
    ui->lineEdit_Port_server->setText("8000");
    tcpServer = new QTcpServer(this);
    tcpSocket = new QTcpSocket(this);
    //初始化串口
    QStringList serialNamePort;
    serialPort = new QSerialPort(this);
    connect(serialPort,SIGNAL(readyRead()),this,SLOT(manual_serialPortReadyRead()));/*手动连接槽函数*/
    ui->comboBox_Com->clear();
    //通过QSerialPortInfo查找可用串口
    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        ui->comboBox_Com->addItem(info.portName());
    }

    connect(tcpServer, &QTcpServer::newConnection, this, &MainWindow::newConnection);
    connect(tcpSocket, &QTcpSocket::connected, this, &MainWindow::clientConnected);
    connect(tcpSocket, &QTcpSocket::disconnected, this, &MainWindow::clientDisconnected);
    // connect(tcpSocket, &QTcpSocket::readyRead, this, &MainWindow::socketReadyRead);
    connect(tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &MainWindow::handleSocketError);

    // 发送、接收计数清零
    sendNum = 0;
    recvNum = 0;
    // 状态栏
    QStatusBar *sBar = statusBar();
    // 状态栏的收、发计数标签
    lblSendNum = new QLabel(this);
    lblRecvNum = new QLabel(this);
    lblPortState = new QLabel(this);
    lblTCPState= new QLabel(this);

    lblTCPState->setText("网络未连接");
    lblTCPState->setStyleSheet("color:red");

    lblPortState->setText("串口未连接");
    lblPortState->setStyleSheet("color:red");

    // 设置标签最小大小
    lblSendNum->setMinimumSize(100, 20);
    lblRecvNum->setMinimumSize(100, 20);
    lblTCPState->setMinimumSize(400,20);
    lblPortState->setMinimumSize(400, 20);
    setNumOnLabel(lblSendNum, "S: ", sendNum);
    setNumOnLabel(lblRecvNum, "R: ", recvNum);

    lblGamepadState = new QLabel(this);
    lblGamepadState->setMinimumSize(150, 20);
    sBar->addPermanentWidget(lblGamepadState);

    // 从右往左依次添加
    sBar->addPermanentWidget(lblTCPState);
    sBar->addPermanentWidget(lblPortState);
    sBar->addPermanentWidget(lblSendNum);
    sBar->addPermanentWidget(lblRecvNum);

    StatusOfTCPConnectB();//网络连接前

    // 定时发送-定时器
    timSend = new QTimer;
    timSend->setInterval(1000);// 设置默认定时时长1000ms
    connect(timSend, &QTimer::timeout, this, [=](){
        on_btnSend_clicked();
    });


    // 初始化手柄检测
    auto gamepads = QGamepadManager::instance()->connectedGamepads();
    if (!gamepads.isEmpty()) {
        m_gamepad = new QGamepad(*gamepads.begin(), this);
    }

    //定时获取IMU和深度数据
    m_AutoGetIMUDepthDataTimer=new QTimer(this);
    m_AutoGetIMUDepthDataTimer->setInterval(1000);// 设置默认定时时长1000ms
    connect(m_AutoGetIMUDepthDataTimer, &QTimer::timeout, this, [=](){
        on_btnGetIMUDepthData_clicked();
    });

    // 定时器每100ms更新一次手柄数据
    m_gamepadTimer = new QTimer(this);
    connect(m_gamepadTimer, &QTimer::timeout, this, &MainWindow::updateGamepadState);
    m_gamepadTimer->start(300);

    // 监听手柄连接/断开事件
    connect(QGamepadManager::instance(), &QGamepadManager::gamepadConnected,
            this, &MainWindow::onGamepadConnected);
    connect(QGamepadManager::instance(), &QGamepadManager::gamepadDisconnected, this, &MainWindow::onGamepadDisconnected);

    QPixmap logoPixmap(":/ttly.png"); // 替换为你的图片路径（资源文件或绝对路径）
    if (!logoPixmap.isNull()) {
        ui->label_Mylogo->setPixmap(logoPixmap.scaled(200, 200, Qt::KeepAspectRatio));
    }

    // 设置点击打开链接（需要启用交互）
    ui->label_Mylogo->setOpenExternalLinks(true); // 允许打开外部链接
    ui->label_Mylogo->setTextFormat(Qt::RichText); // 启用富文本
    ui->label_Mylogo->setTextInteractionFlags(Qt::TextBrowserInteraction); // 允许交互

    ui->label_Mylogo->installEventFilter(this);

}

MainWindow::~MainWindow()
{
    delete ui;
}


// 在 MainWindow 类中添加事件过滤函数
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == ui->label_Mylogo && event->type() == QEvent::MouseButtonPress) {
        QDesktopServices::openUrl(QUrl("https://qingyun0118.github.io/")); // 替换为你的链接
        return true;
    }
    return QMainWindow::eventFilter(obj, event);
}

/*手动实现接收数据函数*/
void MainWindow::manual_serialPortReadyRead()
{
    QByteArray recBuf = serialPort->readAll();
    QString str_rev;
    m_displayBuffer.append(recBuf); // 原始数据用于显示
    m_parseBuffer.append(recBuf);   // 原始数据用于协议解析

    // 显示原始数据
    processRawDataForDisplay(m_displayBuffer, str_rev);
    m_displayBuffer.clear();
    // 接收字节计数
    recvNum += recBuf.size();
    // 状态栏显示计数值
    setNumOnLabel(lblRecvNum, "R: ", recvNum);

    // 帧解析
    while(m_parseBuffer.size() > 0) {
        if(!m_frameStarted) {
            int startIdx = m_parseBuffer.indexOf(0xAA);
            if(startIdx == -1) {
                m_parseBuffer.clear();
                return;
            }
            m_parseBuffer = m_parseBuffer.mid(startIdx);
            m_frameStarted = true;
        }

        // 检查最小长度：帧头(1) + 功能码(1) + 数据(N) + 校验和(2) + 帧尾(1)
        if(m_parseBuffer.size() < 6) return;

        // 根据功能码判断完整帧长度
        quint8 funcCode = static_cast<quint8>(m_parseBuffer[1]);
        int frameLen = 0;
        switch(funcCode) {
        case 0x01: frameLen = 6;   break;
        case 0x02: frameLen = 45; break; // IMU数据
        case 0x03: case 0x04: case 0x05: case 0x06: case 0x07: frameLen = 17; break; // PID参数
        default:
            m_parseBuffer.clear();
            m_frameStarted = false;
            return;
        }

        if(m_parseBuffer.size() < frameLen) return;

        // 提取完整帧
        QByteArray frame = m_parseBuffer.left(frameLen);
        m_parseBuffer = m_parseBuffer.mid(frameLen);
        m_frameStarted = false;

        processFrame(frame);
    }

}



// 网络连接相关实现
void MainWindow::setupNetworkConnection()
{
    if(ui->groupBox_Client->isEnabled()){ // 客户端模式
        // 如果 tcpSocket 已被释放，则重新创建并绑定信号
        if (!tcpSocket) {
            tcpSocket = new QTcpSocket(this);
            connect(tcpSocket, &QTcpSocket::connected, this, &MainWindow::clientConnected);
            connect(tcpSocket, &QTcpSocket::disconnected, this, &MainWindow::clientDisconnected);
            // connect(tcpSocket, &QTcpSocket::readyRead, this, &MainWindow::socketReadyRead);
            connect(tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
                    this, &MainWindow::handleSocketError);
        }

        QString ip = ui->lineEdit_ip->text();
        quint16 port = ui->lineEdit_port_client->text().toUShort();
        tcpSocket->connectToHost(ip, port);
        lblTCPState->setText("Connecting to " + ip + ":" + QString::number(port));
    }
    else{ // 服务器模式
        quint16 port = ui->lineEdit_Port_server->text().toUShort();
        if(tcpServer->listen(QHostAddress::Any, port)){
            lblTCPState->setText("Listening on port " + QString::number(port));
            lblTCPState->setStyleSheet("color:green");
        }else{
            QMessageBox::critical(this, "Server Error", "Failed to start server");
        }
    }
}

//关闭网络连接处理
void MainWindow::closeNetworkConnection()
{
    if(tcpSocket){
        if(tcpSocket->state() == QAbstractSocket::ConnectedState){
            tcpSocket->disconnectFromHost();
            tcpSocket->deleteLater(); // 重要：释放资源
            tcpSocket = nullptr;
        }
    }
    if(tcpServer->isListening()){
        tcpServer->close();
        // 服务器关闭时重置socket
        if(tcpSocket){
            tcpSocket->abort();
            tcpSocket->deleteLater();
            tcpSocket = nullptr;
        }
    }
    lblTCPState->setText("Disconnected");
    StatusOfTCPConnectB();
}

// 网络事件处理
void MainWindow::newConnection()
{
    if(tcpServer->hasPendingConnections()){
        QTcpSocket *newSocket = tcpServer->nextPendingConnection();
        // 断开旧连接
        if(tcpSocket){
            tcpSocket->disconnect();
            tcpSocket->deleteLater();
        }
        tcpSocket = newSocket; // 更新当前socket

        // connect(tcpSocket, &QTcpSocket::readyRead, this, &MainWindow::socketReadyRead);
        connect(tcpSocket, &QTcpSocket::disconnected, this, [=](){
            lblTCPState->setText("Client disconnected");
            lblTCPState->setStyleSheet("color:red");
            StatusOfTCPConnectB();
        });

        lblTCPState->setText("Client connected");
        StatusOfTCPConnectA();
    }
}

//客户端连接
void MainWindow::clientConnected()
{
    lblTCPState->setText("Connected to server");
    lblTCPState->setStyleSheet("color:green");
    StatusOfTCPConnectA();
}

//客户端断开连接
void MainWindow::clientDisconnected()
{
    lblTCPState->setText("Connection closed");
    lblTCPState->setStyleSheet("color:red");
    StatusOfTCPConnectB();
}

//网络连接前状态
void MainWindow::StatusOfTCPConnectB(void)
{

    // ui->groupBox_Mode->setEnabled(false);
    // ui->groupBox_PID->setEnabled(false);
    // ui->groupBox_DepthIMU->setEnabled(false);
    // ui->groupBox_Send->setEnabled(false);
    // ui->groupBox_SendConfig->setEnabled(false);
    // ui->groupBox_Rev->setEnabled(false);
    // ui->groupBox_RevConfig->setEnabled(false);
    // ui->groupBox_TCPTrans->setEnabled(false);

    // ui->groupBox_ComConfig->setEnabled(false);
}

// 网络连接后状态
void MainWindow::StatusOfTCPConnectA(void)
{
    ui->groupBox_ComConfig->setEnabled(true);
}

// 串口连接前状态
void MainWindow::StatusOfComConnectB(void)
{
    // ui->groupBox_Mode->setEnabled(false);
    // ui->groupBox_PID->setEnabled(false);
    // ui->groupBox_DepthIMU->setEnabled(false);
    // ui->groupBox_Send->setEnabled(false);
    // ui->groupBox_SendConfig->setEnabled(false);
    // ui->groupBox_Rev->setEnabled(false);
    // ui->groupBox_RevConfig->setEnabled(false);
    // ui->groupBox_TCPTrans->setEnabled(false);
}


// 串口连接后状态
void MainWindow::StatusOfComConnectA(void)
{
    ui->groupBox_Mode->setEnabled(true);

}

// 系统连接后状态
void MainWindow::StatusOfSysConnectA(void)
{
    ui->groupBox_PID->setEnabled(true);
    ui->groupBox_DepthIMU->setEnabled(true);
    ui->groupBox_Send->setEnabled(true);
    ui->groupBox_SendConfig->setEnabled(true);
    ui->groupBox_Rev->setEnabled(true);
    ui->groupBox_RevConfig->setEnabled(true);
    ui->groupBox_TCPTrans->setEnabled(true);

}

// 状态栏标签显示计数值
void MainWindow::setNumOnLabel(QLabel *lbl, QString strS, long num)
{
    // 标签显示
    QString str = strS + QString::number(num);  // 直接转换为字符串
    lbl->setText(str);
}

// //网络接收
// void MainWindow::socketReadyRead()
// {

//     QByteArray recBuf = tcpSocket->readAll();
//     QString str_rev;
//     m_displayBuffer.append(recBuf); // 原始数据用于显示
//     m_parseBuffer.append(recBuf);   // 原始数据用于协议解析

//     // 显示原始数据
//     processRawDataForDisplay(m_displayBuffer, str_rev);
//     m_displayBuffer.clear();
//     // 接收字节计数
//     recvNum += recBuf.size();
//     // 状态栏显示计数值
//     setNumOnLabel(lblRecvNum, "R: ", recvNum);

//     // 帧解析
//     while(m_parseBuffer.size() > 0) {
//         if(!m_frameStarted) {
//             int startIdx = m_parseBuffer.indexOf(0xAA);
//             if(startIdx == -1) {
//                 m_parseBuffer.clear();
//                 return;
//             }
//             m_parseBuffer = m_parseBuffer.mid(startIdx);
//             m_frameStarted = true;
//         }

//         // 检查最小长度：帧头(1) + 功能码(1) + 数据(N) + 校验和(2) + 帧尾(1)
//         if(m_parseBuffer.size() < 6) return;

//         // 根据功能码判断完整帧长度
//         quint8 funcCode = static_cast<quint8>(m_parseBuffer[1]);
//         int frameLen = 0;
//         switch(funcCode) {
//         case 0x01: frameLen = 6;   break;
//         case 0x02: frameLen = 45; break; // IMU数据
//         case 0x03: case 0x04: case 0x05: case 0x06: case 0x07: frameLen = 17; break; // PID参数
//         default:
//             m_parseBuffer.clear();
//             m_frameStarted = false;
//             return;
//         }

//         if(m_parseBuffer.size() < frameLen) return;

//         // 提取完整帧
//         QByteArray frame = m_parseBuffer.left(frameLen);
//         m_parseBuffer = m_parseBuffer.mid(frameLen);
//         m_frameStarted = false;

//         processFrame(frame);
//     }
// }

// 数据帧解析函数
void MainWindow::processFrame(const QByteArray &frame) {
    // 基本校验：帧头、帧尾、最小长度:1+1+2+2
    if(frame.size() < 6 || static_cast<quint8>(frame[0]) != 0xAA || static_cast<quint8>(frame[frame.size()-1]) != 0xDD) {
        qDebug() << "Invalid frame format";
        QMessageBox::critical(this, "错误提示", "错误数据帧");
        return;
    }

    // 计算校验和
    quint16 checksum = 0;
    for(int i=0; i<frame.size()-3; i++) { // 排除帧尾和校验字节
        checksum += static_cast<quint8>(frame[i]);
    }
    quint16 recvChecksum = (static_cast<quint8>(frame[frame.size()-2]) << 8) |
                           static_cast<quint8>(frame[frame.size()-3]);

    if(checksum != recvChecksum) {
        qDebug() << "Checksum error";
        QMessageBox::critical(this, "错误提示", "校验和错误");
        return;
    }

    // 解析功能码
    quint8 funcCode = static_cast<quint8>(frame[1]);

    switch(funcCode) {
    case 0x01:
    {
        if(static_cast<quint8>(frame[2])==0x01){
            StatusOfSysConnectA();
            ui->btnMode0->setEnabled(false);
        }
        if(static_cast<quint8>(frame[2])==0x02){
            StatusOfSysConnectA();
            ui->btnMode_test->setEnabled(false);
        }
        if(static_cast<quint8>(frame[2])==0x03){
            StatusOfSysConnectA();
            ui->btnMode_AUV->setEnabled(false);
        }
        if(static_cast<quint8>(frame[2])==0x04){
            StatusOfSysConnectA();
            ui->btnMode_ROV->setEnabled(false);
        }
        break;
    }
        break;
    case 0x02: // IMU和深度数据（57字节）
        if(frame.size() != 45) {
            qDebug() << "Invalid 0x02 frame length";
            QMessageBox::critical(this, "错误提示", "IMU和深度数据帧长度错误");
            return;
        }
        parseIMUData(frame.mid(2, 42));
        break;
    case 0x03: // Pitch PID参数（12字节）
        parsePIDData(frame.mid(2, 14), 0);
        break;
    case 0x04: // Yaw PID参数
        parsePIDData(frame.mid(2, 14), 1);
        break;
    case 0x05: // Depth PID参数
        parsePIDData(frame.mid(2, 14), 2);
        break;
    case 0x06: // Roll PID参数
        parsePIDData(frame.mid(2, 14), 3);
        break;
    case 0x07: // Side PID参数
        parsePIDData(frame.mid(2, 14), 4);
        break;
    default:
        qDebug() << "Unknown function code:" << funcCode;
        QMessageBox::critical(this, "错误提示", "未知功能码");
    }
}

// 解析IMU数据
void MainWindow::parseIMUData(const QByteArray &data) {

    // 加速度解析（每个分量4字节，小端序）
    float accX = byteArrayToFloat(data.mid(0,4));
    float accY = byteArrayToFloat(data.mid(4,4));
    float accZ = byteArrayToFloat(data.mid(8,4));

    // 姿态角解析（每个分量4字节）
    float pitch = byteArrayToFloat(data.mid(12,4));
    float roll = byteArrayToFloat(data.mid(16,4));
    float yaw = byteArrayToFloat(data.mid(20,4));

    // 角速度解析（每个分量4字节）
    float pitchVel = byteArrayToFloat(data.mid(24,4));
    float rollVel = byteArrayToFloat(data.mid(28,4));
    float yawVel = byteArrayToFloat(data.mid(32,4));

    // 深度解析（4字节）
    float depth = byteArrayToFloat(data.mid(36,4));

    // 更新UI
    QMetaObject::invokeMethod(this, [=]{
        ui->lineEdit_AccX->setText(QString::number(accX, 'f'));
        ui->lineEdit_AccY->setText(QString::number(accY, 'f'));
        ui->lineEdit_AccZ->setText(QString::number(accZ, 'f'));

        ui->lineEdit_PitchAngle->setText(QString::number(pitch, 'f'));
        ui->lineEdit_RollAngle->setText(QString::number(roll, 'f'));
        ui->lineEdit_YawAngle->setText(QString::number(yaw, 'f'));

        ui->lineEdit_PitchAv->setText(QString::number(pitchVel, 'f'));
        ui->lineEdit_RollAv->setText(QString::number(rollVel, 'f'));
        ui->lineEdit_YawAv->setText(QString::number(yawVel, 'f'));

        ui->lineEdit_Depth->setText(QString::number(depth, 'f'));
    }, Qt::QueuedConnection);
}

// 解析PID参数 type:0-Pitch 1-Yaw 2-Depth 3-Roll 4-Side
void MainWindow::parsePIDData(const QByteArray &data, int type) {
    QByteArray kpBytes = data.mid(0,4);
    QByteArray kiBytes = data.mid(4,4);
    QByteArray kdBytes = data.mid(8,4);

    float kp = byteArrayToFloat(kpBytes);
    float ki = byteArrayToFloat(kiBytes);
    float kd = byteArrayToFloat(kdBytes);

    QMetaObject::invokeMethod(this, [=]{
        switch(type) {
        case 0: // Pitch
            ui->lineEdit_PKp->setText(QString::number(kp, 'f'));
            ui->lineEdit_PKi->setText(QString::number(ki, 'f'));
            ui->lineEdit_PKd->setText(QString::number(kd, 'f'));
            break;
        case 1: // Yaw
            ui->lineEdit_YKp->setText(QString::number(kp, 'f'));
            ui->lineEdit_YKi->setText(QString::number(ki, 'f'));
            ui->lineEdit_YKd->setText(QString::number(kd, 'f'));
            break;
        case 2: // Depth
            ui->lineEdit_DKp->setText(QString::number(kp, 'f'));
            ui->lineEdit_DKi->setText(QString::number(ki, 'f'));
            ui->lineEdit_DKd->setText(QString::number(kd, 'f'));
            break;
        case 3: // Roll
            ui->lineEdit_RKp->setText(QString::number(kp, 'f'));
            ui->lineEdit_RKi->setText(QString::number(ki, 'f'));
            ui->lineEdit_RKd->setText(QString::number(kd, 'f'));
            break;
        case 4: // Side
            ui->lineEdit_SKp->setText(QString::number(kp, 'f'));
            ui->lineEdit_SKi->setText(QString::number(ki, 'f'));
            ui->lineEdit_SKd->setText(QString::number(kd, 'f'));
            break;
        }
    }, Qt::QueuedConnection);
}



/**
 * @brief 处理原始数据以适配接收区显示需求
 * @param rawData 输入的原始数据（QByteArray）
 * @param outputStr 格式化后的输出字符串（通过引用返回）
 */
void MainWindow::processRawDataForDisplay(const QByteArray &rawData, QString &outputStr) {
    // 初始化输出字符串
    outputStr.clear();

    // 1. 处理十六进制显示逻辑
    if (ui->checkBox_RevHex->isChecked()) {
        // 将原始数据转换为十六进制字符串（大写，每字节用空格分隔）
        QString hexStr = rawData.toHex(' ').toUpper();
        outputStr = hexStr;
    } else {
        // 将原始数据按 Latin-1 编码转换为字符串（兼容二进制数据）
        outputStr = QString::fromLatin1(rawData);
    }

    // 2. 处理时间戳添加逻辑
    if (ui->checkBox_RevTime->isChecked()) {
        QDateTime currentTime = QDateTime::currentDateTime();
        QString timestamp = "[" + currentTime.toString("yyyy-MM-dd hh:mm:ss") + "] ";
        outputStr.prepend(timestamp); // 在字符串前插入时间戳
    }

    // 3. 处理换行符逻辑
    if (ui->checkBox_RevNewLine->isChecked()) {
        outputStr += "\r\n"; // 添加 Windows 风格的换行符
    }
    outputStr+=" ";
    ui->textEdit_Rev->insertPlainText(outputStr);
    ui->textEdit_Rev->moveCursor(QTextCursor::End);
}


//发送按钮点击信号槽  发送使用网络发送
void MainWindow::on_btnSend_clicked()
{
    if(tcpSocket && tcpSocket->state() == QAbstractSocket::ConnectedState){

        QByteArray array;

        //Hex复选框
        if(ui->checkBox_SendHex->checkState() == Qt::Checked){
            //array = QString2Hex(data);  //HEX 16进制
            array = QByteArray::fromHex(ui->textEdit_Send->toPlainText().toUtf8()).data();
        }else{
            //array = data.toLatin1();    //ASCII
            array = ui->textEdit_Send->toPlainText().toLocal8Bit().data();
        }

        if(ui->checkBox_SendNewLine->checkState() == Qt::Checked){
            array.append("\r\n");
        }
        // 如发送成功，会返回发送的字节长度。失败，返回-1。
        int a=tcpSocket->write(array);
        // 发送字节计数并显示
        if(a > 0)
        {
            // 发送字节计数
            sendNum += a;
            // 状态栏显示计数值
            setNumOnLabel(lblSendNum, "S: ", sendNum);
        }
    }

}


//清除发送区按钮点击信号槽
void MainWindow::on_btnClearSend_clicked()
{
    ui->textEdit_Send->clear();
    // 清除发送字节计数
    sendNum = 0;
    // 状态栏显示计数值
    setNumOnLabel(lblSendNum, "S: ", sendNum);
}

//清除接收区按钮点击信号槽
void MainWindow::on_btnClearRev_clicked()
{
    ui->textEdit_Rev->clear();
    // 清除发送、接收字节计数
    sendNum = 0;
    recvNum = 0;
    // 状态栏显示计数值
    setNumOnLabel(lblSendNum, "S: ", sendNum);
    setNumOnLabel(lblRecvNum, "R: ", recvNum);
}


//自动发送复选框
void MainWindow::on_checkBox_AutoSend_stateChanged(int arg1)
{
    // 获取复选框状态
    if(arg1 == 0){
        timSend->stop();
        // 时间输入框恢复可选
        ui->spinBox_autotime->setEnabled(true);
    }else{
        // 对输入的值做限幅，小于10ms会弹出对话框提示
        if(ui->spinBox_autotime->text().toInt() >= 10){
            timSend->start(ui->spinBox_autotime->text().toInt());// 设置定时时长，重新计数
            // 让时间输入框不可选，避免误操作（输入功能不可用，控件背景为灰色）
            ui->spinBox_autotime->setEnabled(false);
        }else{
            ui->checkBox_AutoSend->setCheckState(Qt::Unchecked);
            QMessageBox::critical(this, "错误提示", "定时发送的最小间隔为 10ms\r\n请确保输入的值 >=10");
        }
    }
}


// 手柄连接时触发
void MainWindow::onGamepadConnected(int deviceId) {
    if (!m_gamepad) {
        m_gamepad = new QGamepad(deviceId, this);
        // qDebug() << "Xbox手柄已连接，设备ID:" << deviceId;
        lblGamepadState->setText("手柄已连接");
        lblGamepadState->setStyleSheet("color: green;");
    }
}

// 手柄断开时触发
void MainWindow::onGamepadDisconnected(int deviceId) {
    if (m_gamepad && m_gamepad->deviceId() == deviceId) {
        delete m_gamepad;
        m_gamepad = nullptr;
        // qDebug() << "Xbox手柄已断开，设备ID:" << deviceId;
        lblGamepadState->setText("手柄未连接");
        lblGamepadState->setStyleSheet("color: red;");
    }
}

// 实时获取手柄数据
void MainWindow::updateGamepadState() {
    if (!m_gamepad) return;

    // 按键处理：模式切换
    if (m_gamepad->buttonA()) sendModeCommand(0x01);  // 待机模式
    if (m_gamepad->buttonB()) sendModeCommand(0x02);  // 测试模式
    if (m_gamepad->buttonX()) sendModeCommand(0x03);  // AUV模式
    if (m_gamepad->buttonY()) sendModeCommand(0x04);  // ROV模式
    // if (m_gamepad->buttonR1()) on_btnSetTargetDepth_clicked();
    // 左摇杆方向控制
    double axisLX = m_gamepad->axisLeftX();
    double axisLY = m_gamepad->axisLeftY();
    // sendDirectionCommand(axisLX, axisLY);
    if (abs(axisLX) > 0.05 || abs(axisLY) > 0.05) {
        stopFlag=false;
        sendDirectionCommand(axisLX, axisLY);
        // 调试输出左摇杆值
        // qDebug() << "[Gamepad] Left Stick - X:" << QString::number(axisLX, 'f', 2)
        //          << "Y:" << QString::number(axisLY, 'f', 2);
    }

    if((axisLX==0 && axisLY==0) &&(!stopFlag)){
        stopFlag=true;
        sendDirectionCommand(axisLX, axisLY);
    }

    // R2/L2深度控制
    double triggerR = m_gamepad->buttonR2();  // 范围[0,1]
    double triggerL = m_gamepad->buttonL2();
    // R2触发：增加深度（每次+1，不超过100
    if (triggerR > 0.5) {

        double newDepth = ui->spinBox_TargetDepth->value() + 0.5;
        if (newDepth <= 50) {
            ui->spinBox_TargetDepth->setValue(newDepth);
        }
        on_btnSetTargetDepth_clicked();
    }
    // L2触发：减少深度（每次-0.5，不低于0）
    if (triggerL > 0.5) {
        double newDepth = ui->spinBox_TargetDepth->value() - 0.5;
        if (newDepth >= 0) {
            ui->spinBox_TargetDepth->setValue(newDepth);
        }
        on_btnSetTargetDepth_clicked();
    }

    // 右摇杆旋转方向控制
    double axisRX = m_gamepad->axisRightX();

    if(axisRX < -0.05)
    {
        stopFlagL=false;
        // qDebug()<<axisRX;
        sendRotaDirCommond(axisRX);//逆时针
    }else if(axisRX == 0&& !stopFlagL){
        stopFlagL=true;
        sendRotaDirCommond(axisRX);//停止转动
    }

    if(axisRX > 0.05)
    {
        // qDebug()<<axisRX;
        stopFlagR=false;
        sendRotaDirCommond(0x01);//顺时针
    }else if(axisRX == 0&&!stopFlagR){
        stopFlagR=true;
        sendRotaDirCommond(0x00);//停止转动
    }


    if(m_gamepad->buttonR1()){
        stopFlagP1=false;
        sendSetPitchAngleCommand(0x01);
    }else if(!(m_gamepad->buttonR1())&& !stopFlagP1)
    {
        stopFlagP1=true;
        sendSetPitchAngleCommand(0x00);
    }

    if(m_gamepad->buttonL1()){
        stopFlagP2=false;
        sendSetPitchAngleCommand(0x02);
    }else if(!(m_gamepad->buttonL1())&& !stopFlagP2)
    {
        stopFlagP2=true;
        sendSetPitchAngleCommand(0x00);
    }




    // if(m_gamepad->buttonLeft())
    // {
    //     stopFlagL=false;
    //     sendRotaDirCommond(0x02);//逆时针
    //     // qDebug()<< m_gamepad->buttonRight();
    // }else if(!m_gamepad->buttonLeft()&& !stopFlagL){
    //     stopFlagL=true;
    //     sendRotaDirCommond(0x00);//停止转动
    // }

    // if(m_gamepad->buttonRight())
    // {
    //     stopFlagR=false;
    //     sendRotaDirCommond(0x01);//顺时针
    //     // qDebug()<< m_gamepad->buttonLeft();
    // }else if(!m_gamepad->buttonRight()&&!stopFlagR){
    //     stopFlagR=true;
    //     sendRotaDirCommond(0x00);//停止转动
    // }
}

//浮点数转小端字节序
QByteArray MainWindow::floatToByteArray(float value) {
    QByteArray bytes;
    bytes.resize(4);
    quint32 intValue;
    memcpy(&intValue, &value, sizeof(float));    // 将float内存拷贝到uint32
    quint32 leValue = qToLittleEndian(intValue); // 转换为小端序
    memcpy(bytes.data(), &leValue, 4);           // 拷贝到字节数组
    return bytes;
}

// 小端字节序转浮点数
float MainWindow::byteArrayToFloat(const QByteArray &data) {
    quint32 leValue;
    memcpy(&leValue, data.constData(), 4);      // 拷贝字节到uint32
    quint32 hostValue = qFromLittleEndian(leValue); // 转换为主机序
    float result;
    memcpy(&result, &hostValue, sizeof(float)); // 拷贝回float
    return result;
}

// 计算16位校验和（低字节在前）
QByteArray MainWindow::calculateChecksum16(const QByteArray &data) {
    quint16 sum = 0;
    for (char ch : data) {
        sum += static_cast<quint8>(ch);
    }
    QByteArray checksum;
    checksum.append(static_cast<char>(sum & 0xFF));  // 低字节
    checksum.append(static_cast<char>((sum >> 8) & 0xFF)); // 高字节
    return checksum;
}

// 发送模式设置指令
void MainWindow::sendModeCommand(int mode) {
    QByteArray packet;
    packet.append(0xBB);                     // 帧头
    packet.append(0x02);                     // 功能码（设置模式）
    packet.append(static_cast<char>(mode));   // 模式编号
    QByteArray checksum = calculateChecksum16(packet);
    packet.append(checksum);
    packet.append(0xCC);                     // 帧尾
    // tcpSocket->write(packet);
    serialPort->write(packet);
    // 将发送内容写入textEdit控件（十六进制格式）
    QString displayStr;
    if (ui->checkBox_SendHex->isChecked()) {
        // Hex显示：将每个字节转换为大写十六进制，空格分隔
        displayStr = packet.toHex(' ').toUpper();
    } else {
        ui->checkBox_RevHex->setChecked(true);
    }
    // 更新UI
    ui->textEdit_Send->moveCursor(QTextCursor::End);
    ui->textEdit_Send->insertPlainText(displayStr + "\n");
    if(mode==0x01){
        ui->btnMode0->setEnabled(false);
        ui->btnMode_test->setEnabled(true);
        ui->btnMode_AUV->setEnabled(true);
        ui->btnMode_ROV->setEnabled(true);
    }
    else if(mode==0x02){
        ui->btnMode0->setEnabled(true);
        ui->btnMode_test->setEnabled(false);
        ui->btnMode_AUV->setEnabled(true);
        ui->btnMode_ROV->setEnabled(true);
    }
    else if(mode==0x03){
        ui->btnMode0->setEnabled(true);
        ui->btnMode_test->setEnabled(true);
        ui->btnMode_AUV->setEnabled(false);
        ui->btnMode_ROV->setEnabled(true);
    }
    else if(mode==0x04){
        ui->btnMode0->setEnabled(true);
        ui->btnMode_test->setEnabled(true);
        ui->btnMode_AUV->setEnabled(true);
        ui->btnMode_ROV->setEnabled(false);
    }
}

// 方向控制指令
void MainWindow::sendDirectionCommand(float x, float y) {
    QByteArray packet;
    packet.append(0xBB);                     // 帧头
    packet.append(0x0A);                     // 功能码
    packet.append(floatToByteArray(x));       // X轴（4字节）
    packet.append(floatToByteArray(y));       // Y轴（4字节）

    // 计算校验和（帧头 + 功能码 + 数据字段）
    QByteArray checksum = calculateChecksum16(packet);
    packet.append(checksum);                  // 添加双字节校验和
    packet.append(0xCC);                     // 帧尾

    // tcpSocket->write(packet);
    serialPort->write(packet);
    // 将发送内容写入textEdit控件（十六进制格式）
    QString displayStr;
    if (ui->checkBox_SendHex->isChecked()) {
        // Hex显示：将每个字节转换为大写十六进制，空格分隔
        displayStr = packet.toHex(' ').toUpper();
    } else {
        ui->checkBox_RevHex->setChecked(true);
    }
    // 更新UI
    ui->textEdit_Send->moveCursor(QTextCursor::End);
    ui->textEdit_Send->insertPlainText(displayStr + "\n");
}

void MainWindow::sendRotaDirCommond(float rotaDir)
{
    QByteArray packet;
    packet.append(0xBB);                     // 帧头
    packet.append(0x1A);                     // 功能码（设置模式）
    packet.append(floatToByteArray(rotaDir));       //旋转方向（4字节）
    QByteArray checksum = calculateChecksum16(packet);
    packet.append(checksum);
    packet.append(0xCC);                     // 帧尾
    // tcpSocket->write(packet);
    serialPort->write(packet);
    // 将发送内容写入textEdit控件（十六进制格式）
    QString displayStr;
    if (ui->checkBox_SendHex->isChecked()) {
        // Hex显示：将每个字节转换为大写十六进制，空格分隔
        displayStr = packet.toHex(' ').toUpper();
    } else {
        ui->checkBox_RevHex->setChecked(true);
    }
    // 更新UI
    ui->textEdit_Send->moveCursor(QTextCursor::End);
    ui->textEdit_Send->insertPlainText(displayStr + "\n");
}


void MainWindow::sendMaxthrustCommand(float maxThrust,float maxRotaThrust)
{
    QByteArray packet;
    packet.append(0xBB);                     // 帧头
    packet.append(0x1B);                     // 功能码
    packet.append(floatToByteArray(maxThrust));       // 最大进退横移推力（4字节）
    packet.append(floatToByteArray(maxRotaThrust));       // 最大旋转推力（4字节）

    // 计算校验和（帧头 + 功能码 + 数据字段）
    QByteArray checksum = calculateChecksum16(packet);
    packet.append(checksum);                  // 添加双字节校验和
    packet.append(0xCC);                     // 帧尾

    // tcpSocket->write(packet);
    serialPort->write(packet);
    // 将发送内容写入textEdit控件（十六进制格式）
    QString displayStr;
    if (ui->checkBox_SendHex->isChecked()) {
        // Hex显示：将每个字节转换为大写十六进制，空格分隔
        displayStr = packet.toHex(' ').toUpper();
    } else {
        ui->checkBox_RevHex->setChecked(true);
    }
    // 更新UI
    ui->textEdit_Send->moveCursor(QTextCursor::End);
    ui->textEdit_Send->insertPlainText(displayStr + "\n");
}

// 深度控制指令
void MainWindow::sendDepthCommand(float TargetDepth) {
    QByteArray packet;
    packet.append(0xBB);
    packet.append(0x0B);                     // 功能码（深度控制）
    packet.append(floatToByteArray(TargetDepth));
    QByteArray checksum = calculateChecksum16(packet);//校验和
    packet.append(checksum);
    packet.append(0xCC);                     // 帧尾
    // tcpSocket->write(packet);
    serialPort->write(packet);
    // 将发送内容写入textEdit控件（十六进制格式）
    QString displayStr;
    if (ui->checkBox_SendHex->isChecked()) {
        // Hex显示：将每个字节转换为大写十六进制，空格分隔
        displayStr = packet.toHex(' ').toUpper();
    } else {
        ui->checkBox_RevHex->setChecked(true);
    }
    // 更新UI
    ui->textEdit_Send->moveCursor(QTextCursor::End);
    ui->textEdit_Send->insertPlainText(displayStr + "\n");


}


void MainWindow::sendSetPitchAngleCommand(int pitchAn)
{
    QByteArray packet;
    packet.append(0xBB);                     // 帧头
    packet.append(0x2A);                     // 功能码（设置模式）
    packet.append(static_cast<char>(pitchAn));   //
    QByteArray checksum = calculateChecksum16(packet);
    packet.append(checksum);
    packet.append(0xCC);                     // 帧尾
    // tcpSocket->write(packet);
    serialPort->write(packet);
    // 将发送内容写入textEdit控件（十六进制格式）
    QString displayStr;
    if (ui->checkBox_SendHex->isChecked()) {
        // Hex显示：将每个字节转换为大写十六进制，空格分隔
        displayStr = packet.toHex(' ').toUpper();
    } else {
        ui->checkBox_RevHex->setChecked(true);
    }
    // 更新UI
    ui->textEdit_Send->moveCursor(QTextCursor::End);
    ui->textEdit_Send->insertPlainText(displayStr + "\n");

}


void MainWindow::on_btnModifyPitchPID_clicked()
{
    if((ui->lineEdit_PKp->text().isEmpty()==false)&&(ui->lineEdit_PKi->text().isEmpty()==false)&&(ui->lineEdit_PKd->text().isEmpty()==false)){
    float kp = ui->lineEdit_PKp->text().toFloat();
    float ki = ui->lineEdit_PKi->text().toFloat();
    float kd = ui->lineEdit_PKd->text().toFloat();

    QByteArray packet;
    packet.append(0xBB);                     // 帧头
    packet.append(0x04);                     // 功能码（修改俯仰角PID）
    packet.append(floatToByteArray(kp));     // Kp
    packet.append(floatToByteArray(ki));     // Ki
    packet.append(floatToByteArray(kd));     // Kd
    QByteArray checksum = calculateChecksum16(packet);//校验和
    packet.append(checksum);
    packet.append(0xCC);                     // 帧尾
    // tcpSocket->write(packet);
    serialPort->write(packet);

    on_btnGetPitchPID_clicked();

    // 将发送内容写入textEdit控件（十六进制格式）
    QString displayStr;
    if (ui->checkBox_SendHex->isChecked()) {
        // Hex显示：将每个字节转换为大写十六进制，空格分隔
        displayStr = packet.toHex(' ').toUpper();
    } else {
        ui->checkBox_RevHex->setChecked(true);
    }
    // 更新UI
    ui->textEdit_Send->moveCursor(QTextCursor::End);
    ui->textEdit_Send->insertPlainText(displayStr + "\n");
    }
    else{
        QMessageBox::critical(this, "错误提示", "更改参数为空");
    }
}


void MainWindow::on_btnUpdateYawPID_clicked()
{
    if((ui->lineEdit_YKp->text().isEmpty()==false)&&(ui->lineEdit_YKi->text().isEmpty()==false)&&(ui->lineEdit_YKd->text().isEmpty()==false)){
    float kp = ui->lineEdit_YKp->text().toFloat();
    float ki = ui->lineEdit_YKi->text().toFloat();
    float kd = ui->lineEdit_YKd->text().toFloat();

    QByteArray packet;
    packet.append(0xBB);                     // 帧头
    packet.append(0x06);                     // 功能码（修改俯仰角PID）
    packet.append(floatToByteArray(kp));     // Kp
    packet.append(floatToByteArray(ki));     // Ki
    packet.append(floatToByteArray(kd));     // Kd
    QByteArray checksum = calculateChecksum16(packet);//校验和
    packet.append(checksum);
    packet.append(0xCC);                     // 帧尾
    // tcpSocket->write(packet);
    serialPort->write(packet);

    on_btnGetYawPID_clicked();
    // 将发送内容写入textEdit控件（十六进制格式）
    QString displayStr;
    if (ui->checkBox_SendHex->isChecked()) {
        // Hex显示：将每个字节转换为大写十六进制，空格分隔
        displayStr = packet.toHex(' ').toUpper();
    } else {
        ui->checkBox_RevHex->setChecked(true);
    }
    // 更新UI
    ui->textEdit_Send->moveCursor(QTextCursor::End);
    ui->textEdit_Send->insertPlainText(displayStr + "\n");
    }
    else{
        QMessageBox::critical(this, "错误提示", "更改参数为空");
    }
}


void MainWindow::on_btnUpdateDepthPID_clicked()
{
    if((ui->lineEdit_DKp->text().isEmpty()==false)&&(ui->lineEdit_DKi->text().isEmpty()==false)&&(ui->lineEdit_DKd->text().isEmpty()==false)){
    float kp = ui->lineEdit_DKp->text().toFloat();
    float ki = ui->lineEdit_DKi->text().toFloat();
    float kd = ui->lineEdit_DKd->text().toFloat();

    QByteArray packet;
    packet.append(0xBB);                     // 帧头
    packet.append(0x08);                     // 功能码（修改深度PID）
    packet.append(floatToByteArray(kp));     // Kp
    packet.append(floatToByteArray(ki));     // Ki
    packet.append(floatToByteArray(kd));     // Kd
    QByteArray checksum = calculateChecksum16(packet);//校验和
    packet.append(checksum);
    packet.append(0xCC);                     // 帧尾
    // tcpSocket->write(packet);
    serialPort->write(packet);
    on_btnGetDepthPID_clicked();
    // 将发送内容写入textEdit控件（十六进制格式）
    QString displayStr;
    if (ui->checkBox_SendHex->isChecked()) {
        // Hex显示：将每个字节转换为大写十六进制，空格分隔
        displayStr = packet.toHex(' ').toUpper();
    } else {
        ui->checkBox_RevHex->setChecked(true);
    }
    // 更新UI
    ui->textEdit_Send->moveCursor(QTextCursor::End);
    ui->textEdit_Send->insertPlainText(displayStr + "\n");
    }
    else{
        QMessageBox::critical(this, "错误提示", "更改参数为空");
    }
}




void MainWindow::on_btnMode0_clicked()
{
    sendModeCommand(0x01);  // 待机模式
}


void MainWindow::on_btnMode_test_clicked()
{
    sendModeCommand(0x02);  // 待机模式
}


void MainWindow::on_btnMode_AUV_clicked()
{
    sendModeCommand(0x03);  // 待机模式
}


void MainWindow::on_btnMode_ROV_clicked()
{
    sendModeCommand(0x04);  // 待机模式
}


void MainWindow::on_btnSetTargetDepth_clicked()
{
    sendDepthCommand(ui->spinBox_TargetDepth->value());
}


void MainWindow::on_btnConnect_clicked()
{
    QByteArray packet;
    packet.append(0xBB);                     // 帧头
    packet.append(0x01);                     // 功能码
    packet.append(0x11);                    // 请求连接
    QByteArray checksum = calculateChecksum16(packet);//校验和
    packet.append(checksum);
    packet.append(0xCC);                     // 帧尾
    // tcpSocket->write(packet);
    serialPort->write(packet);

    // 将发送内容写入textEdit控件（十六进制格式）
    QString displayStr;
    if (ui->checkBox_SendHex->isChecked()) {
        // Hex显示：将每个字节转换为大写十六进制，空格分隔
        displayStr = packet.toHex(' ').toUpper();
    } else {
        ui->checkBox_RevHex->setChecked(true);
    }
    // 更新UI
    ui->textEdit_Send->moveCursor(QTextCursor::End);
    ui->textEdit_Send->insertPlainText(displayStr + "\n");
}


void MainWindow::on_btnGetIMUDepthData_clicked()
{
    QByteArray packet;
    packet.append(0xBB);                     // 帧头
    packet.append(0x09);                     // 功能码
    QByteArray checksum = calculateChecksum16(packet);//校验和
    packet.append(checksum);
    packet.append(0xCC);                     // 帧尾
    // tcpSocket->write(packet);
    serialPort->write(packet);

    // 将发送内容写入textEdit控件（十六进制格式）
    QString displayStr;
    if (ui->checkBox_SendHex->isChecked()) {
        // Hex显示：将每个字节转换为大写十六进制，空格分隔
        displayStr = packet.toHex(' ').toUpper();
    } else {
        ui->checkBox_RevHex->setChecked(true);
    }
    // 更新UI
    ui->textEdit_Send->moveCursor(QTextCursor::End);
    ui->textEdit_Send->insertPlainText(displayStr + "\n");
}


void MainWindow::on_btnGetPitchPID_clicked()
{
    QByteArray packet;
    packet.append(0xBB);                     // 帧头
    packet.append(0x03);                     // 功能码
    QByteArray checksum = calculateChecksum16(packet);//校验和
    packet.append(checksum);
    packet.append(0xCC);                     // 帧尾
    // tcpSocket->write(packet);
    serialPort->write(packet);
    // 将发送内容写入textEdit控件（十六进制格式）
    QString displayStr;
    if (ui->checkBox_SendHex->isChecked()) {
        // Hex显示：将每个字节转换为大写十六进制，空格分隔
        displayStr = packet.toHex(' ').toUpper();
    } else {
        ui->checkBox_RevHex->setChecked(true);
    }
    // 更新UI
    ui->textEdit_Send->moveCursor(QTextCursor::End);
    ui->textEdit_Send->insertPlainText(displayStr + "\n");
}


void MainWindow::on_btnGetYawPID_clicked()
{
    QByteArray packet;
    packet.append(0xBB);                     // 帧头
    packet.append(0x05);                     // 功能码
    QByteArray checksum = calculateChecksum16(packet);//校验和
    packet.append(checksum);
    packet.append(0xCC);                     // 帧尾
    // tcpSocket->write(packet);
    serialPort->write(packet);

    // 将发送内容写入textEdit控件（十六进制格式）
    QString displayStr;
    if (ui->checkBox_SendHex->isChecked()) {
        // Hex显示：将每个字节转换为大写十六进制，空格分隔
        displayStr = packet.toHex(' ').toUpper();
    } else {
        ui->checkBox_RevHex->setChecked(true);
    }
    // 更新UI
    ui->textEdit_Send->moveCursor(QTextCursor::End);
    ui->textEdit_Send->insertPlainText(displayStr + "\n");
}


void MainWindow::on_btnGetDepthPID_clicked()
{
    QByteArray packet;
    packet.append(0xBB);                     // 帧头
    packet.append(0x07);                     // 功能码
    QByteArray checksum = calculateChecksum16(packet);//校验和
    packet.append(checksum);
    packet.append(0xCC);                     // 帧尾
    // tcpSocket->write(packet);
    serialPort->write(packet);

    // 将发送内容写入textEdit控件（十六进制格式）
    QString displayStr;
    if (ui->checkBox_SendHex->isChecked()) {
        // Hex显示：将每个字节转换为大写十六进制，空格分隔
        displayStr = packet.toHex(' ').toUpper();
    } else {
        ui->checkBox_RevHex->setChecked(true);
    }
    // 更新UI
    ui->textEdit_Send->moveCursor(QTextCursor::End);
    ui->textEdit_Send->insertPlainText(displayStr + "\n");
}


void MainWindow::on_btnGetRollPID_clicked()
{
    QByteArray packet;
    packet.append(0xBB);                     // 帧头
    packet.append(0x0C);                     // 功能码
    QByteArray checksum = calculateChecksum16(packet);//校验和
    packet.append(checksum);
    packet.append(0xCC);                     // 帧尾
    // tcpSocket->write(packet);
    serialPort->write(packet);

    // 将发送内容写入textEdit控件（十六进制格式）
    QString displayStr;
    if (ui->checkBox_SendHex->isChecked()) {
        // Hex显示：将每个字节转换为大写十六进制，空格分隔
        displayStr = packet.toHex(' ').toUpper();
    } else {
        ui->checkBox_RevHex->setChecked(true);
    }
    // 更新UI
    ui->textEdit_Send->moveCursor(QTextCursor::End);
    ui->textEdit_Send->insertPlainText(displayStr + "\n");
}


void MainWindow::on_btnUpdateRollPID_clicked()
{
    if((ui->lineEdit_RKp->text().isEmpty()==false)&&(ui->lineEdit_RKi->text().isEmpty()==false)&&(ui->lineEdit_RKd->text().isEmpty()==false)){
        float kp = ui->lineEdit_RKp->text().toFloat();
        float ki = ui->lineEdit_RKi->text().toFloat();
        float kd = ui->lineEdit_RKd->text().toFloat();

        QByteArray packet;
        packet.append(0xBB);                     // 帧头
        packet.append(0x0D);                     // 功能码（修改深度PID）
        packet.append(floatToByteArray(kp));     // Kp
        packet.append(floatToByteArray(ki));     // Ki
        packet.append(floatToByteArray(kd));     // Kd
        QByteArray checksum = calculateChecksum16(packet);//校验和
        packet.append(checksum);
        packet.append(0xCC);                     // 帧尾
        // tcpSocket->write(packet);
        serialPort->write(packet);
        on_btnGetRollPID_clicked();
        // 将发送内容写入textEdit控件（十六进制格式）
        QString displayStr;
        if (ui->checkBox_SendHex->isChecked()) {
            // Hex显示：将每个字节转换为大写十六进制，空格分隔
            displayStr = packet.toHex(' ').toUpper();
        } else {
            ui->checkBox_RevHex->setChecked(true);
        }
        // 更新UI
        ui->textEdit_Send->moveCursor(QTextCursor::End);
        ui->textEdit_Send->insertPlainText(displayStr + "\n");
    }
    else{
        QMessageBox::critical(this, "错误提示", "更改参数为空");
    }
}


void MainWindow::on_btnGetSidePID_clicked()
{
    QByteArray packet;
    packet.append(0xBB);                     // 帧头
    packet.append(0x0E);                     // 功能码
    QByteArray checksum = calculateChecksum16(packet);//校验和
    packet.append(checksum);
    packet.append(0xCC);                     // 帧尾
    // tcpSocket->write(packet);
    serialPort->write(packet);

    // 将发送内容写入textEdit控件（十六进制格式）
    QString displayStr;
    if (ui->checkBox_SendHex->isChecked()) {
        // Hex显示：将每个字节转换为大写十六进制，空格分隔
        displayStr = packet.toHex(' ').toUpper();
    } else {
        ui->checkBox_RevHex->setChecked(true);
    }
    // 更新UI
    ui->textEdit_Send->moveCursor(QTextCursor::End);
    ui->textEdit_Send->insertPlainText(displayStr + "\n");
}


void MainWindow::on_btnUpdateSidePID_clicked()
{
    if((ui->lineEdit_SKp->text().isEmpty()==false)&&(ui->lineEdit_SKi->text().isEmpty()==false)&&(ui->lineEdit_SKd->text().isEmpty()==false)){
        float kp = ui->lineEdit_SKp->text().toFloat();
        float ki = ui->lineEdit_SKi->text().toFloat();
        float kd = ui->lineEdit_SKd->text().toFloat();

        QByteArray packet;
        packet.append(0xBB);                    // 帧头
        packet.append(0x0F);                     // 功能码（修改深度PID）
        packet.append(floatToByteArray(kp));     // Kp
        packet.append(floatToByteArray(ki));     // Ki
        packet.append(floatToByteArray(kd));     // Kd
        QByteArray checksum = calculateChecksum16(packet);//校验和
        packet.append(checksum);
        packet.append(0xCC);                     // 帧尾
        // tcpSocket->write(packet);
        serialPort->write(packet);
        on_btnGetSidePID_clicked();
        // 将发送内容写入textEdit控件（十六进制格式）
        QString displayStr;
        if (ui->checkBox_SendHex->isChecked()) {
            // Hex显示：将每个字节转换为大写十六进制，空格分隔
            displayStr = packet.toHex(' ').toUpper();
        } else {
            ui->checkBox_RevHex->setChecked(true);
        }
        // 更新UI
        ui->textEdit_Send->moveCursor(QTextCursor::End);
        ui->textEdit_Send->insertPlainText(displayStr + "\n");
    }
    else{
        QMessageBox::critical(this, "错误提示", "更改参数为空");
    }
}




void MainWindow::on_checkBox_AutoGetIMUDepthData_stateChanged(int arg1)
{
    // 获取复选框状态
    if(arg1 == 0){
        m_AutoGetIMUDepthDataTimer->stop();
        // 时间输入框恢复可选
        ui->spinBox_AutoGetDataTime->setEnabled(true);
    }else{
        // 对输入的值做限幅，小于50ms会弹出对话框提示
        if(ui->spinBox_AutoGetDataTime->text().toInt() >= 50){
            m_AutoGetIMUDepthDataTimer->start(ui->spinBox_AutoGetDataTime->text().toInt());// 设置定时时长，重新计数
            // 让时间输入框不可选，避免误操作（输入功能不可用，控件背景为灰色）
            ui->spinBox_AutoGetDataTime->setEnabled(false);
        }else{
            ui->checkBox_AutoGetIMUDepthData->setCheckState(Qt::Unchecked);
            QMessageBox::critical(this, "错误提示", "定时发送的最小间隔为 50ms\r\n请确保输入的值 >=50");
        }
    }
}




void MainWindow::on_checkBox_SendHex_stateChanged(int arg1)
{
    // 获取当前发送框内容
    QString text = ui->textEdit_Send->toPlainText();
    if (text.isEmpty()) return;

    // 断开信号槽，防止修改内容时触发递归
    disconnect(ui->textEdit_Send, &QTextEdit::textChanged, nullptr, nullptr);

    if (arg1 == Qt::Checked) { // 勾选Hex发送
        // 将ASCII文本转为Hex字符串（例如 "AB" -> "41 42"）
        QByteArray asciiData = text.toLocal8Bit();
        QString hexStr = asciiData.toHex(' ').toUpper();
        ui->textEdit_Send->setPlainText(hexStr);
    } else { // 取消Hex发送
        // 将Hex字符串转为ASCII文本（例如 "41 42" -> "AB"）
        QByteArray hexData = QByteArray::fromHex(text.toUtf8());
        QString asciiStr = QString::fromLocal8Bit(hexData);
        ui->textEdit_Send->setPlainText(asciiStr);
    }

    // 重新连接信号槽
    connect(ui->textEdit_Send, &QTextEdit::textChanged, this, [this]() {
        if (ui->checkBox_SendHex->isChecked()) {
            // 输入时自动过滤非Hex字符
            QString text = ui->textEdit_Send->toPlainText();
            QString filtered = text.toUpper().remove(QRegularExpression("[^0-9A-F ]"));
            if (text != filtered) {
                ui->textEdit_Send->setPlainText(filtered);
            }
        }
    });
}


void MainWindow::on_checkBox_RevHex_stateChanged(int arg1)
{
    // 获取当前接收框内容
    QString originalText = ui->textEdit_Rev->toPlainText();
    if (originalText.isEmpty()) return;

    // 断开信号槽，防止修改内容时触发递归
    disconnect(ui->textEdit_Rev, &QTextEdit::textChanged, nullptr, nullptr);

    if (arg1 == Qt::Checked) { // 勾选Hex显示
        // 将ASCII显示内容转为Hex格式（例如 "AB" -> "41 42"）
        QByteArray asciiData = originalText.toLocal8Bit();
        QString hexStr = asciiData.toHex(' ').toUpper();
        ui->textEdit_Rev->setPlainText(hexStr);
    } else { // 取消Hex显示
        // 将Hex内容转为ASCII（例如 "41 42" -> "AB"）
        QByteArray hexData = QByteArray::fromHex(originalText.toUtf8());
        QString asciiStr = QString::fromLocal8Bit(hexData);
        ui->textEdit_Rev->setPlainText(asciiStr);
    }

    // 重新连接信号槽
    connect(ui->textEdit_Rev, &QTextEdit::textChanged, this, [this]() {
        if (ui->checkBox_SendHex->isChecked()) {
            // 输入时自动过滤非Hex字符
            QString text = ui->textEdit_Send->toPlainText();
            QString filtered = text.toUpper().remove(QRegularExpression("[^0-9A-F ]"));
            if (text != filtered) {
                ui->textEdit_Send->setPlainText(filtered);
            }
        }
    });
}


void MainWindow::on_btnSwitchConnectPi_clicked()
{
    if(ui->btnSwitchConnectPi->text() == "连接香橙派"){
        setupNetworkConnection();
        ui->btnSwitchConnectPi->setText("断开连接");
        // 禁用网络配置控件
        ui->lineEdit_ip->setEnabled(false);
        ui->lineEdit_port_client->setEnabled(false);
    }else{
        closeNetworkConnection();
        ui->btnSwitchConnectPi->setText("连接香橙派");
        // 启用网络配置控件
        ui->lineEdit_ip->setEnabled(true);
        ui->lineEdit_port_client->setEnabled(true);
    }
}


void MainWindow::on_btnSwitchServe_clicked()
{
    if(ui->btnSwitchServe->text() == "打开服务器"){
        setupNetworkConnection();
        ui->btnSwitchServe->setText("关闭服务器");
    }else{
        closeNetworkConnection();
        ui->btnSwitchServe->setText("打开服务器");
    }
}


void MainWindow::on_btnSwitchClient2Serve_clicked()
{
    if(ui->groupBox_Client->isEnabled()){
        ui->groupBox_Client->setDisabled(true);
    }else{
        ui->groupBox_Client->setDisabled(false);
    }

    if(ui->groupBox_Serve->isEnabled()){
        ui->groupBox_Serve->setDisabled(true);
    }else{
        ui->groupBox_Serve->setDisabled(false);
    }
}

void MainWindow::handleSocketError(QAbstractSocket::SocketError error)
{
    QString errorDetail;
    switch(error){
    case QAbstractSocket::ConnectionRefusedError:
        errorDetail = "连接被拒绝（服务未运行）";
        tcpSocket->disconnectFromHost();
        ui->btnSwitchConnectPi->setText("连接香橙派");
        ui->lineEdit_ip->setEnabled(true);
        ui->lineEdit_port_client->setEnabled(true);
        break;
    case QAbstractSocket::HostNotFoundError:
        errorDetail = "主机不存在";
        tcpSocket->disconnectFromHost();
        ui->btnSwitchConnectPi->setText("连接香橙派");
        ui->lineEdit_ip->setEnabled(true);
        ui->lineEdit_port_client->setEnabled(true);
        break;
    case QAbstractSocket::RemoteHostClosedError:
        errorDetail = "远程主机已关闭连接";
        tcpSocket->disconnectFromHost();
        ui->btnSwitchConnectPi->setText("连接香橙派");
        ui->lineEdit_ip->setEnabled(true);
        ui->lineEdit_port_client->setEnabled(true);
        break;
    case QAbstractSocket::NetworkError:
        errorDetail = "网络错误，请检查连接";
        tcpSocket->disconnectFromHost();
        ui->btnSwitchConnectPi->setText("连接香橙派");
        ui->lineEdit_ip->setEnabled(true);
        ui->lineEdit_port_client->setEnabled(true);
        break;
    default:
        errorDetail = tcpSocket->errorString();
    }

    lblTCPState->setText("错误: " + errorDetail);
    tcpSocket->abort();
    ui->btnConnect->setEnabled(true);
}




void MainWindow::on_btnOpenVideoTrans_clicked()
{
    if (!tcpWindow_video) {
        tcpWindow_video = new VideoTcpMainWindow(this); // 创建TCP窗口
        tcpWindow_video->setAttribute(Qt::WA_DeleteOnClose); // 关闭时自动释放内存
        connect(tcpWindow_video, &QObject::destroyed, [this]() { tcpWindow_video = nullptr; }); // 窗口关闭后指针置空
    }
    tcpWindow_video->show(); // 显示窗口
}


void MainWindow::on_btnOpenVoiceTrans_clicked()
{
    if (!tcpWindow_voice) {
        tcpWindow_voice = new VoiceTcpMainWindow(this); // 创建TCP窗口
        tcpWindow_voice->setAttribute(Qt::WA_DeleteOnClose); // 关闭时自动释放内存
        connect(tcpWindow_voice, &QObject::destroyed, [this]() { tcpWindow_voice = nullptr; }); // 窗口关闭后指针置空
    }
    tcpWindow_voice->show(); // 显示窗口
}

//开关串口按钮点击信号槽
void MainWindow::on_btnSwitchCom_clicked()
{

    // 初始化默认值（例如：115200波特率，8数据位，1停止位，无校验）
    QSerialPort::BaudRate baudRate = QSerialPort::Baud115200;
    QSerialPort::DataBits dataBits = QSerialPort::Data8;
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    QSerialPort::Parity checkBits = QSerialPort::NoParity;

    // 获取串口波特率
    // baudRate = ui->baundrateCb->currentText().toInt();直接字符串转换为 int 的方法

    if(ui->comboBox_Baud->currentText()=="1200")
        baudRate=QSerialPort::Baud1200;
    else if(ui->comboBox_Baud->currentText()=="2400")
        baudRate=QSerialPort::Baud2400;
    else if(ui->comboBox_Baud->currentText()=="4800")
        baudRate=QSerialPort::Baud4800;
    else if(ui->comboBox_Baud->currentText()=="9600")
        baudRate=QSerialPort::Baud9600;
    else if(ui->comboBox_Baud->currentText()=="19200")
        baudRate=QSerialPort::Baud19200;
    else if(ui->comboBox_Baud->currentText()=="38400")
        baudRate=QSerialPort::Baud38400;
    else if(ui->comboBox_Baud->currentText()=="57600")
        baudRate=QSerialPort::Baud57600;
    else if(ui->comboBox_Baud->currentText()=="115200")
        baudRate=QSerialPort::Baud115200;

    // 获取串口数据位
    if(ui->comboBox_DataBit->currentText()=="5")
        dataBits=QSerialPort::Data5;
    else if(ui->comboBox_DataBit->currentText()=="6")
        dataBits=QSerialPort::Data6;
    else if(ui->comboBox_DataBit->currentText()=="7")
        dataBits=QSerialPort::Data7;
    else if(ui->comboBox_DataBit->currentText()=="8")
        dataBits=QSerialPort::Data8;

    // 获取串口停止位
    if(ui->comboBox_StopBit->currentText()=="1")
        stopBits=QSerialPort::OneStop;
    else if(ui->comboBox_StopBit->currentText()=="1.5")
        stopBits=QSerialPort::OneAndHalfStop;
    else if(ui->comboBox_StopBit->currentText()=="2")
        stopBits=QSerialPort::TwoStop;

    // 获取串口奇偶校验位
    if(ui->comboBox_PriorityBit->currentText() == "none"){
        checkBits = QSerialPort::NoParity;
    }else if(ui->comboBox_PriorityBit->currentText() == "奇校验"){
        checkBits = QSerialPort::OddParity;
    }else if(ui->comboBox_PriorityBit->currentText() == "偶校验"){
        checkBits = QSerialPort::EvenParity;
    }else{

    }

    // 初始化串口属性，设置 端口号、波特率、数据位、停止位、奇偶校验位数
    serialPort->setPortName(ui->comboBox_Com->currentText());
    serialPort->setBaudRate(baudRate);
    serialPort->setDataBits(dataBits);
    serialPort->setStopBits(stopBits);
    serialPort->setParity(checkBits);

    // 根据初始化好的串口属性，打开串口
    // 如果打开成功，反转打开按钮显示和功能。打开失败，无变化，并且弹出错误对话框。
    if(ui->btnSwitchCom->text() == "打开串口"){
        if(serialPort->open(QIODevice::ReadWrite) == true){
            //QMessageBox::
            ui->btnSwitchCom->setText("关闭串口");
            // 让端口号下拉框不可选，避免误操作（选择功能不可用，控件背景为灰色）
            ui->comboBox_Com->setEnabled(false);
            ui->comboBox_Baud->setEnabled(false);
            ui->comboBox_DataBit->setEnabled(false);
            ui->comboBox_StopBit->setEnabled(false);
            ui->comboBox_PriorityBit->setEnabled(false);
            StatusOfComConnectA();

        }else{
            QMessageBox::critical(this, "错误提示", "串口打开失败！！！\r\n该串口可能被占用\r\n请选择正确的串口");
        }
        //statusBar 状态栏显示端口状态
        QString sm = "%1 OPENED, %2, 8, NONE, 1";
        QString status = sm.arg(serialPort->portName()).arg(serialPort->baudRate());
        lblPortState->setText(status);
        lblPortState->setStyleSheet("color:green");
    }else{
        serialPort->close();
        ui->btnSwitchCom->setText("打开串口");
        // 端口号下拉框恢复可选，避免误操作
        ui->comboBox_Com->setEnabled(true);
        ui->comboBox_Baud->setEnabled(true);
        ui->comboBox_DataBit->setEnabled(true);
        ui->comboBox_StopBit->setEnabled(true);
        ui->comboBox_PriorityBit->setEnabled(true);

        ui->checkBox_AutoGetIMUDepthData->setCheckState(Qt::Unchecked);
        ui->checkBox_AutoSend->setCheckState(Qt::Unchecked);

        StatusOfComConnectB();
        //statusBar 状态栏显示端口状态
        QString sm = "%1 CLOSED";
        QString status = sm.arg(serialPort->portName());
        lblPortState->setText(status);
        lblPortState->setStyleSheet("color:red");
    }

}

//
void MainWindow::on_btnComCheck_clicked()
{
    ui->comboBox_Com->clear();
    //通过QSerialPortInfo查找可用串口
    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        ui->comboBox_Com->addItem(info.portName());
    }
}


void MainWindow::on_btnUpdatePower_clicked()
{
    sendMaxthrustCommand(ui->spinBox_MAX_FORWARD_SIDESWAY_THRUST->value(),ui->spinBox_MAX_ROATE_THRUST->value());
}

