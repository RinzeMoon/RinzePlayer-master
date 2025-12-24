#include <QApplication>
#include <QMainWindow>
#include <QTimer>
#include <QObject>
#include "../src/etc/TestHeader.h"
#include "../Header/Ui/mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    MainWindow mainWindow;
    mainWindow.resize(800, 600);
    mainWindow.show();

    // 初始化全局诊断窗口
    /*
    g_diagnosticWindow = new DiagnosticWindow();
    g_diagnosticWindow->show();

    // 修复lambda捕获：添加&捕获全局变量，或直接捕获g_diagnosticWindow
    QTimer::singleShot(1000, [&mainWindow]() {
        if (g_diagnosticWindow) {
            g_diagnosticWindow->addLog("延迟检查: 程序启动完成");
            g_diagnosticWindow->addLog("请操作主窗口，观察诊断窗口的响应性测试");
            g_diagnosticWindow->addLog(QString("主窗口状态 - 可见: %1, 激活: %2")
                .arg(mainWindow.isVisible() ? "是" : "否")
                .arg(mainWindow.isActiveWindow() ? "是" : "否"));
            g_diagnosticWindow->runSomething();
        }
    });

    // 修复lambda捕获：添加&
    QObject::connect(&app, &QApplication::aboutToQuit, []() {
        if (g_diagnosticWindow) {
            g_diagnosticWindow->addLog("应用程序即将退出");
            g_diagnosticWindow->addLog("==========================================");
            delete g_diagnosticWindow;
            g_diagnosticWindow = nullptr;
        }
    });
*/
    return app.exec();
}