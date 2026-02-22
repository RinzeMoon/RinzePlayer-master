//
// Created by lsy on 2025/9/25.
//

#ifndef RINZEPLAYER_UTIL_H
#define RINZEPLAYER_UTIL_H
#include <QObject>
#include <QWidget>
#include <QListWidget>
#include <QTimer>
#include <QPainter>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>

#include <../Header/Manager/MusicSheetManager.h>

namespace Util
{

    class CollapsibleWidget : public QWidget
{
    Q_OBJECT

    // 自定义数据角色：区分项类型和存储歌单ID
    enum DataRole {
        IsCreateItemRole = Qt::UserRole,  // 标记是否为创建项（bool）
        SheetIdRole = Qt::UserRole + 1    // 存储歌单ID（QString）
    };

public:
    // 构造函数：传入面板标题
    explicit CollapsibleWidget(const QString& title, QWidget* parent = nullptr)
        : QWidget(parent), m_isExpanded(false)
    {
        // 主布局（无内边距和间距）
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // 标题按钮（控制折叠/展开）
        m_titleButton = new QPushButton(title, this);
        m_titleButton->setFixedHeight(40);
        mainLayout->addWidget(m_titleButton);

        // 列表容器（用于折叠时隐藏列表）
        m_listContainer = new QWidget(this);
        m_listContainer->setFixedHeight(0);  // 初始折叠
        mainLayout->addWidget(m_listContainer);

        // 列表容器布局
        QVBoxLayout* containerLayout = new QVBoxLayout(m_listContainer);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(0);

        // 列表控件（显示所有项）
        m_listWidget = new QListWidget(this);
        m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); // 逐像素滚动（平滑基础）
        m_listWidget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        containerLayout->addWidget(m_listWidget);

        // 添加"创建列表"项（固定第一项）
        createItem = new QListWidgetItem("+ 创建列表");
        createItem->setData(IsCreateItemRole, true);
        m_listWidget->addItem(createItem);

        // 初始化样式
        initStyles();

        // 绑定信号槽
        connect(m_titleButton, &QPushButton::clicked, this, &CollapsibleWidget::toggleExpand);
        connect(m_listWidget, &QListWidget::itemClicked, this, &CollapsibleWidget::onItemClicked);

    }

    // 添加带歌单ID的普通项
        void addItem(const QString& sheetName, const QString& sheetId)
        {
            // 如果是默认歌单，跳过（因为我们在reloadItems中已经单独处理了）
            if (sheetId == "0000") {
                qDebug() << "跳过默认歌单的重复添加";
                return;
            }

            qDebug() << "添加歌单:" << sheetName << "ID:" << sheetId;
            QListWidgetItem* item = new QListWidgetItem(sheetName);
            item->setData(IsCreateItemRole, false);
            item->setData(SheetIdRole, sheetId);
            m_listWidget->addItem(item);  // 其他歌单添加到末尾
        }

    // 兼容：添加无ID的普通项（不推荐，仅用于临时数据）
    void addItem(const QString& text)
    {
        // 自动生成一个唯一ID（如UUID）
        QString FavoriteId = QString("0001");
        qDebug() << "addItem,默认项目ID: " << text;
        addItem(text, FavoriteId); // 调用带ID的版本
    }

    bool isExpanded()
    {
        return m_isExpanded;
    }

    void changeItemById(const QString& sheetId,const QString& sheetName)
    {
        for (int i = 0;i < m_listWidget->count();++i)
        {
            QListWidgetItem* item = m_listWidget->item(i);
            if (item->data(IsCreateItemRole).toBool())
            {
                continue;
            }

            QString orginId = item->data(SheetIdRole).toString();
            if (orginId == sheetId)
            {
                item->setText(sheetName);
            }
        }
        qDebug() << "ItemName设置完毕";
        saveItems();
    }

