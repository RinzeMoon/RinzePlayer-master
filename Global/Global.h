//
// Created by lsy on 2025/9/22.
//
#pragma once

#include <QString>
#include <QMap>
#include <QUrl>
#include <QPixmap>
#include <QFileInfo>
#include <QDir>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QStandardPaths>

namespace AudioMetaParserHelper {
    class AudioMetaParser;
    AudioMetaParser* getAudioMetaParser();
    QString parseAudioFile(const QString& filePath, struct AudioMeta& meta);
}

extern "C"
{
#include <libavutil/samplefmt.h>
#include <libavutil/avutil.h>
}

namespace RinGlobal
{
    // 播放状态
    enum class PlayState
    {
        Playing,
        Stopped,
        Paused,
        Error
    };

    enum PlayMode {
        Sequential,   // 顺序播放
        LoopSingle,   // 单曲循环
        Random        // 随机播放
    };


    enum VideoPlayerMode {
        LOCAL_FILE,     // 本地文件播放
        HTTP_STREAM,    // HTTP点播流
        RTMP_STREAM     // RTMP直播流
    };

    enum class AVDataType {
        AUDIO_FRAME,
        VIDEO_FRAME,
        PACKET,           // AVPacket（编码数据）
        METADATA,
        END_OF_STREAM     // 流结束标志
    };

    // 播放列表项数据结构
    struct PlaylistItem {
        QString _id;
        QString title;
        QString sourceURL;
        VideoPlayerMode mode;
        QString duration;
        bool isActive;
    };

    struct SourceInfo {
        QString _id;
        VideoPlayerMode mode;
        QString url;                    // 规范化后的URL
        QString nativePath;             // 平台原生路径（仅对本地文件）
        QString fileName;               // 文件名
        QString fileExtension;          // 文件扩展名
        qint64 fileSize = -1;           // 文件大小（仅对本地文件）
        bool isValid = false;
    };

    enum class TranState {
        IDLE,
        INITIALIZING,
        CONNECTING,
        FETCHING,
        PAUSED,
        STOPPED,
        ERROR
    };
}

namespace RinColor
{
    // 颜色常量定义（核心：红色分层+灰白阶调）
    const QString COLOR_RED_PRIMARY = "#e63946";    // 主红色（鲜亮，用于核心交互）
    const QString COLOR_RED_DARK = "#c1121f";      // 深红色（hover/选中态，增强层次）
    const QString COLOR_RED_LIGHT = "#f8edeb";     // 浅红色（背景/次要区域，柔和过渡）
    const QString COLOR_WHITE = "#ffffff";         // 纯白色（主内容区，清爽基底）
    const QString COLOR_GRAY_LIGHT = "#f9f9f9";    // 浅灰白（顶部栏/控制栏，轻微区分）
    const QString COLOR_GRAY_MID = "#eeeeee";      // 中灰白（分割线/未激活状态）
    const QString COLOR_GRAY_TEXT = "#666666";     // 文字灰（普通文本）
    const QString COLOR_BLACK_TEXT = "#333333";    // 深黑灰（强调文本）

    const QString PRIMARY_COLOR = "#4361EE";        // 主色：蓝
    const QString PRIMARY_LIGHT = "#C7D2FE";        // 主色浅版
    const QString ACCENT_COLOR = "#4CC9F0";         // 强调色：亮蓝
    const QString CARD_BG = "#FFFFFF";              // 卡片背景
    const QString PAGE_BG_START = "#FAFAFC";        // 页面背景渐变起点
    const QString PAGE_BG_END = "#F3F4F6";          // 页面背景渐变终点
    const QString TEXT_MAIN = "#1A1A2E";            // 主文字色
    const QString TEXT_LIGHT = "#6B7280";           // 次要文字色
    const QString BORDER_NORMAL = "#E5E7EB";        // 正常边框色
    const QString BORDER_PLACEHOLDER = "#D1D5DB";   // 占位边框色
    const QString SHADOW_COLOR = "rgba(67, 97, 238, 0.08)"; // 卡片阴影色
}

