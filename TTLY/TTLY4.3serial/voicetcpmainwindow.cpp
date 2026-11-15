#include "voicetcpmainwindow.h"
#include "../common/runtimepaths.h"
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QVariant>
#include "qdatetime.h"
#include "qdir.h"
#include "qprocess.h"
#include "ui_voicetcpmainwindow.h"
#include <QMessageBox>
#include <QTimer>
#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QFileDialog>
#include <QProgressDialog>

VoiceTcpMainWindow::VoiceTcpMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::VoiceTcpMainWindow)
    , tcpSocket(new QTcpSocket(this))
{
    ui->setupUi(this);
    // 初始化UI默认值
    ui->lineEdit_IP->setText("10.0.0.1");  // 默认IP提示
    ui->lineEdit_port->setText("8002");                  // 默认端口
    ui->lineEdit_SavePath_voice->setText(TTLYRuntime::path("Result/voice"));//录音文件存储路径

    // 新增信号连接
    connect(tcpSocket, &QTcpSocket::connected, this, [this](){
        qDebug() << "Connected!";
        ui->btnConnectPi->setText("断开连接");
        ui->btnConnectPi->setEnabled(true);
        ui->label_StatueConnect->setText("连接成功");
        // frameTimer->start();
        ui->lineEdit_IP->setEnabled(false);
        ui->lineEdit_port->setEnabled(false);
    });
    connect(tcpSocket, &QTcpSocket::errorOccurred, this, &VoiceTcpMainWindow::handleError);
    connect(tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &VoiceTcpMainWindow::handleError);

    setupAudioFormat();
    isRecording = false;

    waveformTimer = new QTimer(this);
    connect(waveformTimer, &QTimer::timeout, this, [this](){ update(); });
    waveformTimer->start(100); // 每100ms刷新波形

    connect(tcpSocket, &QTcpSocket::readyRead, this, &VoiceTcpMainWindow::readAudioData);

    // 初始化音频播放
    audioOutput = nullptr;
    audioOutputDevice = nullptr;
    isPlaying = false;

    // 连接成功后自动开始播放
    connect(tcpSocket, &QTcpSocket::connected, this, [this](){
        qDebug() << "Connected!";
        startAudioPlayback();  // 新增：启动播放
        // ...其他连接成功逻辑...
    });

    audioWriteTimer = new QTimer(this);
    connect(audioWriteTimer, &QTimer::timeout, this, &VoiceTcpMainWindow::writeAudioData);
    audioWriteTimer->start(10); // 每10ms尝试写入数据

}

VoiceTcpMainWindow::~VoiceTcpMainWindow()
{

    // 释放音频资源
    if (audioOutput) {
        audioOutput->stop();
        delete audioOutput;
    }

    if (audioWriteTimer) {
        audioWriteTimer->stop();
        delete audioWriteTimer;
    }
    delete ui;
}

void VoiceTcpMainWindow::on_btnConnectPi_clicked()
{
    // 断开连接处理
    if(tcpSocket->state() == QAbstractSocket::ConnectedState){
        tcpSocket->disconnectFromHost();
        ui->btnConnectPi->setText("连接香橙派");
        ui->label_StatueConnect->setText("连接已断开");
        // frameTimer->stop();
        ui->lineEdit_IP->setEnabled(true);
        ui->lineEdit_port->setEnabled(true);
        return;
    }

    QString ip = ui->lineEdit_IP->text();
    QString portStr = ui->lineEdit_port->text();

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
    ui->label_StatueConnect->setText("正在连接...");
    qApp->processEvents();

    tcpSocket->connectToHost(ip, port);
    ui->lineEdit_port->clear();
    // 设置5秒超时
    QTimer *timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer, &QTimer::timeout, this, [this](){
        if(tcpSocket->state() == QAbstractSocket::ConnectingState){
            tcpSocket->abort();
            ui->label_StatueConnect->setText("错误: 连接超时");
            ui->btnConnectPi->setEnabled(true);
        }
        sender()->deleteLater(); // 删除定时器
    });
    timeoutTimer->start(5000);
}

void VoiceTcpMainWindow::setupAudioFormat() {
    audioFormat.setSampleRate(44100);
    audioFormat.setChannelCount(1);
    audioFormat.setSampleSize(16);
    audioFormat.setCodec("audio/pcm");
    audioFormat.setByteOrder(QAudioFormat::LittleEndian);
    audioFormat.setSampleType(QAudioFormat::SignedInt);
}



