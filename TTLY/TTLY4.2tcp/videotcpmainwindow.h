#ifndef VIDEOTCPMAINWINDOW_H
#define VIDEOTCPMAINWINDOW_H
#include <QMainWindow>
#include <QTcpSocket>
#include <QLabel>
#include <QImage>
#include <QFileDialog>
#include <QProcess>
#include <QTimer>
#include <QThread>
#include <QFutureWatcher>
#include <QWaitCondition>
#include <QQueue>
namespace Ui {
class VideoTcpMainWindow;
}

class VideoTcpMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit VideoTcpMainWindow(QWidget *parent = nullptr);
    ~VideoTcpMainWindow();

private:
    QTcpSocket *tcpSocket;            // TCP客户端套接字
    QByteArray frameBuffer;           // 视频帧缓冲区
    QImage currentFrame;              // 当前视频帧
    QTimer *frameTimer;               // 帧处理定时器
    // bool isRecording;                 // 录制状态标志
    QProcess *process;                // 外部进程处理器
    bool isScanning = false;          // 网络扫描状态标志
    quint32 expectedFrameSize = 0;
    // 核心功能方法
    void processFrameData();          // 处理视频帧数据
    void updateVideoDisplay();        // 更新视频显示
    void scanNetworkForOrangePi();    // 扫描局域网中的香橙派

    const int CAMERA_WIDTH = 1280;  // 双目总宽度
    const int CAMERA_HEIGHT = 480;  // 双目高度
    const int EYE_WIDTH = 640;      // 单目宽度

    QTimer *recordTimer;          // 新增：录像定时器

    // QThread saveThread;              // 图像保存专用线程
    // QFutureWatcher<void> saveWatcher;// 异步保存监视器
    // QQueue<QPair<QImage, QImage>> imageQueue; // 图像缓存队列
    // QMutex queueMutex;               // 队列互斥锁
    // QWaitCondition queueNotEmpty;    // 队列非空条件

    // void asyncSaveImages();          // 异步保存方法



private slots:
    void onRecordTimerTimeout();  // 新增：录像定时器的槽函数

    void on_btnConnectPi_clicked();

    void on_btnGetPhoto_clicked();

    void on_btnGetVideo_clicked();

    // 网络通信相关槽函数
    void readTcpData();                                     // 读取TCP数据
    void handleError(QAbstractSocket::SocketError error);   // 错误处理

    void on_btnStartBuil3D_clicked();

private:
    Ui::VideoTcpMainWindow *ui;
};

#endif // VIDEOTCPMAINWINDOW_H