namespace AudioUtil
{
 /**
 * @brief 音频文件的完整信息（元数据+格式信息）
 */
    struct AudioMeta {
        // -------------------------- 元数据（用户可见） --------------------------
        AudioMeta() = default;
        explicit AudioMeta(const QString& path) : filePath(QFileInfo(path).absoluteFilePath()) {
            id = generateCrossPlatformId(filePath);
            cover = parseMeta();
        }

        QString id;            // 哈希化文件路径产生的id，确保歌曲一致性
        QString filePath;      // 文件绝对路径
        QString title;         // 标题（优先从标签取，无则用文件名）
        QString artist;        // 歌手
        QString album;         // 专辑
        QString genre;         // 流派
        QString year;          // 发行年份
        QString comment;       // 备注
        QPixmap cover = QPixmap();
        QString LrcUrl;
        int track = 0;         // 轨道号（如专辑中的第几首）

        // -------------------------- 格式信息（技术参数） --------------------------
        QString format;        // 容器格式（如MP3、FLAC、MKV）
        QString codec;         // 音频编码（如MPEG-1 Layer 3、FLAC、AAC）
        int durationMs = 0;    // 时长（毫秒）
        int sampleRate = 0;    // 采样率（Hz，如44100）
        int channels = 0;      // 声道数（如2=立体声）
        int bitRate = 0;       // 比特率（kbps，如320）
        AVSampleFormat sampleFmt; // 采样格式（FFmpeg枚举，如AV_SAMPLE_FMT_S16）

        // 辅助函数：将时长转换为"分:秒"格式（如125秒 → "2:05"）
        QString durationString() const {
            if (durationMs <= 0) return "0:00";
            int totalSeconds = durationMs / 1000;
            int minutes = totalSeconds / 60;
            int seconds = totalSeconds % 60;
            return QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
        }

        // 若filePath变更，需重新生成id（提供接口）
        void setFilePath(const QString& newPath) {
            filePath = QFileInfo(newPath).absoluteFilePath();
        }

        static QString generateCrossPlatformId(const QString& filePath) {
            if (filePath.isEmpty() || !QFile::exists(filePath)) {
                return QUuid::createUuid().toString().replace("-", "");
            }

            QFileInfo fileInfo(filePath);
            // 组合元数据：文件名（无扩展名）+ 文件大小 + 最后修改时间（毫秒级）
            QString metaStr = QString("%1_%2_%3")
                .arg(fileInfo.completeBaseName()) // 无扩展名的文件名（跨平台兼容）
                .arg(fileInfo.size())             // 文件大小（字节，跨平台一致）
                .arg(fileInfo.lastModified().toMSecsSinceEpoch()); // 最后修改时间（毫秒级时间戳，跨平台一致）

            // 对组合字符串生成MD5哈希，得到固定长度的id
            QCryptographicHash hash(QCryptographicHash::Md5);
            hash.addData(metaStr.toUtf8());
            return hash.result().toHex();
        }

        QPixmap parseMeta();
    };

    /**
     * @brief 音频文件的解码结构体
     */
    struct AudioFrame {
        // -------------------------- 原始音频数据（核心） --------------------------
        uint8_t** data = nullptr;  // 音频采样数据（多声道时，每个声道单独占一个指针，如立体声有2个指针）
        int linesize = 0;          // 每个声道的数据长度（字节数，FFmpeg解码后会填充这个值）
        int nb_samples = 0;        // 该帧包含的采样点数（如1024个采样，配合采样率可计算该帧的播放时长）

        // -------------------------- 音频参数（与AudioMeta对应，确保播放兼容） --------------------------
        int sample_rate = 0;       // 采样率（需与AudioMeta.sampleRate一致）
        int channels = 0;          // 声道数（需与AudioMeta.channels一致）
        AVSampleFormat sample_fmt = AV_SAMPLE_FMT_NONE;  // 采样格式（需与AudioMeta.sampleFmt一致，或重采样后的格式）

        // -------------------------- 辅助信息（可选，用于进度同步等） --------------------------
        int64_t pts = AV_NOPTS_VALUE;  // 该帧的时间戳（单位：根据音频流的time_base，用于进度计算）
    };

