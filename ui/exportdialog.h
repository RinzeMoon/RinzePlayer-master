// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QLineEdit>
#include <QFileDialog>
#include <QGroupBox>
#include <QMessageBox>

class ExportDialog : public QDialog {
    Q_OBJECT
public:
    explicit ExportDialog(QWidget *parent = nullptr, const QString &defaultPath = QString());

    // 获取设置
    QString outputPath() const;
    bool includeAudio() const;
    bool exportMotionVectors() const;
    bool exportHeatmap() const;
    bool exportTrail() const;
    bool exportHistory() const;

    // 更新进度
    void setProgress(int progress, const QString &text = QString());

signals:
    void startExport();

private slots:
    void browseOutputPath();
    void onStartClicked();

private:
    QLineEdit *m_outputPathEdit;
    QCheckBox *m_includeAudioCheck;
    QCheckBox *m_exportVectorsCheck;
    QCheckBox *m_exportHeatmapCheck;
    QCheckBox *m_exportTrailCheck;
    QCheckBox *m_exportHistoryCheck;
    QPushButton *m_browseBtn;
    QPushButton *m_startBtn;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
};

#endif // EXPORTDIALOG_H
