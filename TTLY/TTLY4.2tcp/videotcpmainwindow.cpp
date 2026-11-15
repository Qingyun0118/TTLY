#include "videotcpmainwindow.h"
#include "../common/runtimepaths.h"
#include "ui_videotcpmainwindow.h"
#include <QDateTime>
#include <QBuffer>
#include <QTimer>
#include <QNetworkInterface>
#include <QHostInfo>
#include <QtConcurrent/QtConcurrent>
#include <qmessagebox.h>

VideoTcpMainWindow::VideoTcpMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , tcpSocket(new QTcpSocket(this))
    , frameTimer(new QTimer(this))
    , process(new QProcess(this))
    , recordTimer(new QTimer(this))  // 新增：初始化录像定时器
    , ui(new Ui::VideoTcpMainWindow)
{
    ui->setupUi(this);
    // 初始化UI默认值
    ui->lineEdit_IP->setText("10.0.0.1");  // 默认IP提示
    ui->lineEdit_Port->setText("8001");                  // 默认端口
    ui->lineEdit_SavePath_photo->setText(TTLYRuntime::path("Result/image"));//拍照图像存储路径
    ui->lineEdit_SavePath_video->setText(TTLYRuntime::path("Result/images"));//录像图像存储路径
    ui->lineEdit_SavePath_3d->setText(TTLYRuntime::path("Result/3d"));//3d点云文件路径

    // 连接信号槽
    connect(tcpSocket, &QTcpSocket::readyRead, this, &VideoTcpMainWindow::readTcpData);
    // 新增信号连接
    connect(tcpSocket, &QTcpSocket::connected, this, [this](){
        // qDebug() << "Connected!";
        ui->btnConnectPi->setText("断开连接");
        ui->btnConnectPi->setEnabled(true);
        ui->label_ConnectStatus->setText("连接成功");
        frameTimer->start();

        ui->lineEdit_IP->setEnabled(false);
        ui->lineEdit_Port->setEnabled(false);

    });
    connect(tcpSocket, &QTcpSocket::errorOccurred, this, &VideoTcpMainWindow::handleError);
    connect(tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &VideoTcpMainWindow::handleError);
    connect(ui->btnGetPhoto, &QPushButton::clicked, this, &VideoTcpMainWindow::on_btnGetPhoto_clicked);

    // 视频帧定时器设置
    frameTimer->setInterval(33); // 约30fps
    connect(frameTimer, &QTimer::timeout, this, &VideoTcpMainWindow::processFrameData);

    // 设置预期分辨率
    tcpSocket->setReadBufferSize(CAMERA_WIDTH * CAMERA_HEIGHT * 3); // 预留缓冲区

    // 配置录像定时器（每秒触发一次）
    recordTimer->setInterval(500); // 500ms = 0.5秒
    connect(recordTimer, &QTimer::timeout, this, &VideoTcpMainWindow::onRecordTimerTimeout);

    // // 配置保存线程
    // connect(&saveWatcher, &QFutureWatcher<void>::finished, [](){
    //     qDebug() << "图像保存线程结束";
    // });


}

VideoTcpMainWindow::~VideoTcpMainWindow()
{
    delete ui;
    tcpSocket->disconnectFromHost();
    frameTimer->stop();
    recordTimer->stop();  // 新增：停止录像定时器
    delete frameTimer;
    delete recordTimer;   // 新增：释放录像定时器

    // 停止保存线程
    // saveWatcher.cancel();
    // saveWatcher.waitForFinished();
}