    struct SongMeta {
    // -------------------------- 核心标识（与AudioMeta关联） --------------------------
    QString id;         // 与AudioMeta的id完全一致（基于文件路径哈希）
    QString filePath;
    // -------------------------- 展示层信息（用户可见） --------------------------
    QString name;       // 歌曲名（优先显示标签中的title，无则用文件名）
    QString singer;     // 歌手名
    QString album;      // 专辑名
    QString genre;      // 流派（可选，用户可见的分类）
    QString duration;   // 格式化时长（分:秒，如"3:45"）
    QString coverUrl;   // 封面图片URL（本地路径或临时文件，用于UI显示）
    QString LrcUrl;

    //AudioMetaParser s_parser;
    mutable AudioUtil::AudioMeta m_originalAudioMeta;

    // -------------------------- 构造与转换（从AudioMeta生成） --------------------------
    // 默认构造
    SongMeta() = default;

    // 从AudioMeta转换（核心：同步id和展示信息）
    explicit SongMeta(const AudioMeta& audioMeta) {
        fromAudioMeta(audioMeta);
        m_originalAudioMeta = audioMeta;
    }

    // 从AudioMeta同步数据（确保id和展示信息一致）
    void fromAudioMeta(const AudioMeta& audioMeta) {
        // 1. 同步唯一标识（与AudioMeta的id严格一致）
        id = audioMeta.id;
        m_originalAudioMeta = audioMeta;


        // 2. 同步展示信息（用户可见字段）
        name = audioMeta.title.isEmpty() ? getFileNameFromPath(audioMeta.filePath) : audioMeta.title;
        singer = audioMeta.artist.isEmpty() ? "未知歌手" : audioMeta.artist;
        album = audioMeta.album.isEmpty() ? "未知专辑" : audioMeta.album;
        genre = audioMeta.genre.isEmpty() ? "未知流派" : audioMeta.genre;
        duration = audioMeta.durationString(); // 直接复用AudioMeta的格式化时长
        filePath = audioMeta.filePath;
        LrcUrl = audioMeta.LrcUrl;

        // 3. 处理封面URL（从AudioMeta的cover生成）
        setCoverFromAudioMeta(audioMeta.cover);
    }

    const AudioUtil::AudioMeta& getOriginalAudioMeta() const
    {
        return  m_originalAudioMeta;
    }

    // -------------------------- 辅助函数 --------------------------
    // 从文件路径提取文件名（无扩展名）作为默认歌名
    static QString getFileNameFromPath(const QString& filePath) {
        if (filePath.isEmpty()) return "未知歌曲";
        QString fileName = QFileInfo(filePath).fileName();
        int dotIndex = fileName.lastIndexOf('.');
        return (dotIndex != -1) ? fileName.left(dotIndex) : fileName;
    }

    // 从AudioMeta的cover图像生成coverUrl（用于UI显示）
        void setCoverFromAudioMeta(const QPixmap& cover) {
        if (cover.isNull()) {
            // 跨平台默认封面：使用Qt资源文件（qrc路径跨平台一致）
            coverUrl = QString("qrc:/images/default_cover.png");
            return;
        }

        // 【跨平台优化】使用Qt的标准路径：程序专属缓存目录（各平台自动适配）
        // QStandardPaths::CacheLocation：
        // - Windows: C:\Users\<用户名>\AppData\Local\<程序名>\cache
        // - Linux: ~/.cache/<程序名>
        // - macOS: ~/Library/Caches/<程序名>
        QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/MusicCovers";
        // 确保目录存在（跨平台的目录创建）
        QDir().mkpath(cacheDir);

        // 用跨平台的id生成封面文件名（此时id已不为空）
        QString tempCoverPath = QString("%1/%2.jpg").arg(cacheDir).arg(id);

        if (cover.save(tempCoverPath)) {
            coverUrl = tempCoverPath; // 跨平台的有效路径
        } else {
            // 兜底：仍使用资源文件中的默认封面
            coverUrl = QString("qrc:/images/default_cover.png");
        }
    }