        void saveItems() {
        QJsonArray itemsArray;

        // 【关键修改】首先添加默认歌单到JSON数组
        QJsonObject defaultSheetObj;
        defaultSheetObj["name"] = "我喜欢的音乐";
        defaultSheetObj["sheetId"] = "0000";
        itemsArray.append(defaultSheetObj);

        // 遍历m_listWidget中的所有item（从位置1开始，跳过创建项和默认歌单）
        for (int i = 2; i < m_listWidget->count(); ++i) {  // 从2开始：0=创建项，1=默认歌单
            QListWidgetItem* item = m_listWidget->item(i);

            // 跳过"创建列表"项
            if (item->data(IsCreateItemRole).toBool()) {
                continue;
            }

            // 提取item的核心数据
            QString sheetName = item->text();
            QString sheetId = item->data(SheetIdRole).toString();

            // 过滤无效项和默认歌单（因为我们已经单独处理了）
            if (sheetId.isEmpty() || sheetId == "0000") {
                continue;
            }

            // 存入JSON数组
            QJsonObject itemObj;
            itemObj["name"] = sheetName;
            itemObj["sheetId"] = sheetId;
            itemsArray.append(itemObj);
        }

        // 写入文件
        QFile file("../LocalSheets/itemMeta/item_state.json");
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QJsonDocument doc(itemsArray);
            file.write(doc.toJson(QJsonDocument::Indented));
            file.close();
            qDebug() << "item数据保存成功，共" << itemsArray.size() << "个有效item";

            // 验证保存的内容
            qDebug() << "保存的歌单顺序:";
            for (int i = 0; i < itemsArray.size(); ++i) {
                QJsonObject obj = itemsArray[i].toObject();
                qDebug() << "  " << i << ":" << obj["name"].toString() << "(" << obj["sheetId"].toString() << ")";
            }
        }
    }

    void loadItems() {
        /*
        QFile file("../LocalSheets/itemMeta/item_state.json");
        if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "无item历史数据，不加载";
            return;
        }

        // 解析JSON
        QByteArray data = file.readAll();
        file.close();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isArray()) {
            qWarning() << "item数据格式错误，无法加载";
            return;
        }

        QJsonArray itemsArray = doc.array();
        for (const QJsonValue& val : itemsArray) {
            QJsonObject itemObj = val.toObject();
            QString sheetName = itemObj["name"].toString();
            QString sheetId = itemObj["sheetId"].toString();

            // 过滤无效数据
            if (sheetName.isEmpty() || sheetId.isEmpty()) {
                continue;
            }

            // 调用addItem重建item（自动绑定sheetId）
            addItem(sheetName, sheetId);
        }

        qDebug() << "item数据加载成功，共恢复" << itemsArray.size() << "个item";
        */
        reloadItems();
    }

        void reloadItems() {
    qDebug() << "=== reloadItems 开始 ===";

    // 清空现有项目（除了创建项）
    int count = m_listWidget->count();
    for (int i = count - 1; i >= 1; --i) {
        QListWidgetItem* item = m_listWidget->takeItem(i);
        delete item;
    }

    QSet<QString> loadedSheetIds;

    // 【关键修改】首先添加默认歌单 "我喜欢的音乐" (0000) 到最上面
    MusicSheetManager* manager = MusicSheetManager::getInstance();

    // 确保默认歌单已加载
    if (!manager->getSheets().contains("0000")) {
        manager->loadSheetByIdFromLocal("0000");
    }

    // 强制添加默认歌单到第一个位置（在"创建列表"之后）
    QString defaultTitle = manager->getSheetById("0000").title;
    qDebug() << "添加默认歌单到最上面:" << defaultTitle << "ID: 0000";

    // 在位置1插入（0是"创建列表"，1就是第二个位置）
    QListWidgetItem* defaultItem = new QListWidgetItem(defaultTitle);
    defaultItem->setData(IsCreateItemRole, false);
    defaultItem->setData(SheetIdRole, "0000");
    m_listWidget->insertItem(1, defaultItem);  // 使用insertItem而不是addItem

    loadedSheetIds.insert("0000");
    qDebug() << "默认歌单已添加到位置1";

    // 从 item_state.json 加载其他歌单项
    QFile file("../LocalSheets/itemMeta/item_state.json");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull() && doc.isArray()) {
            QJsonArray itemsArray = doc.array();
            qDebug() << "从 item_state.json 读取到" << itemsArray.size() << "个歌单项";

            for (const QJsonValue& val : itemsArray) {
                QJsonObject itemObj = val.toObject();
                QString name = itemObj["name"].toString();
                QString sheetId = itemObj["sheetId"].toString();

                // 跳过默认歌单，因为我们已经单独处理了
                if (sheetId == "0000") {
                    continue;
                }

                // 确保对应的歌单数据已加载到内存
                if (!manager->getSheets().contains(sheetId)) {
                    manager->loadSheetByIdFromLocal(sheetId);
                }

                // 使用歌单数据中的实际标题
                QString actualTitle = manager->getSheetById(sheetId).title;
                qDebug() << "添加其他歌单:" << actualTitle << "ID:" << sheetId;
                addItem(actualTitle, sheetId);  // 这些会添加到末尾
                loadedSheetIds.insert(sheetId);
            }
        }
    }

    // 最终验证
    qDebug() << "reloadItems完成，列表项目数:" << m_listWidget->count();
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem* item = m_listWidget->item(i);
        qDebug() << "位置" << i << "文本:" << item->text() << "ID:" << item->data(SheetIdRole).toString();
    }

    if (m_isExpanded) {
        int totalHeight = m_listWidget->count() * 36;
        m_listContainer->setFixedHeight(totalHeight);
    }
}
signals:
    // 点击"创建列表"项时发射
    void createSheetRequested();
    // 点击普通歌单项时发射（传递歌单ID）
    void sheetItemClicked(const QString& sheetId);

