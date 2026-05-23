// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "exportdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>

ExportDialog::ExportDialog(QWidget *parent, const QString &defaultPath)
    : QDialog(parent)
{
    setWindowTitle("导出运动可视化视频");
    setMinimumSize(550, 500);

    // --- 主布局 ---
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);

    // --- 说明标签 ---
    QLabel *infoLabel = new QLabel("📝 说明：当前版本导出源视频的副本（演示导出框架）。\n完整的可视化叠加渲染功能正在开发中...", this);
    infoLabel->setStyleSheet("color: #666; padding: 10px; background: #f5f5f5; border-radius: 4px;");
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    // --- 输出路径 ---
    QLabel *pathLabel = new QLabel("输出文件：", this);
    QHBoxLayout *pathLayout = new QHBoxLayout();
    m_outputPathEdit = new QLineEdit(this);
    m_outputPathEdit->setPlaceholderText("选择保存位置...");
    m_browseBtn = new QPushButton("浏览...", this);
    pathLayout->addWidget(m_outputPathEdit);
    pathLayout->addWidget(m_browseBtn);
    mainLayout->addWidget(pathLabel);
    mainLayout->addLayout(pathLayout);

    // --- 音频选项 ---
    m_includeAudioCheck = new QCheckBox("包含音频", this);
    m_includeAudioCheck->setChecked(true);
    mainLayout->addWidget(m_includeAudioCheck);

    // --- 可视化选项 ---
    QGroupBox *visualGroup = new QGroupBox("选择要导出的可视化（暂未完全实现）", this);
    QVBoxLayout *visualLayout = new QVBoxLayout(visualGroup);

    m_exportVectorsCheck = new QCheckBox("运动向量可视化", this);
    m_exportHeatmapCheck = new QCheckBox("运动能量热力图", this);
    m_exportTrailCheck = new QCheckBox("运动轨迹图", this);
    m_exportHistoryCheck = new QCheckBox("运动历史图像", this);

    // 默认全选
    m_exportVectorsCheck->setChecked(true);
    m_exportHeatmapCheck->setChecked(true);
    m_exportTrailCheck->setChecked(true);
    m_exportHistoryCheck->setChecked(true);

    visualLayout->addWidget(m_exportVectorsCheck);
    visualLayout->addWidget(m_exportHeatmapCheck);
    visualLayout->addWidget(m_exportTrailCheck);
    visualLayout->addWidget(m_exportHistoryCheck);
    mainLayout->addWidget(visualGroup);

    // --- 进度条 ---
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    mainLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel("准备就绪", this);
    mainLayout->addWidget(m_statusLabel);

    // --- 按钮 ---
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_startBtn = new QPushButton("开始导出", this);
    QPushButton *cancelBtn = new QPushButton("取消", this);
    btnLayout->addStretch();
    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);

    // --- 自动设置默认路径 ---
    if (!defaultPath.isEmpty()) {
        QFileInfo info(defaultPath);
        QString newName = info.completeBaseName() + "_motion.mp4";
        QString newPath = info.absolutePath() + "/" + newName;
        m_outputPathEdit->setText(newPath);
    }

    // --- 连接信号 ---
    connect(m_browseBtn, &QPushButton::clicked, this, &ExportDialog::browseOutputPath);
    connect(m_startBtn, &QPushButton::clicked, this, &ExportDialog::onStartClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void ExportDialog::browseOutputPath() {
    QString path = QFileDialog::getSaveFileName(
        this,
        "选择输出文件",
        QString(),
        "MP4 视频 (*.mp4);;所有文件 (*.*)"
    );

    if (!path.isEmpty()) {
        if (!path.endsWith(".mp4", Qt::CaseInsensitive)) {
            path += ".mp4";
        }
        m_outputPathEdit->setText(path);
    }
}

void ExportDialog::onStartClicked() {
    if (m_outputPathEdit->text().isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择输出文件！");
        return;
    }
    emit startExport();
}

void ExportDialog::setProgress(int percent, const QString &text) {
    m_progressBar->setValue(percent);
    if (!text.isEmpty()) {
        m_statusLabel->setText(text);
    }
}

QString ExportDialog::outputPath() const {
    return m_outputPathEdit->text();
}

bool ExportDialog::includeAudio() const {
    return m_includeAudioCheck->isChecked();
}

bool ExportDialog::exportMotionVectors() const {
    return m_exportVectorsCheck->isChecked();
}

bool ExportDialog::exportHeatmap() const {
    return m_exportHeatmapCheck->isChecked();
}

bool ExportDialog::exportTrail() const {
    return m_exportTrailCheck->isChecked();
}

bool ExportDialog::exportHistory() const {
    return m_exportHistoryCheck->isChecked();
}