        QJsonObject toJson() const
    {
        QJsonObject obj;
        // 1. id：空值时用UUID兜底（跨平台唯一，确保永远不为空）
        obj["id"] = id.isEmpty() ? QUuid::createUuid().toString().replace("-", "") : id;
        // 2. 展示字段：空值时用默认文字兜底（提升用户体验）
        obj["name"] = name.isEmpty() ? name : "未知歌曲";
        obj["singer"] = singer.isEmpty() ? "未知歌手" : singer;
        obj["duration"] = duration.isEmpty() ? "0:00" : duration;
        obj["album"] = album.isEmpty() ? "未知专辑" : album;
        obj["genre"] = genre.isEmpty() ? "未知流派" : genre;
        // 3. 文件路径：保留原路径（后续解析时依赖此路径创建AudioMeta）
        obj["path"] = filePath;
        // 4. coverUrl：过滤无效路径，兜底为默认封面（资源路径跨平台一致）
        QString validCoverUrl = coverUrl;
        // 过滤条件：空值、包含"/.jpg"（文件名空）、文件不存在
        if (validCoverUrl.isEmpty() || validCoverUrl.contains("/.jpg") || !QFile::exists(validCoverUrl)) {
            validCoverUrl = "qrc:/images/default_cover.png"; // 替换为你的默认封面资源路径
        }
        obj["coverUrl"] = validCoverUrl;

        return obj;
    }

        // 从 JSON 对象解析
        static SongMeta fromJson(const QJsonObject& obj)
    {
        SongMeta meta;
        meta.id = obj["id"].toString();
        // 兼容两种字段名：优先使用新字段名，如果不存在则使用旧字段名
        meta.name = obj["name"].toString();
        meta.singer = obj["singer"].toString();
        meta.duration = obj["duration"].toString();
        meta.album = obj["album"].toString();
        meta.genre = obj["genre"].toString();
        meta.filePath = obj["path"].toString();
        meta.coverUrl = obj["coverUrl"].toString();

        return meta;
    }

        /**
         * @brief 将SongMeta转换回AudioMeta
         * @param reloadFromFile 是否从文件重新加载元数据（true=从文件重新解析，false=使用当前数据）
         * @return 转换后的AudioMeta对象
         */
        AudioMeta toAudioMeta(bool reloadFromFile = true) const;
        AudioMeta constructAudioMetaFromCurrentData() const;
        static int parseDurationString(const QString& durationStr);
};
    struct MusicSheet {
        QString id;           // 歌单唯一标识（用于Manager查询）
        QString tag;          // 标签（如"歌单"）
        QString title;        // 标题
        int songCount;        // 歌曲数量
        int playCount;        // 播放量
        QString desc;         // 描述
        QPixmap cover;        // 封面图
        QString coverPath;   // 封面图片的文件路径（如 ./images/MusicCover/长征.bmp""）
        QList<SongMeta> songs;// 歌曲列表

        QJsonObject toJson() const {
            QJsonObject obj;
            obj["id"] = id;
            obj["tag"] = tag;
            obj["title"] = title;
            obj["songCount"] = songCount;
            obj["playCount"] = playCount;
            obj["desc"] = desc;

            // 存储封面图片的文件路径
            obj["coverPath"] = coverPath;  // 直接存路径字符串

            // 歌曲列表转 JSON 数组
            QJsonArray songsArray;
            for (const auto& song : songs) {
                songsArray.append(song.toJson());
            }
            obj["songs"] = songsArray;

            return obj;
        }
    };

