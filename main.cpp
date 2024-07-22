#include <QApplication>
#include "QtSingleApplication.h"
#include "widget.h"
#include "screen.h"
#include <QTextCodec>
#include <QLockFile>
int main(int argc, char* argv[])
{
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF8"));
    QtSingleApplication app("CloudMeeting_AppInstance", argc, argv);
    Screen::init();

    // 检查是否已经有实例在运行
    if (app.isRunning()) {
        WRITE_LOG("the application is running...");
        app.sendMessage("raise_window_noop", 1000); //1s后激活前个实例
        return EXIT_SUCCESS;
    }
    // // 应用实例互斥
    // QString tempDir = QApplication::applicationDirPath();
    // QLockFile lockFile(tempDir + "/CloudMeeting.lock");
    // if (!lockFile.tryLock()) {
    //     qDebug() << "application is running...";
    //     return 1;
    // }

    Widget w;
    app.setActivationWindow(&w);
    w.show();
    return app.exec();
}
