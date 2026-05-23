// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("RinzePlayer");
    QApplication::setApplicationVersion("0.1");

    MainWindow window;
    window.resize(1280, 720);
    window.show();

    return app.exec();
}