private slots:
    // 切换展开/折叠状态
    void toggleExpand()
    {
        m_isExpanded = !m_isExpanded;

        if (m_isExpanded) {
            // 展开：计算总高度（每项36px）
            int totalHeight = m_listWidget->count() * 36;
            m_listContainer->setFixedHeight(totalHeight);
            // 标题高亮
            m_titleButton->setStyleSheet(m_titleButton->styleSheet() + "color: #e63946;");
        } else {
            // 折叠：隐藏列表
            m_listContainer->setFixedHeight(0);
            // 标题恢复默认色
            m_titleButton->setStyleSheet(m_titleButton->styleSheet().replace("color: #e63946;", "color: #666666;"));
        }
    }

    // 处理列表项点击
    void onItemClicked(QListWidgetItem* item)
    {
        bool isCreateItem = item->data(IsCreateItemRole).toBool();
        if (isCreateItem) {
            emit createSheetRequested();  // 触发创建逻辑
        } else {
            QString sheetId = item->data(SheetIdRole).toString();
            if (!sheetId.isEmpty()) {
                emit sheetItemClicked(sheetId);  // 触发歌单加载
            }
        }
    }

private:
    // 初始化样式表
    void initStyles()
    {
        // 标题按钮样式
        m_titleButton->setStyleSheet(R"(
            QPushButton {
                border: none;
                background-color: transparent;
                color: #666666;
                text-align: left;
                padding-left: 24px;
                font-size: 14px;
            }
            QPushButton:hover {
                background-color: #f8edeb;
                color: #e63946;
            }
        )");

        // 列表容器背景
        m_listContainer->setStyleSheet("background-color: #f9f9f9;");

        // 列表及项样式
        m_listWidget->setStyleSheet(R"(
        /* 列表项基础样式（不变） */
        QListWidget {
            border: none;
            background-color: transparent;
            padding-left: 12px;
            color: #666666;
            padding-right: 4px; /* 给滚动条留一点空间，避免内容贴边 */
        }
        QListWidget::item {
            height: 36px;
            border-radius: 4px;
            padding-left: 12px;
        }
        QListWidget::item:hover {
            background-color: #f8edeb;
            color: #e63946;
        }
        QListWidget::item:selected {
            background-color: #e63946;
            color: white;
        }

        /* === 现代隐形滚动条 === */
        /* 垂直滚动条整体：极细且透明 */
        QListWidget QScrollBar:vertical {
            width: 5px; /* 极致纤细，现代感核心 */
            background: transparent; /* 完全透明背景 */
            margin: 5px 0; /* 上下留白，避免滑块顶边 */
        }

        /* 轨道：彻底隐藏（无背景） */
        QListWidget QScrollBar:vertical::track {
            background: transparent; /* 无轨道视觉 */
        }

        /* 滑块：默认半透明椭圆形，几乎隐形 */
        QListWidget QScrollBar:vertical::handle {
            background: rgba(150, 150, 150, 0.3); /* 浅灰半透明（默认几乎看不见） */
            border-radius: 2.5px; /* 宽度5px → 半径2.5px，完美椭圆 */
            min-height: 30px; /* 滑块长度适中 */
            margin: 0; /* 无额外边距，紧贴右侧 */
        }

        /* 鼠标hover滑块：颜色加深，显形 */
        QListWidget QScrollBar:vertical::handle:hover {
            background: rgba(100, 100, 100, 0.6); /* 中等灰度，清晰可见 */
        }

        /* 鼠标按下滑块：颜色再深，强化交互 */
        QListWidget QScrollBar:vertical::handle:pressed {
            background: rgba(80, 80, 80, 0.8); /* 深灰，明确按下状态 */
        }

        /* 隐藏所有箭头按钮和多余元素 */
        QListWidget QScrollBar:vertical::sub-line,
        QListWidget QScrollBar:vertical::add-line,
        QListWidget QScrollBar:vertical::sub-page,
        QListWidget QScrollBar:vertical::add-page {
            height: 0;
            background: transparent;
        }

        /* 滚动条禁用时完全隐藏 */
        QListWidget QScrollBar:vertical:disabled {
            width: 0;
        }
    )");

    }

    // 成员变量
    QPushButton* m_titleButton = nullptr;    // 标题按钮
    QWidget* m_listContainer = nullptr;      // 列表容器（控制折叠）
    QListWidget* m_listWidget = nullptr;     // 列表控件
    bool m_isExpanded;                       // 展开状态标记
    QListWidgetItem* createItem;

    QString itemStorgaePath = "../LocalSheets/";
};




    class ScrollingLabel : public QLabel
{
    Q_OBJECT

public:
    // 构造函数：parent为父窗口，scrollSpeed为滚动间隔(毫秒)
    explicit ScrollingLabel(const QString & Text,QWidget *parent = nullptr, int scrollSpeed = 50)
        : QLabel(parent)
        , m_scrollSpeed(scrollSpeed)
        , m_scrollPos(0)
        , m_needScroll(false)
    {
        // 初始化滚动定时器
        m_scrollTimer = new QTimer(this);
        m_scrollTimer->setInterval(m_scrollSpeed);
        connect(m_scrollTimer, &QTimer::timeout, this, &ScrollingLabel::updateScrollPosition);
        setText(Text);
    }

