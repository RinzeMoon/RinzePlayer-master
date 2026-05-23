// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef URLPLAYBACKCONFIGDIALOG_H
#define URLPLAYBACKCONFIGDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QUrl>

class UrlPlaybackConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UrlPlaybackConfigDialog(QWidget* parent = nullptr);
    ~UrlPlaybackConfigDialog() = default;
    
    void setUrl(const QString& url);
    
    bool lowLatencyEnabled() const;
    bool noBufferEnabled() const;
    bool lowDelayFlagEnabled() const;
    bool discardCorruptEnabled() const;
    bool noParseEnabled() const;
    bool shortProbeEnabled() const;

signals:
    void playRequested(const QUrl& url);

private slots:
    void onAcceptClicked();
    void onLowLatencyToggled(bool checked);

private:
    void setupUi();

    QLineEdit* m_urlEdit;
    QCheckBox* m_lowLatencyCheckBox;
    QCheckBox* m_noBufferCheckBox;
    QCheckBox* m_lowDelayFlagCheckBox;
    QCheckBox* m_discardCorruptCheckBox;
    QCheckBox* m_noParseCheckBox;
    QCheckBox* m_shortProbeCheckBox;
    QPushButton* m_acceptBtn;
    QPushButton* m_cancelBtn;
};

#endif // URLPLAYBACKCONFIGDIALOG_H
