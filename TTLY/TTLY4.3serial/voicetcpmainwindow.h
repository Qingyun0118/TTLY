#ifndef VOICETCPMAINWINDOW_H
#define VOICETCPMAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QAudioOutput>
#include <QByteArray>
#include <QPainter>
#include <QVector>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QQueue>
namespace Ui {
class VoiceTcpMainWindow;
}

class VoiceTcpMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit VoiceTcpMainWindow(QWidget *parent = nullptr);
    ~VoiceTcpMainWindow();




private slots:
    void on_btnConnectPi_clicked();

    void on_btnStartGetVoice_clicked();

    //获取识别结果
    void on_btnSatrtGetResult_clicked();

    void handleError(QAbstractSocket::SocketError error);   // 错误处理

    void on_btnClearResult_clicked();


    void on_btnSaveResult_clicked();

private:
    Ui::VoiceTcpMainWindow *ui;
    QTcpSocket *tcpSocket;            // TCP客户端套接字

    // 添加私有成员
    QAudioFormat audioFormat;
    QByteArray audioBuffer;
    bool isRecording;
    QTimer *waveformTimer;
    QVector<qint16> samplesBuffer;

    // 新增音频播放成员
    QAudioOutput *audioOutput;
    QIODevice *audioOutputDevice;
    bool isPlaying;  // 播放状态标志

    QQueue<QByteArray> audioQueue;
    QTimer *audioWriteTimer;
    QMutex audioMutex;

    // 新增私有函数
    void startAudioPlayback();
    void stopAudioPlayback();

    // 私有函数
    void setupAudioFormat();
    void saveToWav(const QString &filename);
    void paintEvent(QPaintEvent *event) override;

    void readAudioData();

    void writeAudioData();


};

#endif // VOICETCPMAINWINDOW_H
