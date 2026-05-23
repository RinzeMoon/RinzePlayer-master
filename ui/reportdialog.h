// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef REPORTDIALOG_H
#define REPORTDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QLineEdit>
#include <QFileDialog>
#include <QGroupBox>
#include <QMessageBox>

class ReportDialog : public QDialog {
    Q_OBJECT
public:
    explicit ReportDialog(QWidget *parent = nullptr, const QString &defaultPath = QString());

    QString outputPath() const;
    bool includeBasicInfo() const;
    bool includeMotionStats() const;
    bool includeDirectionRose() const;
    bool includeQPStats() const;
    bool includeEncodingModeStats() const;
    bool includeSpatialHeatmap() const;
    bool includeKeyFrames() const;

    void setProgress(int progress, const QString &text = QString());
    void setExporting(bool exporting);

signals:
    void startExport(const QString &videoPath, const QString &outputPath);

private slots:
    void browseOutputPath();
    void browseVideoPath();
    void onStartClicked();

private:
    QLineEdit *m_videoPathEdit;
    QLineEdit *m_outputPathEdit;
    QCheckBox *m_includeBasicInfoCheck;
    QCheckBox *m_includeMotionStatsCheck;
    QCheckBox *m_includeDirectionRoseCheck;
    QCheckBox *m_includeQPStatsCheck;
    QCheckBox *m_includeEncodingModeStatsCheck;
    QCheckBox *m_includeSpatialHeatmapCheck;
    QCheckBox *m_includeKeyFramesCheck;
    QPushButton *m_browseVideoBtn;
    QPushButton *m_browseOutputBtn;
    QPushButton *m_startBtn;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
};

#endif // REPORTDIALOG_H
