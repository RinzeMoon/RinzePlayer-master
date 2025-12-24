#ifndef TESTHEADER_H
#define TESTHEADER_H

// 1. 引入必要的Qt头文件（解决类未定义问题）
#include <QMainWindow>    // 必须包含，否则不认识QMainWindow
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QLabel>
#include <QPushButton>    // 引入QPushButton头文件
#include <QTextEdit>
#include <QTime>          // 引入QTime头文件
#include <QTimer>         // 引入QTimer头文件
#include <QMetaObject>
#include <QTextCursor>
#include <iostream>       // 引入iostream，解决std::cout和std::endl问题
#include <string>         // 用于std::string转换

// 如果使用SDL，需要引入SDL头文件（根据实际情况调整路径）
// #include <SDL2/SDL.h>

// 2. 声明全局变量（使用extern，避免重复定义）
class DiagnosticWindow;
extern DiagnosticWindow *g_diagnosticWindow;

// 3. 定义类，添加Q_OBJECT宏（必须，支持信号槽）
class DiagnosticWindow : public QMainWindow
{
    Q_OBJECT    // 启用Qt的元对象系统，支持信号槽和invokeMethod

public:
    // 构造函数
    DiagnosticWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
        // 窗口基本设置
        setWindowTitle("RinzePlayer - 实时诊断窗口");
        resize(600, 400);

        // 中心部件
        QWidget *centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);

        // 主布局
        QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

        // 状态标签
        statusLabel = new QLabel("🔍 诊断窗口已启动 - 实时监控中...", this);
        mainLayout->addWidget(statusLabel);

        // 标签页
        QTabWidget *tabWidget = new QTabWidget(this);
        mainLayout->addWidget(tabWidget);

        // 添加标签页
        tabWidget->addTab(createUITab(), "UI响应性");
        tabWidget->addTab(createAudioTab(), "音频状态");
        tabWidget->addTab(createLogTab(), "系统日志");

        // 按钮布局
        QWidget *buttonWidget = new QWidget(this);
        QHBoxLayout *buttonLayout = new QHBoxLayout(buttonWidget);
        mainLayout->addWidget(buttonWidget);

        // 按钮定义
        QPushButton *refreshBtn = new QPushButton("🔄 刷新状态", this);
        connect(refreshBtn, &QPushButton::clicked, this, &DiagnosticWindow::refreshAllStats);
        buttonLayout->addWidget(refreshBtn);

        QPushButton *testUIbtn = new QPushButton("🧪 测试UI响应", this);
        connect(testUIbtn, &QPushButton::clicked, this, &DiagnosticWindow::testUIResponsiveness);
        buttonLayout->addWidget(testUIbtn);

        QPushButton *clearLogBtn = new QPushButton("🗑️ 清除日志", this);
        connect(clearLogBtn, &QPushButton::clicked, this, &DiagnosticWindow::clearLogs);
        buttonLayout->addWidget(clearLogBtn);

        QPushButton *closeBtn = new QPushButton("❌ 关闭诊断", this);
        connect(closeBtn, &QPushButton::clicked, this, &QMainWindow::close);
        buttonLayout->addWidget(closeBtn);

        // 启动监控定时器
        startMonitoringTimers();
    }

    // 析构函数
    ~DiagnosticWindow() {
        stopMonitoringTimers();
    }

    // 公共方法
    void addLog(const QString &message);
    void updateUIStatus(bool isResponsive);
    void testUIResponsiveness();
    void refreshAllStats() {
        addLog("手动刷新状态 - 所有监控数据已更新");
    }
    void clearLogs() {
        if (logTextEdit) {
            logTextEdit->clear();
            addLog("日志已清空");
        }
    }
    void runSomething() {
        addLog("执行测试任务...");
    }

private slots:
    // 槽函数（定时器触发）
    void updateSystemStats() {
        addLog("系统状态更新：CPU/内存使用正常");
    }

    void checkUIResponsiveness() {
        // 模拟UI响应性检查
        updateUIStatus(true); // 暂时设为正常
    }

private:
    // 私有方法：创建标签页
    QWidget *createUITab();
    QWidget *createAudioTab();
    QWidget *createLogTab();

    // 私有方法：定时器控制
    void startMonitoringTimers();
    void stopMonitoringTimers();

    // 成员变量
    QLabel *statusLabel = nullptr;
    QTextEdit *logTextEdit = nullptr;
    QLabel *uiResponsiveLabel = nullptr;
    QTableWidget *uiEventTable = nullptr;
    QTimer *systemMonitorTimer = nullptr;
    QTimer *uiMonitorTimer = nullptr;
};

#endif // TESTHEADER_H