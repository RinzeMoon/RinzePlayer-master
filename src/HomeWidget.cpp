#include <../Header/Ui/HomeWidget.h>

#include <QGuiApplication>
#include <QScreen>
#include <QEasingCurve>
#include <QPainter>
#include <QFontMetrics>
#include <QResizeEvent>

// 全局样式常量
const QString PRIMARY_COLOR = "#4361EE";
const QString PRIMARY_LIGHT = "#C7D2FE";
const QString CARD_BG = "#FFFFFF";
const QString PAGE_BG_START = "#FAFAFC";
const QString PAGE_BG_END = "#F3F4F6";
const QString TEXT_MAIN = "#1A1A2E";
const QString TEXT_LIGHT = "#6B7280";
const QString BORDER_NORMAL = "#E5E7EB";
const QString BORDER_PLACEHOLDER = "#D1D5DB";
const QString SHADOW_COLOR = "rgba(67, 97, 238, 0.08)";

// --------------------------
// HomeFunctionCard 实现（核心修复）
// --------------------------
HomeFunctionCard::HomeFunctionCard(const QString& iconPath,
                                 const QString& title,
                                 const QString& desc,
                                 bool isEnable,
                                 QWidget *parent)
    : QGraphicsView(parent),
      m_isEnable(isEnable),
      m_isChecked(false),
      m_iconRotation(0),
      m_scaleFactor(1.0)
{
    // 视图样式（不变）
    setStyleSheet("border: none; background: transparent;");
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setRenderHint(QPainter::Antialiasing);

    // 场景设置（不变）
    m_scene = new QGraphicsScene(this);
    m_scene->setBackgroundBrush(Qt::NoBrush);
    setScene(m_scene);

    // 核心修改：缩短卡片宽度至280px（原340px），高度微调至100px
    m_contentWidget = new QWidget();
    m_contentWidget->setFixedSize(280, 100); // 宽度减小60px，避免遮挡
    m_contentWidget->setStyleSheet(QString(R"(
        QWidget {
            background-color: %1;
            border-radius: 12px;
        }
    )"));

    // 图标、文字控件（不变）
    m_iconLabel = new QLabel(m_contentWidget);
    m_titleLabel = new QLabel(title, m_contentWidget);
    m_descLabel = new QLabel(desc, m_contentWidget);

    // 图标处理（不变）
    m_originalIcon.load(iconPath);
    if (!m_originalIcon.isNull()) {
        QPixmap coloredIcon = m_originalIcon;
        QPainter painter(&coloredIcon);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        QColor iconColor = m_isEnable ? QColor(PRIMARY_COLOR) : QColor(TEXT_LIGHT).lighter(110);
        painter.fillRect(coloredIcon.rect(), iconColor);
        m_originalIcon = coloredIcon;
        m_iconLabel->setPixmap(m_originalIcon.scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation)); // 图标略小
    }
    m_iconLabel->setAlignment(Qt::AlignCenter);

    // 文字样式（调整字体大小适配窄卡片）
    m_titleLabel->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 600;") // 字体略小
                               .arg(m_isEnable ? TEXT_MAIN : TEXT_LIGHT));
    m_titleLabel->setAlignment(Qt::AlignLeft);

    m_descLabel->setStyleSheet(QString("color: %1; font-size: 11px;") // 字体略小
                              .arg(m_isEnable ? TEXT_LIGHT : QColor(TEXT_LIGHT).lighter(110).name()));
    m_descLabel->setAlignment(Qt::AlignLeft);
    m_descLabel->setWordWrap(true);
    m_descLabel->setMaximumWidth(180); // 文字区域宽度适配窄卡片

    // 内容布局（缩小内边距和间距，避免拥挤）
    auto* cardLayout = new QHBoxLayout(m_contentWidget);
    cardLayout->setContentsMargins(20, 16, 20, 16); // 内边距减小
    cardLayout->setSpacing(12); // 间距减小
    cardLayout->addWidget(m_iconLabel);
    auto* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2); // 文字间距减小
    textLayout->addWidget(m_titleLabel);
    textLayout->addWidget(m_descLabel);
    textLayout->addStretch();
    cardLayout->addLayout(textLayout);
    cardLayout->addStretch();

    // 图形项包装（适配新尺寸）
    m_proxy = m_scene->addWidget(m_contentWidget);
    m_proxy->setTransformOriginPoint(m_contentWidget->width()/2, m_contentWidget->height()/2);
    m_scene->setSceneRect(m_contentWidget->rect());

    // 视图尺寸适配新卡片
    setFixedSize(m_contentWidget->size() + QSize(4, 4));
    centerOn(m_proxy);

    // 动画初始化（不变）
    m_iconAnim = new QPropertyAnimation(this, "iconRotation", this);
    m_iconAnim->setDuration(1500);
    m_iconAnim->setStartValue(0);
    m_iconAnim->setEndValue(360);
    m_iconAnim->setLoopCount(0);
    m_iconAnim->stop();

    m_scaleAnim = new QPropertyAnimation(this, "scaleFactor", this);
    m_scaleAnim->setDuration(150);
    m_scaleAnim->setEasingCurve(QEasingCurve::InOutCubic);

    setCursor(m_isEnable ? Qt::PointingHandCursor : Qt::ArrowCursor);
}
// 基础属性
bool HomeFunctionCard::isChecked() const { return m_isChecked; }
void HomeFunctionCard::setChecked(bool checked) {
    if (m_isChecked == checked) return;
    m_isChecked = checked;
    emit checkedChanged(checked);
    m_contentWidget->setStyleSheet(m_contentWidget->styleSheet() + QString(R"(
        QWidget {
            border-color: %1;
            background-color: %2;
        }
    )").arg(PRIMARY_COLOR, checked ? "rgba(67, 97, 238, 0.05)" : CARD_BG));
}

void HomeFunctionCard::updateDesc(const QString& newDesc) {
    m_descLabel->setText(newDesc);
    QFont descFont = m_descLabel->font();
    QFontMetrics descFm(descFont);
    m_descLabel->setText(descFm.elidedText(newDesc, Qt::ElideRight, 220 * 2));
}

qreal HomeFunctionCard::iconRotation() const { return m_iconRotation; }
void HomeFunctionCard::setIconRotation(qreal rotation) {
    if (m_iconRotation == rotation || m_originalIcon.isNull()) return;
    m_iconRotation = rotation;
    QTransform transform;
    transform.rotate(rotation);
    QPixmap rotatedIcon = m_originalIcon.transformed(transform, Qt::SmoothTransformation);
    m_iconLabel->setPixmap(rotatedIcon.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

// 缩放控制（核心：限制最大缩放，避免遮挡）
qreal HomeFunctionCard::scaleFactor() const { return m_scaleFactor; }
void HomeFunctionCard::setScaleFactor(qreal factor) {
    m_scaleFactor = factor;
    m_proxy->setScale(factor);
    // 缩放后重新居中，防止偏移出视图
    centerOn(m_proxy);
}

// 悬停进入（放大+阴影增强）
void HomeFunctionCard::enterEvent(QEnterEvent *event) {
    Q_UNUSED(event);
    if (!m_isEnable) return;
    // 缩放比例从1.03降至1.02，进一步减少遮挡
    m_scaleAnim->setStartValue(1.0);
    m_scaleAnim->setEndValue(1.02);
    m_scaleAnim->start();
    if (!m_isChecked) m_iconAnim->start();
    // 悬停样式（仅阴影变化）
    m_contentWidget->setStyleSheet(m_contentWidget->styleSheet() + QString(R"(
        QWidget {
            box-shadow: 0 4px 16px %2;
        }
    )").arg(PRIMARY_LIGHT, SHADOW_COLOR));
}

// 悬停离开（恢复）
void HomeFunctionCard::leaveEvent(QEvent *event) {
    Q_UNUSED(event);
    if (!m_isEnable) return;
    m_scaleAnim->setStartValue(m_scaleFactor);
    m_scaleAnim->setEndValue(1.0);
    m_scaleAnim->start();
    m_iconAnim->stop();
    setIconRotation(0);
    m_contentWidget->setStyleSheet(QString(R"(
        QWidget {
            background-color: %1;
            border-radius: 12px;  /* 保留圆角 */
        }
    )"));
}

// 点击反馈
void HomeFunctionCard::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    if (m_isEnable && event->button() == Qt::LeftButton) {
        emit clicked();
        QPropertyAnimation *pressAnim = new QPropertyAnimation(this, "scaleFactor", this);
        pressAnim->setDuration(100);
        pressAnim->setKeyValues({
            {0.0, 1.03},
            {0.5, 0.97},
            {1.0, 1.03}
        });
        pressAnim->start(QAbstractAnimation::DeleteWhenStopped);
    }
    QGraphicsView::mouseReleaseEvent(event);
}

// 视图大小调整（确保场景与视图匹配）
void HomeFunctionCard::resizeEvent(QResizeEvent *event) {
    Q_UNUSED(event);
    if (m_scene && m_contentWidget) {
        m_scene->setSceneRect(0, 0, width(), height());
        centerOn(m_proxy);
    }
}

// --------------------------
// HomePage 实现（优化布局间距，解决遮挡）
// --------------------------
HomePage::HomePage(QWidget *parent) : QWidget(parent) {
    setStyleSheet(QString("background: linear-gradient(to bottom, %1, %2);")
                 .arg(PAGE_BG_START, PAGE_BG_END));

    m_mainLayout = new QVBoxLayout(this);
    int screenWidth = QGuiApplication::primaryScreen()->geometry().width();
    int horizontalMargin = screenWidth > 1200 ? 150 : (screenWidth > 800 ? 80 : 30);
    m_mainLayout->setContentsMargins(horizontalMargin, 60, horizontalMargin, 60);
    m_mainLayout->setSpacing(32);

    // 标题区（不变）
    auto* titleContainer = new QWidget(this);
    titleContainer->setStyleSheet("background: transparent;");
    auto* titleLayout = new QHBoxLayout(titleContainer);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(20);

    auto* textContainer = new QWidget(this);
    auto* textLayout = new QVBoxLayout(textContainer);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(4);

    m_welcomeLabel = new QLabel(this);
    updateWelcomeText();
    m_welcomeLabel->setStyleSheet(QString("color: %1; font-size: 28px; font-weight: 700;")
                                 .arg(TEXT_MAIN));

    m_subtitleLabel = new QLabel("管理你的音乐库，发现更多精彩", this);
    m_subtitleLabel->setStyleSheet(QString("color: %1; font-size: 14px;").arg(TEXT_LIGHT));

    textLayout->addWidget(m_welcomeLabel);
    textLayout->addWidget(m_subtitleLabel);
    titleLayout->addWidget(textContainer);
    titleLayout->addStretch();

    m_mainLayout->addWidget(titleContainer);

    // 卡片区：增大间距至35px，避免拥挤
    m_cardsGridLayout = new QGridLayout();
    m_cardsGridLayout->setSpacing(35); // 间距比之前更大，配合窄卡片
    int columnCount = screenWidth > 1200 ? 3 : (screenWidth > 800 ? 2 : 1);

    // 添加卡片（不变）
    HomeFunctionCard* scanCard = new HomeFunctionCard(
        ":/icons/scan.svg", "扫描本地音乐",
        "自动导入设备中的音乐文件，快速构建本地库", true, this
    );
    connect(scanCard, &HomeFunctionCard::clicked, this, &HomePage::scanLocalMusicRequested);
    m_cardMap.insert("scan", scanCard);

    HomeFunctionCard* recentCard = new HomeFunctionCard(
        ":/icons/recent.svg", "最近播放",
        "查看历史播放记录，快速找回听过的歌曲", true, this
    );
    connect(recentCard, &HomeFunctionCard::clicked, this, &HomePage::recentPlayRequested);
    m_cardMap.insert("recent", recentCard);

    HomeFunctionCard* recommendCard = new HomeFunctionCard(
        ":/icons/recommend.svg", "发现推荐",
        "基于你的喜好，智能推荐相似风格音乐", true, this
    );
    connect(recommendCard, &HomeFunctionCard::clicked, this, &HomePage::recommendRequested);
    m_cardMap.insert("recommend", recommendCard);

    HomeFunctionCard* serverCard = new HomeFunctionCard(
        ":/icons/server.svg", "导入服务器歌曲",
        "即将支持从音乐服务器同步歌曲库", false, this
    );
    m_cardMap.insert("server", serverCard);

    // 布局卡片（不变）
    QList<HomeFunctionCard*> cardList = m_cardMap.values();
    for (int i = 0; i < cardList.size(); i++) {
        m_cardsGridLayout->addWidget(cardList[i], i / columnCount, i % columnCount);
    }

    m_mainLayout->addLayout(m_cardsGridLayout);
    m_mainLayout->addStretch();
}

// 其他方法不变
void HomePage::updateWelcomeText() {
    int hour = QTime::currentTime().hour();
    QString prefix = hour >=6 && hour <12 ? "早上好" :
                    (hour >=12 && hour <18 ? "下午好" :
                    (hour >=18 && hour <22 ? "晚上好" : "夜深了"));
    m_welcomeLabel->setText(QString("%1，音乐中心").arg(prefix));
}

void HomePage::setCardLoading(const QString& cardKey, bool isLoading) {
    if (!m_cardMap.contains(cardKey)) return;
    HomeFunctionCard* card = m_cardMap[cardKey];

    if (isLoading) {
        card->setEnabled(false);
        card->updateDesc("正在加载...");
        card->findChild<QPropertyAnimation*>("m_iconAnim")->start();
    } else {
        card->setEnabled(true);
        if (cardKey == "scan") card->updateDesc("自动导入设备中的音乐文件，快速构建本地库");
        else if (cardKey == "recent") card->updateDesc("查看历史播放记录，快速找回听过的歌曲");
        else if (cardKey == "recommend") card->updateDesc("基于你的喜好，智能推荐相似风格音乐");
        card->findChild<QPropertyAnimation*>("m_iconAnim")->stop();
        card->setIconRotation(0);
    }
}