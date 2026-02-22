//
// Created by lsy on 2025/12/10.
//

#ifndef RINZEPLAYER_LYRICPARSER_H
#define RINZEPLAYER_LYRICPARSER_H

#include "../Global/Global.h"
#include <QRunnable>
#include <QMutex>
#include <QThreadPool>

using namespace LyricTextType;
using namespace LrcLine;

class LyricParser : public QObject {
    Q_OBJECT

public:
    explicit LyricParser(QObject* parent = nullptr);

    // ================= 核心加载接口 =================
    bool loadFromFile(const QString& filePath);    // 从文件加载（同步）
    bool loadFromContent(const QString& content);  // 从文本加载（同步）
    void clear();                                  // 清空数据

    // ================= 状态查询 =================
    bool isEmpty() const { return m_lines.isEmpty(); }
    int totalLines() const { return m_lines.size(); }
    bool hasEnhancedContent() const;  // 是否包含罗马音/翻译/假名等

    // ================= 时间查询接口 =================
    int findIndexAtTime(qint64 currentMs) const;               // 二分查找行索引
    LyricLine getLineAtTime(qint64 currentMs) const;           // 获取当前时间歌词行
    const LyricLine& getLine(int index) const;                 // 获取指定索引行
    LyricsList getContextLines(int centerIndex, int linesBefore = 2, int linesAfter = 2) const; // 获取上下文

    // ================= 辅助工具 =================
    static QString guessLyricPath(const QString& audioFilePath);
    QMap<qint64, QString> toLegacyMap() const;

    // ========== 新增：暴露歌词行（供线程池访问） ==========
    LyricsList getLines() const { return m_lines; }

signals:
    void loaded(bool success);          // 同步加载完成信号
    void error(const QString& message); // 错误信号

private:
    // ================= 内部解析方法 =================
    bool parseContent(const QString& content);
    void parseSingleLine(const QString& line);
    static qint64 parseTimeTag(const QString& timeTag);
    static QString mapTagToType(const QString& tag);

    // 数据存储（使用你的 LyricsList 别名）
    LyricsList m_lines;
    mutable int m_lastQueryIndex = 0; // 缓存最后查询的索引
};

// ========== 线程池封装类（异步解析，核心新增） ==========
class LyricParserThreadPool : public QObject {
    Q_OBJECT
public:
    // 单例模式（全局唯一线程池）
    static LyricParserThreadPool* instance(QObject* parent = nullptr) {
        static QMutex mutex;
        if (!m_instance) {
            QMutexLocker locker(&mutex);
            if (!m_instance) {
                m_instance = new LyricParserThreadPool(parent);
            }
        }
        return m_instance;
    }

    // ================= 异步接口（核心） =================
    void asyncLoadFromFile(const QString& filePath);  // 异步从文件加载
    void asyncLoadFromContent(const QString& content);// 异步从文本加载

    // ================= 线程池配置 =================
    void setMaxThreadCount(int count) { m_threadPool->setMaxThreadCount(count); }
    int activeThreadCount() const { return m_threadPool->activeThreadCount(); }

signals:
    // 异步解析完成信号（返回解析结果，使用你的 LyricsList）
    void parseFinished(bool success, const LyricsList& lines, const QString& errorMsg);
    // 兼容原有 loaded 信号
    void loaded(bool success);

private:
    explicit LyricParserThreadPool(QObject* parent = nullptr);
    ~LyricParserThreadPool() override;

    // 解析任务内部类
    class ParseTask : public QRunnable {
    public:
        enum TaskType { LoadFile, LoadContent };
        ParseTask(TaskType type, const QString& data, LyricParserThreadPool* pool);
        void run() override;

    private:
        TaskType m_type;
        QString m_data;
        LyricParserThreadPool* m_pool;
    };

    static LyricParserThreadPool* m_instance;
    QThreadPool* m_threadPool;
    QMutex m_taskMutex;
};

#endif //RINZEPLAYER_LYRICPARSER_H