    // 从 JSON 解析（通过路径加载图片）
    static MusicSheet fromJson(const QJsonObject& obj) {
        MusicSheet sheet;
        sheet.id = obj["id"].toString();
        sheet.tag = obj["tag"].toString();
        sheet.title = obj["title"].toString();
        sheet.songCount = obj["songCount"].toInt();
        sheet.playCount = obj["playCount"].toInt();
        sheet.desc = obj["desc"].toString();

        // 从 JSON 读取路径，然后加载图片
        sheet.coverPath = obj["coverPath"].toString();
        if (!sheet.coverPath.isEmpty()) {
            sheet.cover.load(sheet.coverPath);
        }

        // 解析歌曲列表
        QJsonArray songsArray = obj["songs"].toArray();
        for (const auto& songJson : songsArray) {
            // 第一步：从JSON解析SongMeta
            SongMeta songMeta = SongMeta::fromJson(songJson.toObject());

            // 第二步：修正文件路径（从 "../music/" 改为 "../../music/"）
            /*
            if (songMeta.filePath.startsWith("../music/")) {
                // 将 "../music/" 替换为 "../../music/"
                QString correctedPath = "../../" + songMeta.filePath.mid(3); // 跳过 "../"
                songMeta.filePath = correctedPath;
                qDebug() << "修正文件路径: " << songMeta.filePath << " -> " << correctedPath;
            }
            */

            // 第三步：尝试使用AudioMeta读取元数据（可选）
            if (!songMeta.filePath.isEmpty()) {
                QFileInfo fileInfo(songMeta.filePath);

                // 检查文件是否存在
                if (fileInfo.exists()) {
                    qDebug() << "音频文件存在:" << fileInfo.absoluteFilePath();

                    try {
                        AudioUtil::AudioMeta audioMeta(songMeta.filePath);

                        // 使用音频文件的ID
                        if (!audioMeta.id.isEmpty()) {
                            songMeta.id = audioMeta.id;
                        }

                    } catch (const std::exception& e) {
                        qWarning() << "读取音频文件元数据失败:" << e.what();
                    }
                } else {
                    qWarning() << "音频文件不存在:" << songMeta.filePath;
                    qDebug() << "绝对路径:" << fileInfo.absoluteFilePath();
                }
            }

            // 第四步：确保关键字段不为空
            if (songMeta.id.isEmpty()) {
                songMeta.id = QUuid::createUuid().toString().replace("-", "");
            }

            if (songMeta.coverUrl.isEmpty()) {
                songMeta.coverUrl = QString("qrc:/images/default_cover.png");
            }

            sheet.songs.append(songMeta);
        }

        return sheet;
    }
};

namespace CoverUtil
{
    // BMP文件头（14字节）
#pragma pack(push, 1)
    // BMP文件头（14字节）
    struct BMPFileHeader {
        uint16_t bfType = 0x4D42;  // 固定为"BM"，不依赖外部变量
        uint32_t bfSize;           // 总大小（后续在函数中赋值）
        uint16_t bfReserved1 = 0;  // 固定为0
        uint16_t bfReserved2 = 0;  // 固定为0
        uint32_t bfOffBits = 54;   // 固定为54（14+40），不依赖外部变量
    };

    // BMP信息头（40字节）
    struct BMPInfoHeader {
        uint32_t biSize = 40;               // 固定为40，不依赖外部变量
        int32_t  biWidth;                   // 宽度（后续赋值）
        int32_t  biHeight;                  // 高度（后续赋值）
        uint16_t biPlanes = 1;              // 固定为1
        uint16_t biBitCount = 24;           // 固定为24，不依赖外部变量
        uint32_t biCompression = 0;         // 固定为0
        uint32_t biSizeImage;               // 像素数据大小（后续赋值）
        int32_t  biXPelsPerMeter = 2835;    // 72dpi，固定
        int32_t  biYPelsPerMeter = 2835;    // 72dpi，固定
        uint32_t biClrUsed = 0;             // 固定为0
        uint32_t biClrImportant = 0;        // 固定为0
    };
#pragma pack(pop)
}

namespace VideoUtil {
    struct VideoFrame {
        uint8_t* data = nullptr;      // 视频像素数据（单平面或平面数组）
        uint8_t* data_array[4] = {nullptr};  // 多平面数据
        int linesize[4] = {0};        // 每个平面的行大小
        int width = 0;                // 宽度
        int height = 0;               // 高度
        AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;  // 像素格式
        int64_t pts = AV_NOPTS_VALUE; // 时间戳
        bool key_frame = false;       // 是否为关键帧

        // 获取数据指针
        uint8_t** getData() {
            return data_array[0] ? data_array : &data;
        }
    };
}

