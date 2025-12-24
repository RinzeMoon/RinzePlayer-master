#ifndef HOMEWIDGET_H
#define HOMEWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QPropertyAnimation>
#include <QMap>
#include <QTime>
#include <QPainter>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QPixmap>
#include <QTransform>
// 新增：图形项相关头文件（Qt 6必带）
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsProxyWidget>

// 功能卡片类（基于QGraphicsView，无setTransform依赖）
class HomeFunctionCard : public QGraphicsView {
    Q_OBJECT
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked)
    Q_PROPERTY(qreal iconRotation READ iconRotation WRITE setIconRotation)
    Q_PROPERTY(qreal scaleFactor READ scaleFactor WRITE setScaleFactor)

public:
    explicit HomeFunctionCard(const QString& iconPath,
                             const QString& title,
                             const QString& desc,
                             bool isEnable = true,
                             QWidget *parent = nullptr);

    bool isChecked() const;
    void setChecked(bool checked);
    void updateDesc(const QString& newDesc);
    qreal iconRotation() const;
    void setIconRotation(qreal rotation);
    qreal scaleFactor() const;
    void setScaleFactor(qreal factor);

signals:
    void clicked();
    void checkedChanged(bool checked);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override; // 适配场景大小

private:
    // 内层内容控件（原卡片内容）
    QWidget *m_contentWidget;
    QLabel *m_iconLabel;
    QLabel *m_titleLabel;
    QLabel *m_descLabel;
    // 图形项相关（实现缩放）
    QGraphicsScene *m_scene;
    QGraphicsProxyWidget *m_proxy;
    // 状态变量
    bool m_isEnable;
    bool m_isChecked;
    qreal m_iconRotation;
    qreal m_scaleFactor;
    QPixmap m_originalIcon;
    // 动画
    QPropertyAnimation *m_iconAnim;
    QPropertyAnimation *m_scaleAnim;
};

// 主页类（不变）
class HomePage : public QWidget {
    Q_OBJECT

public:
    explicit HomePage(QWidget *parent = nullptr);

    void updateWelcomeText();
    void setCardLoading(const QString& cardKey, bool isLoading);

signals:
    void scanLocalMusicRequested();
    void recentPlayRequested();
    void recommendRequested();
    void searchRequested(const QString& keyword);

private:
    QVBoxLayout *m_mainLayout;
    QLabel *m_welcomeLabel;
    QLabel *m_subtitleLabel;
    QLineEdit *m_searchEdit;
    QGridLayout *m_cardsGridLayout;
    QMap<QString, HomeFunctionCard*> m_cardMap;
};

#endif // HOMEWIDGET_H