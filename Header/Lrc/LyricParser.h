//
// Created by lsy on 2025/12/10.
//

#ifndef RINZEPLAYER_LYRICPARSER_H
#define RINZEPLAYER_LYRICPARSER_H

#include "../Global/Global.h"

using namespace LyricTextType;
using LrcLine::LyricLine;

class LyricParser : public QObject {
    Q_OBJECT

public:
    explicit LyricParser(QObject* parent = nullptr);

    // ================= 核心加载接口 =================
    bool loadFromFile(const QString& filePath);    // 从文件加载
    bool loadFromContent(const QString& content);  // 从文本加载
    void clear();                                  // 清空数据

    // ================= 状态查询 =================
    bool isEmpty() const { return m_lines.isEmpty(); }
    int totalLines() const { return m_lines.size(); }
    bool hasEnhancedContent() const;  // 是否包含罗马音/翻译

    // ================= 时间查询接口 =================
    // 方法1: 获取当前时间对应的行索引（使用二分查找，O(log n)）
    int findIndexAtTime(qint64 currentMs) const;

    // 方法2: 直接获取当前时间的歌词行（内部调用findIndexAtTime）
    LyricLine getLineAtTime(qint64 currentMs) const;

    // 方法3: 获取指定索引的歌词行（边界安全）
    const LyricLine& getLine(int index) const;

    // ================= 上下文获取 =================
    // 获取当前行及前后上下文（用于UI显示多行）
    QVector<LyricLine> getContextLines(int centerIndex,
                                       int linesBefore = 2,
                                       int linesAfter = 2) const;

    // ================= 辅助工具 =================
    // 智能猜测歌词文件路径（根据音频文件路径）
    static QString guessLyricPath(const QString& audioFilePath);

    // 转换为传统时间-文本映射（兼容旧格式）
    QMap<qint64, QString> toLegacyMap() const;

    signals:
        void loaded(bool success);          // 加载完成信号
    void error(const QString& message); // 错误信号

private:
    // ================= 内部解析方法 =================
    bool parseContent(const QString& content);
    void parseSingleLine(const QString& line);

    // 时间标签解析（处理[mm:ss.xx]格式）
    static qint64 parseTimeTag(const QString& timeTag);

    // 标签映射（将<romaji>等映射为标准类型）
    static QString mapTagToType(const QString& tag);

    // 数据存储（始终按时间排序）
    QVector<LyricLine> m_lines;

    // 性能优化：缓存最后查询的索引（针对连续播放优化）
    mutable int m_lastQueryIndex = 0;
};

#endif //RINZEPLAYER_LYRICPARSER_H