namespace LyricTextType {
    const QString ORIGINAL     = "original";     // 原文 (如日语、中文)
    const QString TRANSLATION  = "translation";  // 翻译 (如中文翻译)
    const QString ROMANIZATION = "romanization"; // 罗马音/拼音
    const QString KANA         = "kana";         // 假名 (用于日语)
    // 你可以随时添加:
    // const QString ROMANJI     = "romanji";
    // const QString PINYIN      = "pinyin";
    // const QString CYRILLIC    = "cyrillic";
}

namespace LrcLine
{
    struct LyricLine {
        qint64 timeMs = 0;  // 时间戳（毫秒）

        // 存储多语言歌词文本，key为LyricTextType::XXX，value为对应文本
        QHash<QString, QString> texts;

        // 便捷访问方法
        QString getText(const QString& type = LyricTextType::ORIGINAL) const {
            return texts.value(type, "");
        }

        bool hasText(const QString& type) const {
            return texts.contains(type);
        }

        void setText(const QString& type, const QString& content) {
            if (!content.isEmpty()) {
                texts.insert(type, content);
            }
        }

        QStringList availableTypes() const {
            return texts.keys();
        }

        bool isEmpty() const {
            return texts.isEmpty();
        }

        // ============= 完整比较运算符组（基于timeMs） =============
        // 注意：所有运算符都声明为const成员函数

        // 小于 <
        bool operator<(const LyricLine& other) const {
            return timeMs < other.timeMs;
        }

        // 大于 >
        bool operator>(const LyricLine& other) const {
            return timeMs > other.timeMs;
        }

        // 小于等于 <=
        bool operator<=(const LyricLine& other) const {
            return timeMs <= other.timeMs;
        }

        // 大于等于 >=
        bool operator>=(const LyricLine& other) const {
            return timeMs >= other.timeMs;
        }

        // 等于 == （注意：这里只比较时间，不比较文本内容）
        bool operator==(const LyricLine& other) const {
            return timeMs == other.timeMs;
        }

        // 不等于 !=
        bool operator!=(const LyricLine& other) const {
            return timeMs != other.timeMs;
        }

        // ============= 流输出运算符（用于调试） =============
        // 这不是比较运算符，但通常很有用
        friend QDebug operator<<(QDebug debug, const LyricLine& line) {
            QDebugStateSaver saver(debug);
            debug.nospace() << "LyricLine{time=" << line.timeMs
                           << "ms, texts=" << line.texts << "}";
            return debug;
        }
    };

    using LyricsList = QVector<LrcLine::LyricLine>;
}
Q_DECLARE_METATYPE(LrcLine::LyricLine)

namespace idGen
{
    /**
    * @brief 根据文件路径生成4位数字字母混合的ID
    * @param filePath 文件完整路径
    * @return 4位数字字母混合的ID（小写字母+数字）
    */
    static QString generateFileId(const QString& filePath) {
        // 1. 处理空路径/无效路径
        if (filePath.isEmpty()) {
            return "0000"; // 空路径默认ID
        }

        // 2. 对文件路径做MD5哈希（统一转UTF-8，避免编码问题）
        QByteArray hashData = QCryptographicHash::hash(
            filePath.toUtf8(),
            QCryptographicHash::Md5
        );

        // 3. 哈希值转十六进制字符串（包含0-9、a-f）
        QString hexStr = hashData.toHex();

        // 4. 截取前4位（不足4位补0，超过4位取前4位）
        QString id = hexStr.left(4).rightJustified(4, '0');

        // 5. 可选优化：确保至少包含1个数字+1个字母（避免全数字/全字母）
        bool hasDigit = false;
        bool hasLetter = false;
        for (QChar c : id) {
            if (c.isDigit()) hasDigit = true;
            if (c.isLetter()) hasLetter = true;
        }
        // 若不满足混合要求，替换最后一位为字母/数字
        if (!hasDigit) {
            id.replace(3, 1, '8'); // 最后一位改为数字8
        } else if (!hasLetter) {
            id.replace(3, 1, 'f'); // 最后一位改为字母f
        }

        return id;
    }
}