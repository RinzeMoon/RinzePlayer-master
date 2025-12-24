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

// ================= 构造函数 =================
LyricParser::LyricParser(QObject* parent) : QObject(parent), m_lastQueryIndex(0) {}

// ================= 公开加载方法 =================
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
    // stream.setCodec("UTF-8");  // 强制UTF-8，避免中文乱码
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

// ================= 核心解析逻辑 =================
bool LyricParser::parseContent(const QString& content) {
    QStringList lines = content.split('\n', Qt::SkipEmptyParts);

    // 预分配内存（避免多次重分配）
    m_lines.reserve(lines.size());

    for (const QString& rawLine : lines) {
        parseSingleLine(rawLine.trimmed());
    }

    // 按时间排序（必需，二分查找的前提）
    std::sort(m_lines.begin(), m_lines.end());

    // 合并同一时间点的多语言行（提升查询效率）
    QVector<LyricLine> merged;
    for (int i = 0; i < m_lines.size(); ++i) {
        if (merged.isEmpty() || merged.last().timeMs != m_lines[i].timeMs) {
            merged.append(m_lines[i]);
        } else {
            // 合并相同时间点的多语言文本
            for (const QString& type : m_lines[i].texts.keys()) {
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
        R"(\[(\d{1,}):(\d{2})(?:\.(\d{1,3}))?\])"  // 核心正则
    );

    QRegularExpressionMatchIterator timeIter = timeRegex.globalMatch(line);
    QVector<qint64> timePoints;
    int lastPos = 0;

    // 1. 提取所有时间标签
    while (timeIter.hasNext()) {
        QRegularExpressionMatch match = timeIter.next();
        qint64 ms = parseTimeTag(match.captured(0));  // 完整标签
        timePoints.append(ms);
        lastPos = match.capturedEnd();  // 最后一个标签结束位置
    }

    if (timePoints.isEmpty()) {
        return;  // 无时间标签，可能是元数据行
    }

    // 2. 提取标签后的内容
    QString content = line.mid(lastPos).trimmed();
    if (content.isEmpty()) return;

    // 3. 解析多语言标签：<romaji>文本</romaji>
    static QRegularExpression tagRegex(
        R"(<(\w+)>(.*?)</\1>)",                     // 匹配标签
        QRegularExpression::CaseInsensitiveOption |  // 不区分大小写
        QRegularExpression::DotMatchesEverythingOption  // .匹配换行
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
        parsedTexts.insert(LyricTextType::ORIGINAL, remaining);
    }

    // 5. 为每个时间点创建歌词行
    for (qint64 timeMs : timePoints) {
        if (!parsedTexts.isEmpty()) {
            LyricLine lyric;
            lyric.timeMs = timeMs;
            lyric.texts = parsedTexts;
            m_lines.append(lyric);
        }
    }
}

// ================= 时间标签解析 =================
qint64 LyricParser::parseTimeTag(const QString& timeTag) {
    // 输入格式：[mm:ss.xxx] 或 [mm:ss]
    static QRegularExpression regex(R"(\[(\d+):(\d{2})(?:\.(\d{1,3}))?\])");
    QRegularExpressionMatch match = regex.match(timeTag);

    if (!match.hasMatch()) return 0;

    qint64 minutes = match.captured(1).toLongLong();
    qint64 seconds = match.captured(2).toLongLong();
    QString msStr = match.captured(3);

    qint64 milliseconds = 0;
    if (!msStr.isEmpty()) {
        milliseconds = msStr.toLongLong();
        // 处理2位或3位毫秒
        if (msStr.length() == 1) milliseconds *= 100;
        else if (msStr.length() == 2) milliseconds *= 10;
        // 3位已经是毫秒，无需调整
    }

    return minutes * 60000 + seconds * 1000 + milliseconds;
}

// ================= 标签类型映射 =================
QString LyricParser::mapTagToType(const QString& tag) {
    static const QHash<QString, QString> tagMap = {
        // 罗马音/拼音
        {"romaji", LyricTextType::ROMANIZATION},
        {"romanization", LyricTextType::ROMANIZATION},
        {"rmj", LyricTextType::ROMANIZATION},
        {"pinyin", LyricTextType::ROMANIZATION},
        {"py", LyricTextType::ROMANIZATION},

        // 翻译
        {"trans", LyricTextType::TRANSLATION},
        {"translation", LyricTextType::TRANSLATION},
        {"zh", LyricTextType::TRANSLATION},
        {"cn", LyricTextType::TRANSLATION},
        {"en", LyricTextType::TRANSLATION},

        // 可扩展：假名、注音等
        // {"kana", LyricTextType::KANA},
        // {"furigana", LyricTextType::KANA},
    };

    return tagMap.value(tag.toLower(), "");
}

// ================= 时间查询（核心算法）=================
int LyricParser::findIndexAtTime(qint64 currentMs) const {
    if (m_lines.isEmpty()) return -1;

    // 优化：检查上次查询的附近（针对连续播放）
    if (m_lastQueryIndex < m_lines.size()) {
        // 检查当前行是否仍然有效
        if (m_lastQueryIndex > 0 &&
            m_lines[m_lastQueryIndex - 1].timeMs <= currentMs &&
            (m_lastQueryIndex == m_lines.size() - 1 ||
             m_lines[m_lastQueryIndex].timeMs > currentMs)) {
            return m_lastQueryIndex - 1;
        }

        // 检查下一行
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

    // right指向 currentMs 所在或刚过的歌词行
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

// ================= 上下文获取 =================
QVector<LyricLine> LyricParser::getContextLines(int centerIndex,
                                                int linesBefore,
                                                int linesAfter) const {
    QVector<LyricLine> context;
    if (centerIndex < 0 || centerIndex >= m_lines.size()) return context;

    int start = qMax(0, centerIndex - linesBefore);
    int end = qMin(m_lines.size() - 1, centerIndex + linesAfter);

    context.reserve(end - start + 1);
    for (int i = start; i <= end; ++i) {
        context.append(m_lines[i]);
    }

    return context;
}

// ================= 辅助工具方法 =================
bool LyricParser::hasEnhancedContent() const {
    for (const LyricLine& line : m_lines) {
        if (line.hasText(LyricTextType::ROMANIZATION) ||
            line.hasText(LyricTextType::TRANSLATION)) {
            return true;
        }
    }
    return false;
}

QString LyricParser::guessLyricPath(const QString& audioFilePath) {
    QFileInfo audioFile(audioFilePath);
    if (!audioFile.exists()) return "";

    QString baseName = audioFile.completeBaseName(); // 无扩展名
    QDir dir = audioFile.dir();

    // 常见歌词扩展名（按优先级）
    QStringList extensions = {".lrc", ".txt", ".lyric", ".krc"};
    for (const QString& ext : extensions) {
        QString path = dir.filePath(baseName + ext);
        if (QFile::exists(path)) return path;

        // 也尝试带语言后缀的（如 song.ja.lrc）
        path = dir.filePath(baseName + ".ja" + ext);
        if (QFile::exists(path)) return path;
    }

    return "";
}

QMap<qint64, QString> LyricParser::toLegacyMap() const {
    QMap<qint64, QString> map;
    for (const LyricLine& line : m_lines) {
        map.insert(line.timeMs, line.getText(LyricTextType::ORIGINAL));
    }
    return map;
}