// 启动音频播放
void VoiceTcpMainWindow::startAudioPlayback()
{
    // if (!audioOutput) {
    //     // 确保音频格式被设备支持
    //     QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
    //     if (!info.isFormatSupported(audioFormat)) {
    //         qWarning() << "默认设备不支持该音频格式，使用最近似格式";
    //         audioFormat = info.nearestFormat(audioFormat);
    //     }

    //     audioOutput = new QAudioOutput(audioFormat, this);
    //     audioOutputDevice = audioOutput->start();
    //     isPlaying = true;
    //     qDebug() << "音频播放已启动";
    // }

    if (!audioOutput) {
        QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
        if (!info.isFormatSupported(audioFormat)) {
            audioFormat = info.nearestFormat(audioFormat);
        }

        audioOutput = new QAudioOutput(audioFormat, this);
        audioOutput->setBufferSize(8192); // 增大缓冲区大小
        audioOutputDevice = audioOutput->start();
        isPlaying = true;
        audioQueue.clear(); // 清空旧数据
    }
}

// 停止音频播放
void VoiceTcpMainWindow::stopAudioPlayback()
{
    // if (audioOutput) {
    //     audioOutput->stop();
    //     delete audioOutput;
    //     audioOutput = nullptr;
    //     audioOutputDevice = nullptr;
    //     isPlaying = false;
    //     qDebug() << "音频播放已停止";
    // }
    if (audioOutput) {
        audioWriteTimer->stop();
        QMutexLocker locker(&audioMutex);
        audioQueue.clear();
        audioOutput->stop();
        delete audioOutput;
        audioOutput = nullptr;
        audioOutputDevice = nullptr;
        isPlaying = false;
    }
}

void VoiceTcpMainWindow::readAudioData() {
    QByteArray data = tcpSocket->readAll();
    // 在TCP连接成功后，设置接收缓冲区大小
    tcpSocket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 1024 * 1024);  // 1MB
    // 录音逻辑
    if (isRecording) {
        audioBuffer.append(data);
    }

    // 将数据加入队列（加锁保护）
    QMutexLocker locker(&audioMutex);
    audioQueue.enqueue(data);

    // 实时播放逻辑
    // if (isPlaying && audioOutputDevice) {
    //     // 写入音频设备实现播放
    //     qint64 bytesWritten = audioOutputDevice->write(data);
    //     if (bytesWritten != data.size()) {
    //         qWarning() << "音频数据未完全写入，可能丢失"
    //                    << data.size() - bytesWritten << "字节";
    //     }
    // }

    // 更新波形缓冲区（保留最近1024个样本）
    const qint16* samples = reinterpret_cast<const qint16*>(data.constData());
    int sampleCount = data.size() / sizeof(qint16);
    samplesBuffer.resize(samplesBuffer.size() + sampleCount);
    std::copy(samples, samples + sampleCount, samplesBuffer.end() - sampleCount);
    if (samplesBuffer.size() > 2048) {
        samplesBuffer = samplesBuffer.mid(samplesBuffer.size() - 1024);
    }
}


// 新增写入数据函数
void VoiceTcpMainWindow::writeAudioData() {
    // if (!audioOutputDevice || audioQueue.isEmpty()) return;

    // QMutexLocker locker(&audioMutex);
    // int bytesAvailable = audioOutput->bytesFree();

    // while (bytesAvailable > 0 && !audioQueue.isEmpty()) {
    //     QByteArray &chunk = audioQueue.head();
    //     int chunkSize = chunk.size();

    //     // 计算本次可写入的数据量
    //     int writeSize = qMin(bytesAvailable, chunkSize);
    //     qint64 bytesWritten = audioOutputDevice->write(chunk.constData(), writeSize);

    //     if (bytesWritten > 0) {
    //         // 移除已写入的数据
    //         if (bytesWritten == chunkSize) {
    //             audioQueue.dequeue();
    //         } else {
    //             chunk = chunk.mid(bytesWritten);
    //         }
    //         bytesAvailable -= bytesWritten;
    //     } else {
    //         break; // 写入失败，下次重试
    //     }
    // }
    if (!audioOutputDevice || audioQueue.isEmpty()) return;

    QMutexLocker locker(&audioMutex);
    int bytesAvailable = audioOutput->bytesFree();

    while (bytesAvailable > 0 && !audioQueue.isEmpty()) {
        QByteArray &chunk = audioQueue.head();
        int chunkSize = chunk.size();
        int writeSize = qMin(bytesAvailable, chunkSize);
        qint64 bytesWritten = audioOutputDevice->write(chunk.constData(), writeSize);

        if (bytesWritten > 0) {
            if (bytesWritten == chunkSize) {
                audioQueue.dequeue();
            } else {
                chunk = chunk.mid(bytesWritten);
            }
            bytesAvailable -= bytesWritten;
        } else {
            break;
        }
    }
}

