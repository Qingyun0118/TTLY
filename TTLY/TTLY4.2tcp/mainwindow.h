#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "videotcpmainwindow.h"
#include "voicetcpmainwindow.h"
#include <QMainWindow>
#include <QTcpSocket>
#include <QTcpServer>
#include <QString>
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QTimer>
#include <QPainter>
#include <QtGamepad/QGamepad>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QSerialPort *serialPort;//定义串口指针  串口用来接收数据


private slots:

    // 网络相关槽函数
    void newConnection();
    void clientConnected();
    void clientDisconnected();
    // void socketReadyRead();

    //数据帧处理相关函数
    void processFrame(const QByteArray &frame);
    void parseIMUData(const QByteArray &data);
    void parsePIDData(const QByteArray &data, int type);

    //手柄相关函数
    void onGamepadConnected(int deviceId);     // 手柄连接事件
    void onGamepadDisconnected(int deviceId);  // 手柄断开事件
    void updateGamepadState();                 // 更新手柄数据

    //连接状态
    /*网络连接前后*/
    void StatusOfTCPConnectB(void);
    void StatusOfTCPConnectA(void);
    /*串口连接前后*/
    void StatusOfComConnectB(void);
    void StatusOfComConnectA(void);

    /*系统连接后*/
    void StatusOfSysConnectA(void);

    //数据处理相关函数
    QByteArray floatToByteArray(float value);
    float byteArrayToFloat(const QByteArray &data);
    QByteArray calculateChecksum16(const QByteArray &data);

    //发送指令相关函数
    void sendModeCommand(int mode);
    void sendDirectionCommand(float x, float y);
    void sendDepthCommand(float TargetDepth);
    void sendRotaDirCommond(float rotaDir);
    void sendMaxthrustCommand(float maxThrust,float maxRotaThrust);
    void sendSetPitchAngleCommand(int pitchAn);
    void processRawDataForDisplay(const QByteArray &rawData, QString &outputStr);

    //手动连接槽函数  串口接收显示
    void manual_serialPortReadyRead();

    //发送按钮点击事件
    void on_btnSend_clicked();

    //清除发送区
    void on_btnClearSend_clicked();

    //清除接收区
    void on_btnClearRev_clicked();

    //自动发送
    void on_checkBox_AutoSend_stateChanged(int arg1);

    //修改PID参数按钮
    void on_btnModifyPitchPID_clicked();
    void on_btnUpdateYawPID_clicked();
    void on_btnUpdateDepthPID_clicked();
    void on_btnUpdateRollPID_clicked();
    void on_btnGetSidePID_clicked();
    void on_btnUpdateSidePID_clicked();

    //获取pid参数按钮
    void on_btnGetPitchPID_clicked();
    void on_btnGetYawPID_clicked();
    void on_btnGetDepthPID_clicked();
    void on_btnGetRollPID_clicked();

    //模式选择按钮
    void on_btnMode0_clicked();
    void on_btnMode_test_clicked();
    void on_btnMode_AUV_clicked();
    void on_btnMode_ROV_clicked();

    //设置目标深度按钮
    void on_btnSetTargetDepth_clicked();

    //系统连接请求按钮
    void on_btnConnect_clicked();

    //IMU和深度数据获取按钮
    void on_btnGetIMUDepthData_clicked();

    //自动获取IMU和深度数据
    void on_checkBox_AutoGetIMUDepthData_stateChanged(int arg1);

    //16进制发送
    void on_checkBox_SendHex_stateChanged(int arg1);
    //16进制接收
    void on_checkBox_RevHex_stateChanged(int arg1);

    //香橙派连接按钮
    void on_btnSwitchConnectPi_clicked();

    //关闭服务器按钮
    void on_btnSwitchServe_clicked();

    //切换服务器和客户端模式按钮
    void on_btnSwitchClient2Serve_clicked();

    //网络连接错误处理函数
    void handleSocketError(QAbstractSocket::SocketError error);

    //打开网络视频传输界面按钮
    void on_btnOpenVideoTrans_clicked();
    //打开网络声音传输界面按钮
    void on_btnOpenVoiceTrans_clicked();

    //打开串口按钮
    void on_btnSwitchCom_clicked();
    //串口检测按钮
    void on_btnComCheck_clicked();

    void on_btnUpdatePower_clicked();

private:
    Ui::MainWindow *ui;
    // QByteArray m_recvBuffer; // 接收缓冲区

    QTcpServer *tcpServer = nullptr;
    QTcpSocket *tcpSocket = nullptr;
    bool isServerMode = false;


    bool stopFlag=false;
    bool stopFlagL=false;
    bool stopFlagR=false;

    bool stopFlagP1=false;
    bool stopFlagP2=false;

    // 修改网络连接相关方法
    void setupNetworkConnection();
    void closeNetworkConnection();

    QByteArray m_displayBuffer; // 用于显示原始数据
    QByteArray m_parseBuffer;   // 用于协议帧解析
    bool m_frameStarted = false;
    // 发送、接收字节计数
    long sendNum, recvNum;
    QLabel *lblSendNum;
    QLabel *lblRecvNum;
    QLabel *lblPortState;
    QLabel *lblTCPState;
    QLabel *lblGamepadState;  // 手柄状态标签
    void setNumOnLabel(QLabel *lbl, QString strS, long num);

    // 定时发送-定时器
    QTimer *timSend;
    QGamepad *m_gamepad = nullptr;            // 手柄对象
    QTimer *m_gamepadTimer;                    // 定时读取手柄数据
    QTimer  *m_AutoGetIMUDepthDataTimer;        //定时读取IMU和深度数据

    VideoTcpMainWindow *tcpWindow_video = nullptr; // 指向视频传输窗口的指针
    VoiceTcpMainWindow *tcpWindow_voice = nullptr; // 指向声音传输窗口的指针

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
};
#endif // MAINWINDOW_H
