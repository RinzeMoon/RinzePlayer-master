// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DANMAKUOVERLAY_H
#define DANMAKUOVERLAY_H

#include <QWidget>
#include <QTimer>
#include <QList>

struct DanmakuItem {
    QString sender;   // "host" / "guest" / "system"
    QString text;
    int track;        // row track index
    qint64 startMs;   // wall time when appeared
    bool fadingOut;
};

class DanmakuOverlay : public QWidget {
    Q_OBJECT
public:
    explicit DanmakuOverlay(QWidget *parent = nullptr);
    ~DanmakuOverlay();

    void pushMessage(const QString &sender, const QString &text);
    void pushSystem(const QString &text);
    void setEnabled(bool enabled) { m_enabled = enabled; update(); }
    bool isEnabled() const { return m_enabled; }
    void clearAll();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void tick();

private:
    int allocateTrack() const;

    QTimer *m_tickTimer = nullptr;
    QList<DanmakuItem> m_items;

    int m_trackCount = 8;
    int m_trackHeight = 36;
    int m_visibleMs = 4000;   // stay visible for 4s
    int m_fadeMs = 1500;      // fade out over 1.5s
    bool m_enabled = true;

    QColor m_hostColor{0xf0, 0xa0, 0x40};   // orange
    QColor m_guestColor{0x60, 0xa0, 0xf0};  // blue
    QColor m_systemColor{0xff, 0xff, 0xff}; // white
};

#endif // DANMAKUOVERLAY_H
