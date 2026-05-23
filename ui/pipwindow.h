// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PIPWINDOW_H
#define PIPWINDOW_H

#include <QWidget>
#include "ui/videowidget.h"

class PiPWindow : public QWidget {
    Q_OBJECT
public:
    explicit PiPWindow(QWidget *parent = nullptr);
    ~PiPWindow();

    void setVideoData(VideoDoubleBuf *vidData, SubtitleDoubleBuf *subData);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    VideoWidget *m_videoWidget;
    bool m_isDragging;
    QPoint m_dragStartPos;
    QPoint m_windowStartPos;
};

#endif // PIPWINDOW_H
