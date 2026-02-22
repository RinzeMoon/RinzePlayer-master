#include <QApplication>
#include <QMainWindow>
#include <QTimer>
#include <QObject>
#include "../Header/Ui/mainwindow.h"
#include "../Header/VideoPart/AvSourceParse/AVSourceResolver.h"
#include <signal.h>
#include "../Header/Ui/VideoPlayerMW.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

   // signal(SIGTERM, sa_handler);

    MainWindow mainWindow;
    mainWindow.resize(800, 600);
    mainWindow.show();

    VideoPlayerMW * mw = new VideoPlayerMW();
    mw->show();

    return app.exec();
}
