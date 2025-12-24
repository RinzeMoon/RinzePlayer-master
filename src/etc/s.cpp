#include "TestHeader.h"
#include <QThread>

// 定义全局变量（仅在源文件中定义一次）
DiagnosticWindow *g_diagnosticWindow = nullptr;

// 实现DiagnosticWindow的私有方法
QWidget *DiagnosticWindow::createUITab() {
    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);

    uiResponsiveLabel = new QLabel("UI响应性: 未测试", this);
    layout->addWidget(uiResponsiveLabel);

    QPushButton *testBtn = new QPushButton("立即测试");
    connect(testBtn, &QPushButton::clicked, this, &DiagnosticWindow::testUIResponsiveness);
    layout->addWidget(testBtn);

    // 修复QTableWidget构造函数：先创建，再设置行列（避免参数不匹配）
    uiEventTable = new QTableWidget(this);
    uiEventTable->setRowCount(0);
    uiEventTable->setColumnCount(3);
    uiEventTable->setHorizontalHeaderLabels({"时间", "状态", "测试次数"});
    uiEventTable->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(uiEventTable);

    return widget;
}

QWidget *DiagnosticWindow::createAudioTab() {
    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);

    QLabel *sdlStatusLabel = new QLabel("SDL状态: 未初始化", this);
    layout->addWidget(sdlStatusLabel);

    QPushButton *testSdlBtn = new QPushButton("测试SDL初始化", this);
    connect(testSdlBtn, &QPushButton::clicked, [this, sdlStatusLabel]() {
        // 注释SDL相关代码（如果未安装SDL，可先注释）
        /*
        if (SDL_Init(SDL_INIT_AUDIO) < 0) {
            QString error = QString("SDL初始化失败: %1").arg(SDL_GetError());
            sdlStatusLabel->setText(error);
            addLog(error);
        } else {
            int numDevices = SDL_GetNumAudioDevices(0);
            sdlStatusLabel->setText(QString("SDL初始化成功，音频设备数: %1").arg(numDevices));
            addLog("SDL音频模块初始化成功");
            SDL_Quit();
        }
        */
        sdlStatusLabel->setText("SDL初始化成功（模拟）");
        addLog("SDL音频模块初始化成功（模拟）");
    });
    layout->addWidget(testSdlBtn);

    QPushButton *ffmpegInfoBtn = new QPushButton("显示FFmpeg信息");
    connect(ffmpegInfoBtn, &QPushButton::clicked, [this]() {
        addLog("FFmpeg版本: 6.0 (模拟)");
    });
    layout->addWidget(ffmpegInfoBtn);

    return widget;
}

QWidget *DiagnosticWindow::createLogTab() {
    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);

    logTextEdit = new QTextEdit(this);
    logTextEdit->setReadOnly(true); // 日志只读
    layout->addWidget(logTextEdit);

    return widget;
}

// 实现定时器方法
void DiagnosticWindow::startMonitoringTimers() {
    systemMonitorTimer = new QTimer(this);
    connect(systemMonitorTimer, &QTimer::timeout, this, &DiagnosticWindow::updateSystemStats);
    systemMonitorTimer->start(2000); // 2秒一次

    uiMonitorTimer = new QTimer(this);
    connect(uiMonitorTimer, &QTimer::timeout, this, &DiagnosticWindow::checkUIResponsiveness);
    uiMonitorTimer->start(5000); // 5秒一次
}

void DiagnosticWindow::stopMonitoringTimers() {
    if (systemMonitorTimer) {
        systemMonitorTimer->stop();
        delete systemMonitorTimer;
        systemMonitorTimer = nullptr;
    }
    if (uiMonitorTimer) {
        uiMonitorTimer->stop();
        delete uiMonitorTimer;
        uiMonitorTimer = nullptr;
    }
}

// 实现添加日志方法（修复QMetaObject::invokeMethod调用）
void DiagnosticWindow::addLog(const QString &message) {
    // 修复：添加Qt::QueuedConnection参数，确保在主线程执行
    QMetaObject::invokeMethod(this, [this, message]() {
        QString timestamp = QTime::currentTime().toString("hh:mm:ss.zzz");
        QString logEntry = QString("[%1] %2").arg(timestamp).arg(message);

        if (logTextEdit) {
            logTextEdit->append(logEntry);

            // 保持日志行数不超过500
            if (logTextEdit->document()->lineCount() > 500) {
                QTextCursor cursor = logTextEdit->textCursor();
                cursor.movePosition(QTextCursor::Start);
                cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 100);
                cursor.removeSelectedText();
            }
        }

        // 输出到控制台
        std::cout << logEntry.toStdString() << std::endl;
    }, Qt::QueuedConnection); // 关键：添加连接类型参数
}

// 实现更新UI状态方法（修复QMetaObject::invokeMethod调用）
void DiagnosticWindow::updateUIStatus(bool isResponsive) {
    QMetaObject::invokeMethod(this, [this, isResponsive]() {
        static int uiTestCount = 0;
        uiTestCount++;

        QString status = isResponsive ? "✅ 正常" : "❌ 无响应";
        uiResponsiveLabel->setText("UI响应性: " + status);
        uiResponsiveLabel->setStyleSheet(isResponsive ?
            "color: #27AE60;" : "color: #E74C3C; font-weight: bold;");

        // 更新表格
        int row = uiEventTable->rowCount();
        uiEventTable->insertRow(row);

        QTableWidgetItem *timeItem = new QTableWidgetItem(QTime::currentTime().toString("hh:mm:ss"));
        QTableWidgetItem *statusItem = new QTableWidgetItem(status);
        QTableWidgetItem *countItem = new QTableWidgetItem(QString::number(uiTestCount));

        uiEventTable->setItem(row, 0, timeItem);
        uiEventTable->setItem(row, 1, statusItem);
        uiEventTable->setItem(row, 2, countItem);

        // 滚动到最后
        uiEventTable->scrollToBottom();
    }, Qt::QueuedConnection); // 关键：添加连接类型参数
}

// 实现测试UI响应性方法
void DiagnosticWindow::testUIResponsiveness() {
    QTime startTime = QTime::currentTime();
    // 模拟耗时操作（暂停100ms）
    QThread::msleep(100); // 需要包含<QThread>头文件
    int elapsed = startTime.msecsTo(QTime::currentTime());

    bool responsive = elapsed < 200; // 小于200ms视为响应正常
    updateUIStatus(responsive);

    addLog(QString("UI响应性测试完成，耗时: %1ms，状态: %2").arg(elapsed).arg(responsive ? "正常" : "缓慢"));
}