        // 显式引入父类的setText方法，避免隐藏
    using QLabel::setText;

    // 重写setText（针对字符串参数）
    void setText(const QString &text)
    {
        m_originalText = text;
        QLabel::setText(text); // 调用父类实现

        // 计算文本宽度并检查是否需要滚动
        QFontMetrics metrics(font());
        m_textWidth = metrics.horizontalAdvance(m_originalText);
        checkNeedScroll();
    }

    // 设置滚动速度(毫秒/像素)
    void setScrollSpeed(int speed)
    {
        if (speed > 0) {
            m_scrollSpeed = speed;
            m_scrollTimer->setInterval(m_scrollSpeed);
        }
    }

    void startScroll()
    {
        if (m_needScroll && !m_isScrolling) {
            m_scrollTimer->start();
            m_isScrolling = true;
            update(); // 触发重绘
        }
    }

    void stopScroll()
    {
        if (m_isScrolling) {
            m_scrollTimer->stop();
            m_isScrolling = false;
            update(); // 刷新显示（停在当前位置）
        }
    }

    void toggleScroll()
    {
        if (m_isScrolling) {
            stopScroll();
        } else {
            startScroll();
        }
    }

protected:
    // 重写绘制事件
    void paintEvent(QPaintEvent *event) override
    {
        if (!m_needScroll || !m_isScrolling) {
            // 不需要滚动或已停止时，使用父类绘制逻辑（显示完整文本或当前位置）
            QLabel::paintEvent(event);
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::TextAntialiasing); // 文本抗锯齿

        QRect rect = contentsRect();
        QFontMetrics metrics(font());

        // 计算文本垂直居中位置
        int textHeight = metrics.height(); // 文本总高度（ascent + descent）
        // 计算基线位置：垂直居中 = 区域顶部 + (区域高度 - 文本高度)/2 + 上行高度（ascent）
        int baselineY = rect.top() + (rect.height() - textHeight) / 2 + metrics.ascent();

        // 绘制主文本（根据滚动位置偏移）
        painter.drawText(rect.x() - m_scrollPos, baselineY, m_originalText);

        // 绘制衔接文本实现无缝循环（优化衔接位置，避免间隙/重叠）
        int offset = m_textWidth + rect.width(); // 完整的衔接间隔
        if (m_scrollPos > rect.width()) {
            painter.drawText(rect.x() - m_scrollPos + offset, baselineY, m_originalText);
        }
    }

    // 重写大小变化事件
    void resizeEvent(QResizeEvent *event) override
    {
        QLabel::resizeEvent(event);
        checkNeedScroll(); // 大小改变时重新检查滚动需求
    }

private slots:
    // 更新滚动位置
    void updateScrollPosition()
    {
        if (!m_needScroll || !m_isScrolling) return;

        m_scrollPos++;
        // 滚动到末端后重置位置（实现循环）
        if (m_scrollPos > m_textWidth + contentsRect().width() / 2) {
            m_scrollPos = 0;
        }
        update(); // 触发重绘
    }

private:
    // 检查是否需要滚动（根据文本宽度和控件宽度）
    void checkNeedScroll()
    {
        int labelWidth = contentsRect().width();
        m_needScroll = (m_textWidth > labelWidth);

        // 如果不需要滚动，强制停止
        if (!m_needScroll && m_isScrolling) {
            stopScroll();
        }
        // 如果需要滚动且未开始，可自动启动（也可注释掉，改为手动启动）
        else if (m_needScroll && !m_isScrolling) {
            startScroll(); // 文本过长时自动开始滚动（按需修改）
        }
    }

    // 成员变量
    QString m_originalText;    // 原始文本
    QTimer *m_scrollTimer;     // 滚动定时器
    int m_scrollSpeed;         // 滚动速度(毫秒/像素)
    int m_scrollPos;           // 当前滚动位置
    int m_textWidth;           // 文本宽度（像素）
    bool m_needScroll;         // 是否需要滚动（文本过长）
    bool m_isScrolling;        // 当前是否正在滚动（控制状态）
};
};



#endif //RINZEPLAYER_UTIL_H