//
// Created by lsy on 2025/11/28.
//

#include "../Global/Global.h"
#include "../Header/AudioPart/AudioMetaParser.h"



AudioUtil::AudioMeta AudioUtil::SongMeta::toAudioMeta(bool reloadFromFile) const
{
    if (reloadFromFile && !filePath.isEmpty()) {
        AudioMetaParser* parser = AudioMetaParser::getInstance();
        AudioMeta newMeta;
        QString error;
        if (parser->parse(filePath, newMeta, error)) {
            return newMeta;
        } else {
            qWarning() << "Failed to parse audio file:" << error;
            return constructAudioMetaFromCurrentData();
        }
    } else {
        return constructAudioMetaFromCurrentData();
    }
}

AudioUtil::AudioMeta AudioUtil::SongMeta::constructAudioMetaFromCurrentData() const
{
    AudioMeta audioMeta;
    // ... 实现构造逻辑 ...
    audioMeta.filePath = QString("NULL");
    audioMeta.id = QString("NULL");
    return audioMeta;
}

int AudioUtil::SongMeta::parseDurationString(const QString& durationStr) {
    if (durationStr.isEmpty()) return 0;

    QString str = durationStr.trimmed();

    // 处理简单情况
    if (str == "0:00") return 0;

    // 分割字符串
    QStringList parts = str.split(':');
    if (parts.size() != 2) {
        // 尝试其他格式
        parts = str.split(':');
        if (parts.size() > 2) {
            // 可能是"时:分:秒"格式，但歌曲通常不会超过几小时，所以取最后两部分
            // 例如"1:23:45" -> ["1", "23", "45"]，我们取最后两个
            int lastIndex = parts.size() - 1;
            int minutes = parts[lastIndex - 1].toInt();
            int seconds = parts[lastIndex].toInt();
            return (minutes * 60 + seconds) * 1000;
        }
        return 0;
    }

    // 直接解析分钟和秒
    int minutes = parts[0].toInt();
    int seconds = parts[1].toInt();

    // 简单的有效性检查
    if (minutes < 0 || seconds < 0 || seconds >= 60) {
        return 0;
    }

    return (minutes * 60 + seconds) * 1000;
}


QPixmap AudioUtil::AudioMeta::parseMeta() {
    AudioMetaParser* parser = AudioMetaParser::getInstance();
    if (parser) {
        return parser->parseAudioFile(filePath, *this);
    }
    return QPixmap();
}