void VideoTcpMainWindow::on_btnConnectPi_clicked()
{
    // 断开连接处理
    if(tcpSocket->state() == QAbstractSocket::ConnectedState){
        tcpSocket->disconnectFromHost();
        ui->btnConnectPi->setText("连接香橙派");
        ui->label_ConnectStatus->setText("连接已断开");
        ui->lineEdit_IP->setEnabled(true);
        ui->lineEdit_Port->setEnabled(true);
        frameTimer->stop();
        return;
    }

    QString ip = ui->lineEdit_IP->text();
    QString portStr = ui->lineEdit_Port->text();

    if(ip.isEmpty() || portStr.isEmpty()){
        QMessageBox::warning(this, "错误", "请输入有效的IP地址和端口号");
        return;
    }

    bool ok;
    quint16 port = portStr.toUShort(&ok);
    if(!ok || port < 1024){
        QMessageBox::warning(this, "错误", "端口号无效（1024-65535）");
        return;
    }

    // 开始连接
    ui->btnConnectPi->setEnabled(false);
    ui->label_ConnectStatus->setText("正在连接...");
    qApp->processEvents();

    tcpSocket->connectToHost(ip, port);

    // 设置5秒超时
    QTimer *timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer, &QTimer::timeout, this, [this](){
        if(tcpSocket->state() == QAbstractSocket::ConnectingState){
            tcpSocket->abort();
            ui->label_ConnectStatus->setText("错误: 连接超时");
            ui->btnConnectPi->setEnabled(true);
        }
        sender()->deleteLater(); // 删除定时器
    });
    timeoutTimer->start(5000);


}

void VideoTcpMainWindow::readTcpData() {
    // 连接成功时的处理
    if(tcpSocket->state() == QAbstractSocket::ConnectedState && !frameTimer->isActive()){
        ui->btnConnectPi->setText("断开连接");
        ui->btnConnectPi->setEnabled(true);
        ui->label_ConnectStatus->setText("连接成功");
        frameTimer->start();
    }
    while (tcpSocket->bytesAvailable() > 0) {
        if (expectedFrameSize == 0) {
            // 等待帧长度头（4字节）
            if (tcpSocket->bytesAvailable() < 4) return;

            QByteArray sizeData = tcpSocket->read(4);
            expectedFrameSize = qFromBigEndian<quint32>(sizeData.constData());
        }

        // 等待完整帧数据
        if (tcpSocket->bytesAvailable() < expectedFrameSize) return;

        // 读取帧数据
        QByteArray frameData = tcpSocket->read(expectedFrameSize);
        expectedFrameSize = 0;

        currentFrame = QImage::fromData(frameData, "JPEG");
        if (!currentFrame.isNull()) {
            updateVideoDisplay();

            // 录制处理（优化后的异步版本）
            // if (isRecording) {
            //     // 深拷贝当前帧数据
            //     QImage frameCopy = currentFrame.copy();

            //     // 分割左右眼图像
            //     QImage leftImage = frameCopy.copy(QRect(0, 0, 640, 480));
            //     QImage rightImage = frameCopy.copy(QRect(640, 0, 640, 480));

            //     // 加锁操作队列
            //     {
            //         QMutexLocker locker(&queueMutex);
            //         imageQueue.enqueue(qMakePair(leftImage, rightImage));
            //         queueNotEmpty.wakeOne();
            //     }

            //     // 启动异步保存（首次启动）
            //     if (!saveWatcher.isRunning()) {
            //         QFuture<void> future = QtConcurrent::run(this, &VideoTcpMainWindow::asyncSaveImages);
            //         saveWatcher.setFuture(future);
            //     }
            // }
        }

    }
}

// 新增异步保存方法
// void VideoTcpMainWindow::asyncSaveImages()
// {
//     QString basePath = ui->lineEdit_SavePath_video->text();

//     // 预创建目录结构
//     QDir baseDir(basePath);
//     baseDir.mkpath("left");
//     baseDir.mkpath("right");

//     while(true) {
//         QPair<QImage, QImage> images;

//         // 加锁获取图像数据
//         {
//             QMutexLocker locker(&queueMutex);
//             while(imageQueue.isEmpty()) {
//                 // 等待最多100ms防止死锁
//                 if(!queueNotEmpty.wait(&queueMutex, 100)) {
//                     if(imageQueue.isEmpty()) return; // 退出条件
//                 }
//             }
//             images = imageQueue.dequeue();
//         }

//         // 生成文件名
//         QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
//         QString leftPath = QString("%1/left/frame_%2.jpg").arg(basePath).arg(timestamp);
//         QString rightPath = QString("%1/right/frame_%3.jpg").arg(basePath).arg(timestamp);

//         // 使用更高效的保存方式
//         QFile leftFile(leftPath);
//         QFile rightFile(rightPath);

//         // 批量写入（减少IO次数）
//         if(leftFile.open(QIODevice::WriteOnly)) {
//             images.first.save(&leftFile, "JPEG", 100);
//             leftFile.close();
//         }
//         if(rightFile.open(QIODevice::WriteOnly)) {
//             images.second.save(&rightFile, "JPEG", 100);
//             rightFile.close();
//         }

