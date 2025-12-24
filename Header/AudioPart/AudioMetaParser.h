#pragma once

#include <QObject>
#include <QString>
#include <QPixmap>
#include <QDebug>
#include <QFileInfo>
#include <QMutex>
#include <QDir>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

namespace AudioUtil {
    struct AudioMeta;
}

class AudioMetaParser
{
public:
    ~AudioMetaParser();

    // 3. 全局获取实例的接口（线程安全）
    static AudioMetaParser* getInstance();

    // -------------------------- 对外暴露的解析接口 --------------------------
    // 核心接口：解析音频文件，填充AudioMeta数据
    QPixmap parseAudioFile(const QString& filePath, AudioUtil::AudioMeta& meta);

    bool parse(const QString& filePath, AudioUtil::AudioMeta& meta, QString& error);

private:

    explicit AudioMetaParser(QObject *parent = nullptr)
    {
        initFFmpeg();
    }
    // 2. 禁止拷贝和赋值：避免创建多个实例
    AudioMetaParser(const AudioMetaParser&) = delete;
    AudioMetaParser& operator=(const AudioMetaParser&) = delete;
    // 3. 静态实例指针（懒汉式：初始为nullptr）
    static AudioMetaParser* s_instance;
    // 4. 线程安全锁：保证多线程下初始化唯一
    static QMutex s_mutex;


    void initFFmpeg();
    QString errorCodeToString(int errCode);

    void extractMetadata(AVFormatContext* fmtCtx, AudioUtil::AudioMeta& meta);
    void extractFormatInfo(AVFormatContext* fmtCtx, int audioStreamIdx, AudioUtil::AudioMeta& meta);

    bool extractCover(AVFormatContext* fmtCtx, AudioUtil::AudioMeta& meta);
    bool extractEmbeddedCover(AVFormatContext* fmtCtx, AudioUtil::AudioMeta& meta);
    bool extractAPICCover(AVFormatContext* fmtCtx, AudioUtil::AudioMeta& meta);

    bool encodePNG(const QString& outputPath, AVFrame* frame);
    bool saveCoverToFile(AVFrame* frame, const QString& filePath);

    std::string filterFilename(const std::string& filename);
    void ensureCoverDirectory(const QString& dirPath);

    QPixmap loadExistingCover(const QString& filePath);

    static bool s_ffmpegInitialized;
};

