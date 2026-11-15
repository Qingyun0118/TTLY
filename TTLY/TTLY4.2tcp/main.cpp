#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    const int screenshotIndex = arguments.indexOf("--screenshot");

    QString screenshotPath;
    if (screenshotIndex >= 0) {
        if (screenshotIndex + 1 >= arguments.size() || arguments.at(screenshotIndex + 1).isEmpty()) {
            qCritical() << "Usage: --screenshot <output.png>";
            return 2;
        }
        screenshotPath = arguments.at(screenshotIndex + 1);
    }

    MainWindow window;
    window.show();

    // CI can render the actual Qt widget without a desktop or robot hardware.
    if (!screenshotPath.isEmpty()) {
        QTimer::singleShot(500, &app, [&app, &window, screenshotPath]() {
            const QFileInfo output(screenshotPath);
            if (!QDir().mkpath(output.absolutePath())) {
                qCritical() << "Cannot create screenshot directory:" << output.absolutePath();
                app.exit(3);
                return;
            }

            const QPixmap image = window.grab();
            if (image.isNull() || !image.save(screenshotPath, "PNG")) {
                qCritical() << "Failed to save screenshot:" << screenshotPath;
                app.exit(4);
                return;
            }

            qInfo() << "Saved UI screenshot:" << screenshotPath;
            app.quit();
        });
    }

    return app.exec();
}