//         // 控制保存频率（可选）
//         QThread::msleep(50); // 根据实际性能调整
//     }
// }


// 处理视频帧数据
void VideoTcpMainWindow::processFrameData()
{
    // 这个函数现在由定时器触发，确保即使没有新数据也会尝试处理
    if (!currentFrame.isNull()) {
        updateVideoDisplay();
    }
}

// 更新视频显示
void VideoTcpMainWindow::updateVideoDisplay()
{
    QPixmap pixmap = QPixmap::fromImage(currentFrame);
    pixmap = pixmap.scaled(ui->label_VideoPlayer->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ui->label_VideoPlayer->setPixmap(pixmap);
}


// 错误处理
void VideoTcpMainWindow::handleError(QAbstractSocket::SocketError error)
{
    QString errorDetail;
    switch(error){
    case QAbstractSocket::ConnectionRefusedError:
        errorDetail = "连接被拒绝（服务未运行）";
        tcpSocket->disconnectFromHost();
        ui->btnConnectPi->setText("连接香橙派");
        ui->lineEdit_IP->setEnabled(true);
        ui->lineEdit_Port->setEnabled(true);
        break;
    case QAbstractSocket::HostNotFoundError:
        errorDetail = "主机不存在";
        tcpSocket->disconnectFromHost();
        ui->btnConnectPi->setText("连接香橙派");
        ui->lineEdit_IP->setEnabled(true);
        ui->lineEdit_Port->setEnabled(true);
        break;
    case QAbstractSocket::RemoteHostClosedError:
        errorDetail = "远程主机已关闭连接";
        tcpSocket->disconnectFromHost();
        ui->btnConnectPi->setText("连接香橙派");
        ui->lineEdit_IP->setEnabled(true);
        ui->lineEdit_Port->setEnabled(true);
        break;
    case QAbstractSocket::NetworkError:
        errorDetail = "网络错误，请检查连接";
        tcpSocket->disconnectFromHost();
        ui->btnConnectPi->setText("连接香橙派");
        ui->lineEdit_IP->setEnabled(true);
        ui->lineEdit_Port->setEnabled(true);
        break;
    default:
        errorDetail = tcpSocket->errorString();
    }

    ui->label_ConnectStatus->setText("错误: " + errorDetail);
    frameTimer->stop();
    tcpSocket->abort();
    ui->btnConnectPi->setEnabled(true);
}


void VideoTcpMainWindow::on_btnGetPhoto_clicked()
{
    if (currentFrame.isNull()) {
        QMessageBox::warning(this, "警告", "当前无视频帧可保存");
        return;
    }

    // // 验证原始帧尺寸（双目相机总分辨率应为1280x480）
    // if(currentFrame.width() != 1280 || currentFrame.height() != 480){
    //     QMessageBox::warning(this, "错误", "视频帧尺寸不符合双目相机要求(1280x480)");
    //     return;
    // }

    // 分割左右图像
    QImage leftImage = currentFrame.copy(QRect(0, 0, CAMERA_WIDTH/2, CAMERA_HEIGHT));    // 左半部分
    QImage rightImage = currentFrame.copy(QRect(CAMERA_WIDTH/2, 0, CAMERA_WIDTH/2, CAMERA_HEIGHT)); // 右半部分

    // // 获取保存路径
    // QString basePath = ui->lineEdit_Savepath_picture->text();
    // if(basePath.isEmpty()){
    //     basePath = QFileDialog::getExistingDirectory(this, "选择保存目录");
    //     if(basePath.isEmpty()) return;
    //     ui->lineEdit_Savepath_picture->setText(basePath);
    // }

    // // 创建目录（如果不存在）
    // QDir().mkpath(basePath);

    // // 生成带时间戳的文件名
    // QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    // QString leftPath = basePath + "/left_" + timestamp + ".jpg";
    // QString rightPath = basePath + "/right_" + timestamp + ".jpg";

    // // 保存图像
    // bool saveSuccess = true;
    // saveSuccess &= leftImage.save(leftPath, "JPEG", 100);
    // saveSuccess &= rightImage.save(rightPath, "JPEG", 100);

    // if(saveSuccess){
    //     QMessageBox::information(this, "成功",
    //                              QString("照片保存成功：\n左眼：%1\n右眼：%2").arg(leftPath).arg(rightPath));
    // } else {
    //     QMessageBox::critical(this, "错误", "部分文件保存失败，请检查存储权限和路径");
    // }

    // 获取保存路径
    QString basePath = ui->lineEdit_SavePath_photo->text();
    if(basePath.isEmpty()){
        basePath = QFileDialog::getExistingDirectory(this, "选择保存目录");
        if(basePath.isEmpty()) return;
        ui->lineEdit_SavePath_photo->setText(basePath);
    }

    // 创建左右眼子目录
    QString leftDir = basePath + "/left/";
    QString rightDir = basePath + "/right/";
    QDir().mkpath(leftDir);  // 递归创建左目录
    QDir().mkpath(rightDir); // 递归创建右目录

    // 生成带时间戳的文件名
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    QString leftPath = leftDir + "frame_" + timestamp + ".jpg";  // 左眼路径
    QString rightPath = rightDir + "frame_" + timestamp + ".jpg"; // 右眼路径

    // 保存图像
    bool saveSuccess = true;
    saveSuccess &= leftImage.save(leftPath, "JPEG", 100);    // 保存左眼
    saveSuccess &= rightImage.save(rightPath, "JPEG", 100);  // 保存右眼

    if(saveSuccess){
        // QMessageBox::information(this, "成功",
        //                          QString("照片保存成功：\n左眼：%1\n右眼：%2").arg(leftPath).arg(rightPath));
        ui->label_StatusPhoto->setText("照片保存成功！"+timestamp);
    } else {
        // QMessageBox::critical(this, "错误", "部分文件保存失败，请检查：\n1. 磁盘剩余空间\n2. 目录写入权限\n3. 特殊字符路径");
        ui->label_StatusPhoto->setText("照片保存失败！");
    }
}


void VideoTcpMainWindow::on_btnGetVideo_clicked()
{
    // if(ui->btnGetVideo->text()=="开始录制"){
    //     ui->btnGetVideo->setText("停止录制");
    //     isRecording=true;
    //     // QMessageBox::information(this,"成功","开始录制");
    // }
    // else{
    //     ui->btnGetVideo->setText("开始录制");
    //     isRecording=false;
    //     // QMessageBox::information(this,"成功","停止录制");
    // }

    if (ui->btnGetVideo->text() == "开始录制") {
        ui->btnGetVideo->setText("停止录制");
        recordTimer->start();  // 启动定时器
    } else {
        ui->btnGetVideo->setText("开始录制");
        recordTimer->stop();  // 停止定时器

    }
}

void VideoTcpMainWindow::onRecordTimerTimeout()
{
    // 如果当前帧有效，直接调用拍照逻辑
    if (!currentFrame.isNull()) {
        // 使用录像路径（lineEdit_SavePath_video）
        QString basePath = ui->lineEdit_SavePath_video->text();
        if (basePath.isEmpty()) {
            ui->label_StatusPhoto->setText("错误：未设置录像路径");
            return;
        }

        // 分割左右图像（直接复用拍照逻辑）
        QImage leftImage = currentFrame.copy(QRect(0, 0, CAMERA_WIDTH/2, CAMERA_HEIGHT));
        QImage rightImage = currentFrame.copy(QRect(CAMERA_WIDTH/2, 0, CAMERA_WIDTH/2, CAMERA_HEIGHT));

        // 创建目录
        QDir(basePath).mkpath("left");
        QDir(basePath).mkpath("right");

        // 生成文件名
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
        QString leftPath = QString("%1/left/frame_%2.jpg").arg(basePath).arg(timestamp);
        QString rightPath = QString("%1/right/frame_%2.jpg").arg(basePath).arg(timestamp);

        // 保存图像
        bool saveSuccess = true;
        saveSuccess &= leftImage.save(leftPath, "JPEG", 100);
        saveSuccess &= rightImage.save(rightPath, "JPEG", 100);

        if (saveSuccess) {
            ui->label_StatusVideo->setText("录像帧保存成功！"+timestamp);
        } else {
            ui->label_StatusVideo->setText("录像帧保存失败！");
        }
    }
}

void VideoTcpMainWindow::on_btnStartBuil3D_clicked()
{

}

