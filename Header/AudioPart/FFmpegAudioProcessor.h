//
// Created by lsy on 2025/9/29.
//

#ifndef RINZEPLAYER_FFMPEGAUDIOPROCESSOR_H
#define RINZEPLAYER_FFMPEGAUDIOPROCESSOR_H

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include "../../Global/Global.h"

#include <QString>
#include <QImage>

using namespace AudioUtil;

class FFmpegAudioProcessor
{
public:
    FFmpegAudioProcessor();
    ~FFmpegAudioProcessor();

    /**
     * @brief 解析音频文件的完整信息（元数据+格式信息）
     * @param filePath 音频文件路径
     * @param info 输出参数：存储解析结果
     * @param error 输出参数：错误信息
     * @return 是否成功
     */

    bool parseAudioInfo(const QString& filePath, AudioMeta& info, QString& error);

    /**
     * @brief 提取音频文件的封面图片
     * @param filePath 音频文件路径
     * @param cover 输出参数：封面图片（QImage）
     * @param error 输出参数：错误信息
     * @return 是否成功
     */
    bool extractCover(const QString& filePath, QImage& cover, QString& error);

    /**
     * @brief 从视频文件中提取音频流，保存为单独的音频文件
     * @param videoPath 视频文件路径
     * @param outputAudioPath 输出音频文件路径（如"output.mp3"）
     * @param error 输出参数：错误信息
     * @return 是否成功
     */
    bool extractAudioFromVideo(const QString& videoPath, const QString& outputAudioPath, QString& error);
private:
    // 初始化FFmpeg（全局只执行一次）
    void initFFmpeg();

    // 解析元数据（从AVFormatContext的metadata提取到AudioInfo）
    void parseMetadata(AVFormatContext* fmtCtx, AudioMeta& info);

    // 解析格式信息（从音频流提取编码、采样率等）
    void parseFormatInfo(AVFormatContext* fmtCtx, int audioStreamIdx, AudioMeta& info);

    // 将FFmpeg的错误码转换为可读字符串
    QString avErrorToString(int errCode);
};


#endif //RINZEPLAYER_FFMPEGAUDIOPROCESSOR_H