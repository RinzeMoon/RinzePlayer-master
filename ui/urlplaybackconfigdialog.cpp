// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "urlplaybackconfigdialog.h"
#include <QFormLayout>

UrlPlaybackConfigDialog::UrlPlaybackConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
}

void UrlPlaybackConfigDialog::setupUi()
{
    setWindowTitle("URL 播放配置");
    setMinimumWidth(450);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // URL 输入区域
    QLabel* urlLabel = new QLabel("流媒体 URL:", this);
    m_urlEdit = new QLineEdit(this);
    mainLayout->addWidget(urlLabel);
    mainLayout->addWidget(m_urlEdit);
    
    mainLayout->addSpacing(15);
    
    // 低延迟总开关
    m_lowLatencyCheckBox = new QCheckBox("启用低延迟模式", this);
    m_lowLatencyCheckBox->setChecked(false);
    connect(m_lowLatencyCheckBox, &QCheckBox::toggled, this, &UrlPlaybackConfigDialog::onLowLatencyToggled);
    mainLayout->addWidget(m_lowLatencyCheckBox);
    
    mainLayout->addSpacing(10);
    
    // 低延迟配置选项组
    QGroupBox* lowLatencyGroup = new QGroupBox("低延迟配置选项", this);
    QFormLayout* groupLayout = new QFormLayout(lowLatencyGroup);
    
    m_noBufferCheckBox = new QCheckBox("无缓冲 (NOBUFFER)", this);
    m_lowDelayFlagCheckBox = new QCheckBox("低延迟标志 (LOW_DELAY)", this);
    m_discardCorruptCheckBox = new QCheckBox("丢弃损坏帧 (DISCARD_CORRUPT)", this);
    m_noParseCheckBox = new QCheckBox("不解析 (NOPARSE)", this);
    m_shortProbeCheckBox = new QCheckBox("快速探测 (SHORT PROBE)", this);
    
    m_noBufferCheckBox->setEnabled(false);
    m_lowDelayFlagCheckBox->setEnabled(false);
    m_discardCorruptCheckBox->setEnabled(false);
    m_noParseCheckBox->setEnabled(false);
    m_shortProbeCheckBox->setEnabled(false);
    
    groupLayout->addRow(m_noBufferCheckBox);
    groupLayout->addRow(m_lowDelayFlagCheckBox);
    groupLayout->addRow(m_discardCorruptCheckBox);
    groupLayout->addRow(m_noParseCheckBox);
    groupLayout->addRow(m_shortProbeCheckBox);
    
    mainLayout->addWidget(lowLatencyGroup);
    
    mainLayout->addStretch();
    
    // 按钮区域
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_cancelBtn = new QPushButton("取消", this);
    m_acceptBtn = new QPushButton("确认播放", this);
    m_acceptBtn->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;");
    
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_acceptBtn, &QPushButton::clicked, this, &UrlPlaybackConfigDialog::onAcceptClicked);
    
    btnLayout->addStretch();
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_acceptBtn);
    
    mainLayout->addLayout(btnLayout);
}

void UrlPlaybackConfigDialog::setUrl(const QString& url)
{
    m_urlEdit->setText(url);
}

bool UrlPlaybackConfigDialog::lowLatencyEnabled() const
{
    return m_lowLatencyCheckBox->isChecked();
}

bool UrlPlaybackConfigDialog::noBufferEnabled() const
{
    return m_noBufferCheckBox->isChecked();
}

bool UrlPlaybackConfigDialog::lowDelayFlagEnabled() const
{
    return m_lowDelayFlagCheckBox->isChecked();
}

bool UrlPlaybackConfigDialog::discardCorruptEnabled() const
{
    return m_discardCorruptCheckBox->isChecked();
}

bool UrlPlaybackConfigDialog::noParseEnabled() const
{
    return m_noParseCheckBox->isChecked();
}

bool UrlPlaybackConfigDialog::shortProbeEnabled() const
{
    return m_shortProbeCheckBox->isChecked();
}

void UrlPlaybackConfigDialog::onLowLatencyToggled(bool checked)
{
    // 启用/禁用所有低延迟选项
    m_noBufferCheckBox->setEnabled(checked);
    m_lowDelayFlagCheckBox->setEnabled(checked);
    m_discardCorruptCheckBox->setEnabled(checked);
    m_noParseCheckBox->setEnabled(checked);
    m_shortProbeCheckBox->setEnabled(checked);
    
    // 如果启用，默认选中所有
    if (checked) {
        m_noBufferCheckBox->setChecked(true);
        m_lowDelayFlagCheckBox->setChecked(true);
        m_discardCorruptCheckBox->setChecked(true);
        m_noParseCheckBox->setChecked(true);
        m_shortProbeCheckBox->setChecked(true);
    } else {
        m_noBufferCheckBox->setChecked(false);
        m_lowDelayFlagCheckBox->setChecked(false);
        m_discardCorruptCheckBox->setChecked(false);
        m_noParseCheckBox->setChecked(false);
        m_shortProbeCheckBox->setChecked(false);
    }
}

void UrlPlaybackConfigDialog::onAcceptClicked()
{
    QString urlText = m_urlEdit->text().trimmed();
    if (urlText.isEmpty()) {
        return;
    }
    
    emit playRequested(QUrl::fromUserInput(urlText));
    accept();
}