//波形绘制
void VoiceTcpMainWindow::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    QRect waveformRect = ui->label_VoicePlayer->geometry();
    painter.setViewport(waveformRect);
    painter.setWindow(0, 0, waveformRect.width(), waveformRect.height());

    painter.fillRect(0, 0, waveformRect.width(), waveformRect.height(), Qt::black);

    if (samplesBuffer.isEmpty()) return;

    painter.setPen(QPen(Qt::green, 1));
    int step = qMax(1, samplesBuffer.size() / waveformRect.width());
    for (int x = 0; x < waveformRect.width(); ++x) {
        int index = x * step;
        if (index >= samplesBuffer.size()) break;
        qint16 sample = samplesBuffer[index];
        int y = (sample + 32768) * waveformRect.height() / 65536;
        painter.drawLine(x, waveformRect.height()/2, x, y);
    }
}



// 实现开始录音按钮
void VoiceTcpMainWindow::on_btnStartGetVoice_clicked()
{
    if (!isRecording) {
        // 开始录音
        audioBuffer.clear();
        isRecording = true;
        ui->btnStartGetVoice->setText("停止录音");
    } else {
        // 停止录音并保存
        isRecording = false;
        ui->btnStartGetVoice->setText("开始录音");

        QDir dir(ui->lineEdit_SavePath_voice->text());

        // 确保目录存在
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                QMessageBox::warning(this, "错误", "无法创建目录: " + ui->lineEdit_SavePath_voice->text());
                return;
            }
        }

        // 生成当前时间作为文件名（格式示例：20231010_153045）
        QString fileName = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".wav";
        QString savePath = dir.filePath(fileName);

        // 保存文件
        saveToWav(savePath);
        ui->label_StatueRecordresult->setText("录音保存为"+fileName);
    }
}

void VoiceTcpMainWindow::saveToWav(const QString &filename) {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "错误", "无法创建文件");
        return;
    }

    // WAV文件头
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    // RIFF头
    out.writeRawData("RIFF", 4);
    out << quint32(audioBuffer.size() + 36); // 后续数据大小
    out.writeRawData("WAVE", 4);

    // fmt子块
    out.writeRawData("fmt ", 4);
    out << quint32(16);          // 块大小
    out << quint16(1);           // PCM格式
    out << quint16(1);           // 声道数
    out << quint32(44100);       // 采样率
    out << quint32(44100 * 2);   // 字节率
    out << quint16(2);           // 块对齐
    out << quint16(16);          // 位深

    // data子块
    out.writeRawData("data", 4);
    out << quint32(audioBuffer.size());
    file.write(audioBuffer);
    file.close();
}


void VoiceTcpMainWindow::on_btnSatrtGetResult_clicked()
{
    const QString scriptPath = TTLYRuntime::path("Sound_Rec/soundimprove.py");
    if (!QFileInfo(scriptPath).isFile()) {
        QMessageBox::warning(this, "识别错误", "找不到声音识别脚本，请检查 TTLY_ROOT：\n" + scriptPath);
        return;
    }
    const QString pythonCmd = TTLYRuntime::pythonExecutable();
    if (pythonCmd.isEmpty()) {
        QMessageBox::warning(this, "识别错误", "找不到 Python，请创建项目 .venv 或设置 TTLY_PYTHON。");
        return;
    }

    QStringList selectedFiles = QFileDialog::getOpenFileNames(
        this, "选择录音文件", ui->lineEdit_SavePath_voice->text(), "音频文件 (*.wav)");
    if (selectedFiles.isEmpty()) {
        return;
    }

    QProgressDialog *progressDialog = new QProgressDialog(this);
    progressDialog->setWindowTitle("请稍候");
    progressDialog->setLabelText("正在批量识别...");
    progressDialog->setCancelButton(nullptr);
    progressDialog->setRange(0, 0);
    progressDialog->setFixedSize(400, 150);
    progressDialog->show();

    QProcess *process = new QProcess(this);
    process->setWorkingDirectory(TTLYRuntime::projectRoot());
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert("PYTHONIOENCODING", "utf-8");
    environment.insert("PYTHONUTF8", "1");
    environment.insert("PYTHONUNBUFFERED", "1");
    process->setProcessEnvironment(environment);

    QStringList args = { scriptPath };
    for (const QString &file : selectedFiles) {
        args << "--audio" << QDir::toNativeSeparators(file);
    }

    // A read can split a UTF-8 character or a result line; decode complete lines.
    const auto appendOutput = [this, process](bool flush) {
        QByteArray pending = process->property("stdoutBuffer").toByteArray();
        pending += process->readAllStandardOutput();
        int newline;
        while ((newline = pending.indexOf('\n')) >= 0 || (flush && !pending.isEmpty())) {
            const int length = newline >= 0 ? newline : pending.size();
            const QString line = QString::fromUtf8(pending.left(length)).trimmed();
            pending.remove(0, length + (newline >= 0 ? 1 : 0));
            if (line.contains("-")) {
                ui->textEdit_Result->append(line);
                ui->textEdit_Result->setStyleSheet("color:red");
            }
        }
        process->setProperty("stdoutBuffer", pending);
    };
    connect(process, &QProcess::readyReadStandardOutput, this, [appendOutput]() {
        appendOutput(false);
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [=](int code, QProcess::ExitStatus status) {
                appendOutput(true);
                progressDialog->close();
                progressDialog->deleteLater();
                if (code != 0 || status != QProcess::NormalExit) {
                    QString error = QString::fromUtf8(process->readAllStandardError()).trimmed();
                    if (error.isEmpty()) {
                        error = QString("识别进程异常退出（代码 %1）").arg(code);
                    }
                    QMessageBox::warning(this, "识别错误", error);
                }
                process->deleteLater();
            });
    connect(process, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            progressDialog->close();
            progressDialog->deleteLater();
            QMessageBox::warning(this, "识别错误",
                                 "无法启动 Python：" + pythonCmd + "\n" + process->errorString());
            process->deleteLater();
        }
    });
    process->start(pythonCmd, args);
}


