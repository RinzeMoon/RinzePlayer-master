//
// Created by lsy on 2025/12/10.
//

#include "../Header/Lrc/LyricParser.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <algorithm>
#include <QStringConverter>
#include <QMutexLocker>

// ========== 静态成员初始化 ==========
LyricParserThreadPool* LyricParserThreadPool::m_instance = nullptr;

// ========== 原有 LyricParser 实现（完全适配你的定义） ==========
LyricParser::LyricParser(QObject* parent) : QObject(parent), m_lastQueryIndex(0) {}

bool LyricParser::loadFromFile(const QString& filePath) {
    clear();
    m_lastQueryIndex = 0;

    if (!QFile::exists(filePath)) {
        emit error(QString("歌词文件不存在: %1").arg(filePath));
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit error(QString("无法打开歌词文件: %1").arg(filePath));
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString content = stream.readAll();
    file.close();

    bool success = parseContent(content);
    emit loaded(success);
    return success;
}

bool LyricParser::loadFromContent(const QString& content) {
    clear();
    m_lastQueryIndex = 0;
    bool success = parseContent(content);
    emit loaded(success);
    return success;
}

void LyricParser::clear() {
    m_lines.clear();
    m_lastQueryIndex = 0;
}

bool LyricParser::parseContent(const QString& content) {
    QStringList lines = content.split('\n', Qt::SkipEmptyParts);
    m_lines.reserve(lines.size());

    for (const QString& rawLine : lines) {
        parseSingleLine(rawLine.trimmed());
    }

    // 按时间排序（使用你定义的 < 运算符）
    std::sort(m_lines.begin(), m_lines.end());

    // 合并同一时间点的多语言行（提升查询效率）
    LyricsList merged;
    for (int i = 0; i < m_lines.size(); ++i) {
        if (merged.isEmpty() || merged.last().timeMs != m_lines[i].timeMs) {
            merged.append(m_lines[i]);
        } else {
            // 合并相同时间点的多类型文本（包含你新增的KANA等）
            for (const QString& type : m_lines[i].availableTypes()) {
                merged.last().setText(type, m_lines[i].getText(type));
            }
        }
    }
    m_lines = merged;

    qDebug() << "[LyricParser] 解析完成，共" << m_lines.size()
             << "行歌词（合并后）";
    return !m_lines.isEmpty();
}

void LyricParser::parseSingleLine(const QString& line) {
    // 正则匹配时间标签：[mm:ss.xx] [mm:ss] [m:ss]
    static QRegularExpression timeRegex(
        R"(\[(\d{1,}):(\d{2})(?:\.(\d{1,3}))?\])"
    );

    QRegularExpressionMatchIterator timeIter = timeRegex.globalMatch(line);
    QVector<qint64> timePoints;
    int lastPos = 0;

    // 1. 提取所有时间标签
    while (timeIter.hasNext()) {
        QRegularExpressionMatch match = timeIter.next();
        qint64 ms = parseTimeTag(match.captured(0));
        timePoints.append(ms);
        lastPos = match.capturedEnd();
    }

    if (timePoints.isEmpty()) {
        return; // 无时间标签，跳过
    }

    // 2. 提取标签后的内容
    QString content = line.mid(lastPos).trimmed();
    if (content.isEmpty()) return;

    // 3. 解析多语言标签：<romaji>文本</romaji> <kana>文本</kana> 等
    static QRegularExpression tagRegex(
        R"(<(\w+)>(.*?)</\1>)",
        QRegularExpression::CaseInsensitiveOption |
        QRegularExpression::DotMatchesEverythingOption
    );

    QHash<QString, QString> parsedTexts;
    QString remaining = content;
    QRegularExpressionMatchIterator tagIter = tagRegex.globalMatch(content);

    while (tagIter.hasNext()) {
        QRegularExpressionMatch match = tagIter.next();
        QString tag = match.captured(1).toLower();
        QString text = match.captured(2).trimmed();

        QString type = mapTagToType(tag);
        if (!type.isEmpty() && !text.isEmpty()) {
            parsedTexts.insert(type, text);
        }
        remaining.remove(match.capturedStart(), match.capturedLength());
    }

    // 4. 剩余内容作为原文（清理残留标签）
    remaining = remaining.trimmed();
    static QRegularExpression leftoverTags(R"(</?\w+[^>]*>)");
    remaining.remove(leftoverTags);
    remaining = remaining.trimmed();

    if (!remaining.isEmpty()) {
        parsedTexts.insert(ORIGINAL, remaining);
    }

    // 5. 为每个时间点创建歌词行（使用你的 LyricLine 结构体）
    for (qint64 timeMs : timePoints) {
        if (!parsedTexts.isEmpty()) {
            LyricLine lyric;
            lyric.timeMs = timeMs;
            lyric.texts = parsedTexts;
            m_lines.append(lyric);
        }
    }
}

qint64 LyricParser::parseTimeTag(const QString& timeTag) {
    static QRegularExpression regex(R"(\[(\d+):(\d{2})(?:\.(\d{1,3}))?\])");
    QRegularExpressionMatch match = regex.match(timeTag);
    if (!match.hasMatch()) return 0;

    qint64 minutes = match.captured(1).toLongLong();
    qint64 seconds = match.captured(2).toLongLong();
    QString msStr = match.captured(3);

    qint64 milliseconds = 0;
    if (!msStr.isEmpty()) {
        milliseconds = msStr.toLongLong();
        if (msStr.length() == 1) milliseconds *= 100;
        else if (msStr.length() == 2) milliseconds *= 10;
    }

    return minutes * 60000 + seconds * 1000 + milliseconds;
}

QString LyricParser::mapTagToType(const QString& tag) {
    static const QHash<QString, QString> tagMap = {
        // 罗马音/拼音
        {"romaji", ROMANIZATION},
        {"romanization", ROMANIZATION},
        {"rmj", ROMANIZATION},
        {"pinyin", ROMANIZATION},
        {"py", ROMANIZATION},
        // 翻译
        {"trans", TRANSLATION},
        {"translation", TRANSLATION},
        {"zh", TRANSLATION},
        {"cn", TRANSLATION},
        {"en", TRANSLATION},
        // 假名（新增，适配你的KANA类型）
        {"kana", KANA},
        {"hiragana", KANA},
        {"katakana", KANA},
        {"jp", KANA}
    };

    return tagMap.value(tag.toLower(), "");
}

int LyricParser::findIndexAtTime(qint64 currentMs) const {
    if (m_lines.isEmpty()) return -1;

    // 优化：检查上次查询的附近（针对连续播放）
    if (m_lastQueryIndex < m_lines.size()) {
        if (m_lastQueryIndex > 0 &&
            m_lines[m_lastQueryIndex - 1].timeMs <= currentMs &&
            (m_lastQueryIndex == m_lines.size() - 1 || m_lines[m_lastQueryIndex].timeMs > currentMs)) {
            return m_lastQueryIndex - 1;
        }

        if (m_lastQueryIndex < m_lines.size() - 1 &&
            m_lines[m_lastQueryIndex].timeMs <= currentMs &&
            m_lines[m_lastQueryIndex + 1].timeMs > currentMs) {
            m_lastQueryIndex++;
            return m_lastQueryIndex - 1;
        }
    }

    // 二分查找（标准算法）
    int left = 0, right = m_lines.size() - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (m_lines[mid].timeMs <= currentMs) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    m_lastQueryIndex = (right >= 0) ? right + 1 : 0;
    return right;
}

LyricLine LyricParser::getLineAtTime(qint64 currentMs) const {
    int index = findIndexAtTime(currentMs);
    return getLine(index);
}

const LyricLine& LyricParser::getLine(int index) const {
    static const LyricLine EMPTY_LINE;
    return (index >= 0 && index < m_lines.size()) ? m_lines[index] : EMPTY_LINE;
}

LyricsList LyricParser::getContextLines(int centerIndex, int linesBefore, int linesAfter) const {
    LyricsList context;
    if (centerIndex < 0 || centerIndex >= m_lines.size()) return context;

    int start = qMax(0, centerIndex - linesBefore);
    int end = qMin(m_lines.size() - 1, centerIndex + linesAfter);

    context.reserve(end - start + 1);
    for (int i = start; i <= end; ++i) {
        context.append(m_lines[i]);
    }

    return context;
}

bool LyricParser::hasEnhancedContent() const {
    for (const LyricLine& line : m_lines) {
        if (line.hasText(ROMANIZATION) || line.hasText(TRANSLATION) || line.hasText(KANA)) {
            return true;
        }
    }
    return false;
}

QString LyricParser::guessLyricPath(const QString& audioFilePath) {
    QFileInfo audioFile(audioFilePath);
    if (!audioFile.exists()) return "";

    QString baseName = audioFile.completeBaseName();
    QDir dir = audioFile.dir();

    QStringList extensions = {".lrc", ".txt", ".lyric", ".krc"};
    for (const QString& ext : extensions) {
        QString path = dir.filePath(baseName + ext);
        if (QFile::exists(path)) return path;
        path = dir.filePath(baseName + ".ja" + ext);
        if (QFile::exists(path)) return path;
    }

    return "";
}

QMap<qint64, QString> LyricParser::toLegacyMap() const {
    QMap<qint64, QString> map;
    for (const LyricLine& line : m_lines) {
        map.insert(line.timeMs, line.getText(ORIGINAL));
    }
    return map;
}

// ========== 线程池封装类实现 ==========
LyricParserThreadPool::LyricParserThreadPool(QObject* parent) : QObject(parent) {
    m_threadPool = QThreadPool::globalInstance();
    m_threadPool->setMaxThreadCount(4); // 默认线程数
}

LyricParserThreadPool::~LyricParserThreadPool() {
    m_threadPool->waitForDone(); // 等待所有任务完成
}

void LyricParserThreadPool::asyncLoadFromFile(const QString& filePath) {
    QMutexLocker locker(&m_taskMutex);
    m_threadPool->start(new ParseTask(ParseTask::LoadFile, filePath, this));
}

void LyricParserThreadPool::asyncLoadFromContent(const QString& content) {
    QMutexLocker locker(&m_taskMutex);
    m_threadPool->start(new ParseTask(ParseTask::LoadContent, content, this));
}

// ========== 解析任务实现 ==========
LyricParserThreadPool::ParseTask::ParseTask(TaskType type, const QString& data, LyricParserThreadPool* pool)
    : m_type(type), m_data(data), m_pool(pool) {
    setAutoDelete(true); // 任务完成后自动销毁
}

void LyricParserThreadPool::ParseTask::run() {
    LyricParser parser;
    QString errorMsg;
    bool success = false;
    LyricsList lines;

    try {
        if (m_type == LoadFile) {
            success = parser.loadFromFile(m_data);
            if (!success) {
                errorMsg = QString("加载文件失败: %1").arg(m_data);
            }
        } else {
            success = parser.loadFromContent(m_data);
            if (!success) {
                errorMsg = "解析歌词内容失败";
            }
        }
        lines = parser.getLines();
    } catch (const std::exception& e) {
        success = false;
        errorMsg = QString("解析异常: %1").arg(e.what());
    } catch (...) {
        success = false;
        errorMsg = "未知解析异常";
    }

    // 跨线程发送结果信号（Qt::QueuedConnection 保证主线程接收）
    QMetaObject::invokeMethod(m_pool, [=]() {
        emit m_pool->parseFinished(success, lines, errorMsg);
        emit m_pool->loaded(success);
    }, Qt::QueuedConnection);
}