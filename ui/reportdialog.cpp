// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "reportdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>

ReportDialog::ReportDialog(QWidget *parent, const QString &defaultPath)
    : QDialog(parent)
{
    setWindowTitle("生成视频分析报告");
    setMinimumSize(550, 650);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);

    QLabel *infoLabel = new QLabel("📝 从视频生成包含运动向量、编码统计等内容的分析报告", this);
    infoLabel->setStyleSheet("color: #666; padding: 10px; background: #f5f5f5; border-radius: 4px;");
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    QLabel *videoLabel = new QLabel("视频文件:", this);
    QHBoxLayout *videoLayout = new QHBoxLayout();
    m_videoPathEdit = new QLineEdit(this);
    m_videoPathEdit->setPlaceholderText("选择要分析的视频文件...");
    m_browseVideoBtn = new QPushButton("浏览...", this);
    videoLayout->addWidget(m_videoPathEdit);
    videoLayout->addWidget(m_browseVideoBtn);
    mainLayout->addWidget(videoLabel);
    mainLayout->addLayout(videoLayout);

    QLabel *outputLabel = new QLabel("输出报告:", this);
    QHBoxLayout *outputLayout = new QHBoxLayout();
    m_outputPathEdit = new QLineEdit(this);
    m_outputPathEdit->setPlaceholderText("选择输出位置...");
    m_browseOutputBtn = new QPushButton("浏览...", this);
    outputLayout->addWidget(m_outputPathEdit);
    outputLayout->addWidget(m_browseOutputBtn);
    mainLayout->addWidget(outputLabel);
    mainLayout->addLayout(outputLayout);

    QGroupBox *contentGroup = new QGroupBox("报告内容", this);
    QVBoxLayout *contentLayout = new QVBoxLayout(contentGroup);

    m_includeBasicInfoCheck = new QCheckBox("基础信息（文件名、时长、分辨率、帧率）", this);
    m_includeMotionStatsCheck = new QCheckBox("运动强度统计（能量曲线图）", this);
    m_includeDirectionRoseCheck = new QCheckBox("运动方向玫瑰图", this);
    m_includeQPStatsCheck = new QCheckBox("QP统计（编码质量）", this);
    m_includeEncodingModeStatsCheck = new QCheckBox("编码模式统计（I/P/B帧、skip/intra/inter）", this);
    m_includeSpatialHeatmapCheck = new QCheckBox("空间热力图", this);
    m_includeKeyFramesCheck = new QCheckBox("关键帧快照", this);

    m_includeBasicInfoCheck->setChecked(true);
    m_includeMotionStatsCheck->setChecked(true);
    m_includeDirectionRoseCheck->setChecked(true);
    m_includeQPStatsCheck->setChecked(true);
    m_includeEncodingModeStatsCheck->setChecked(true);
    m_includeSpatialHeatmapCheck->setChecked(true);
    m_includeKeyFramesCheck->setChecked(true);

    contentLayout->addWidget(m_includeBasicInfoCheck);
    contentLayout->addWidget(m_includeMotionStatsCheck);
    contentLayout->addWidget(m_includeDirectionRoseCheck);
    contentLayout->addWidget(m_includeQPStatsCheck);
    contentLayout->addWidget(m_includeEncodingModeStatsCheck);
    contentLayout->addWidget(m_includeSpatialHeatmapCheck);
    contentLayout->addWidget(m_includeKeyFramesCheck);
    mainLayout->addWidget(contentGroup);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    mainLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel("准备就绪", this);
    mainLayout->addWidget(m_statusLabel);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_startBtn = new QPushButton("开始生成", this);
    QPushButton *cancelBtn = new QPushButton("取消", this);
    btnLayout->addStretch();
    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);

    if (!defaultPath.isEmpty()) {
        m_videoPathEdit->setText(defaultPath);
        QFileInfo info(defaultPath);
        QString newPath = info.absolutePath() + "/" + info.completeBaseName() + "_analysis_report.pdf";
        m_outputPathEdit->setText(newPath);
    }

    connect(m_browseVideoBtn, &QPushButton::clicked, this, &ReportDialog::browseVideoPath);
    connect(m_browseOutputBtn, &QPushButton::clicked, this, &ReportDialog::browseOutputPath);
    connect(m_startBtn, &QPushButton::clicked, this, &ReportDialog::onStartClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void ReportDialog::browseVideoPath() {
    QString path = QFileDialog::getOpenFileName(this,
        "选择要分析的视频文件", "",
        "视频文件 (*.mp4 *.mkv *.avi *.mov *.flv *.webm);;所有文件 (*)");
    if (!path.isEmpty()) {
        m_videoPathEdit->setText(path);
        if (m_outputPathEdit->text().isEmpty()) {
            QFileInfo info(path);
            QString newPath = info.absolutePath() + "/" + info.completeBaseName() + "_analysis_report.pdf";
            m_outputPathEdit->setText(newPath);
        }
    }
}

void ReportDialog::browseOutputPath() {
    QString path = QFileDialog::getSaveFileName(this,
        "选择报告输出位置", "",
        "PDF文件 (*.pdf);;HTML文件 (*.html);;所有文件 (*)");
    if (!path.isEmpty()) {
        m_outputPathEdit->setText(path);
    }
}

void ReportDialog::onStartClicked() {
    if (m_videoPathEdit->text().isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择要分析的视频文件！");
        return;
    }
    if (m_outputPathEdit->text().isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择报告输出位置！");
        return;
    }
    setExporting(true);
    emit startExport(m_videoPathEdit->text(), m_outputPathEdit->text());
}

void ReportDialog::setProgress(int percent, const QString &text) {
    m_progressBar->setValue(percent);
    if (!text.isEmpty()) {
        m_statusLabel->setText(text);
    }
    if (percent >= 100) {
        setExporting(false);
    }
}

void ReportDialog::setExporting(bool exporting) {
    m_startBtn->setEnabled(!exporting);
    m_browseVideoBtn->setEnabled(!exporting);
    m_browseOutputBtn->setEnabled(!exporting);
    m_videoPathEdit->setEnabled(!exporting);
    m_outputPathEdit->setEnabled(!exporting);
    m_includeBasicInfoCheck->setEnabled(!exporting);
    m_includeMotionStatsCheck->setEnabled(!exporting);
    m_includeDirectionRoseCheck->setEnabled(!exporting);
    m_includeQPStatsCheck->setEnabled(!exporting);
    m_includeEncodingModeStatsCheck->setEnabled(!exporting);
    m_includeSpatialHeatmapCheck->setEnabled(!exporting);
    m_includeKeyFramesCheck->setEnabled(!exporting);
    
    if (exporting) {
        m_startBtn->setText("正在生成...");
    } else {
        m_startBtn->setText("开始生成");
    }
}

QString ReportDialog::outputPath() const {
    return m_outputPathEdit->text();
}

bool ReportDialog::includeBasicInfo() const {
    return m_includeBasicInfoCheck->isChecked();
}

bool ReportDialog::includeMotionStats() const {
    return m_includeMotionStatsCheck->isChecked();
}

bool ReportDialog::includeDirectionRose() const {
    return m_includeDirectionRoseCheck->isChecked();
}

bool ReportDialog::includeQPStats() const {
    return m_includeQPStatsCheck->isChecked();
}

bool ReportDialog::includeEncodingModeStats() const {
    return m_includeEncodingModeStatsCheck->isChecked();
}

bool ReportDialog::includeSpatialHeatmap() const {
    return m_includeSpatialHeatmapCheck->isChecked();
}

bool ReportDialog::includeKeyFrames() const {
    return m_includeKeyFramesCheck->isChecked();
}