void VoiceTcpMainWindow::handleError(QAbstractSocket::SocketError error)
{
    QString errorDetail;
    switch(error){
    case QAbstractSocket::ConnectionRefusedError:
        errorDetail = "连接被拒绝（服务未运行）";
        tcpSocket->disconnectFromHost();
        ui->btnConnectPi->setText("连接香橙派");
        ui->lineEdit_IP->setEnabled(true);
        ui->lineEdit_port->setEnabled(true);
        break;
    case QAbstractSocket::HostNotFoundError:
        errorDetail = "主机不存在";
        tcpSocket->disconnectFromHost();
        ui->btnConnectPi->setText("连接香橙派");
        ui->lineEdit_IP->setEnabled(true);
        ui->lineEdit_port->setEnabled(true);
        break;
    case QAbstractSocket::RemoteHostClosedError:
        errorDetail = "远程主机已关闭连接";
        tcpSocket->disconnectFromHost();
        ui->btnConnectPi->setText("连接香橙派");
        ui->lineEdit_IP->setEnabled(true);
        ui->lineEdit_port->setEnabled(true);
        break;
    case QAbstractSocket::NetworkError:
        errorDetail = "网络错误，请检查连接";
        tcpSocket->disconnectFromHost();
        ui->btnConnectPi->setText("连接香橙派");
        ui->lineEdit_IP->setEnabled(true);
        ui->lineEdit_port->setEnabled(true);
        break;
    default:
        errorDetail = tcpSocket->errorString();
    }

    ui->label_StatueConnect->setText("错误: " + errorDetail);
    tcpSocket->abort();
    ui->btnConnectPi->setEnabled(true);
}

void VoiceTcpMainWindow::on_btnClearResult_clicked()
{
    ui->textEdit_Result->clear();
}


void VoiceTcpMainWindow::on_btnSaveResult_clicked()
{
    // 获取文本框内容
    QString content = ui->textEdit_Result->toPlainText();
    // if (content.isEmpty()) {
    //     QMessageBox::warning(this, "提示", "没有可保存的内容");
    //     return;
    // }

    // 获取录音文件保存路径
    // QString saveDir = ui->lineEdit_SavePath_voice->text();
    QString saveDir = TTLYRuntime::path("Result/voice");
    QDir dir(saveDir);

    // 检查目录是否存在，若不存在则创建
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            QMessageBox::warning(this, "错误", "无法创建目录: " + saveDir);
            return;
        }
    }

    // 生成带时间戳的文件名
    QString fileName = "Result.txt";
    QString filePath = dir.filePath(fileName);

    // 写入文件
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        QMessageBox::warning(this, "错误", "无法创建文件: " + filePath);
        return;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");  // 设置编码
    out << content;
    file.close();

    // 清除内容并提示
    ui->textEdit_Result->clear();
    ui->label_StatueRecordresult->setText("保存成功,文件已保存为Result.txt");
    // QMessageBox::information(this, "保存成功", "文件已保存至: " + filePath);
}

