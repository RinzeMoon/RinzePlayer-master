//
// Created by lsy on 2026/1/16.
//

#ifndef RINZEPLAYER_AVDATAFETCHERFACTORY_H
#define RINZEPLAYER_AVDATAFETCHERFACTORY_H

#include "../Global/Global.h"
#include "../VideoPart/AVDataFetch/AVDataFetcher.h"
#include "../Header/VideoPart/AvSourceParse/AVSourceResolver.h"

#include "../Header/VideoPart/AVDataFetch/LocalFileDataFetcher.h"
#include "../Header/VideoPart/AVDataFetch/MediaDataFetcher.h"
using RinGlobal::VideoPlayerMode;
using RinGlobal::SourceInfo;
class AVDataFetcherFactory
{
public:
    static std::unique_ptr<AVDataFetcher> createFetcher(const RinGlobal::SourceInfo& sourceInfo)
    {
        qDebug() << "[Factory::createFetcher(SourceInfo)] === 开始 ===";
        qDebug() << "  模式:" << static_cast<int>(sourceInfo.mode);
        qDebug() << "  是否有效:" << sourceInfo.isValid;
        qDebug() << "  URL:" << sourceInfo.url;
        qDebug() << "  nativePath:" << sourceInfo.nativePath;

        std::unique_ptr<AVDataFetcher> fetcher;
        fetcher = std::make_unique<LocalFileDataFetcher>();

        switch (sourceInfo.mode) {
        case VideoPlayerMode::LOCAL_FILE:
            qDebug() << "[Factory] 创建 LocalFileDataFetcher";
            fetcher = std::make_unique<LocalFileDataFetcher>();
            break;
        case VideoPlayerMode::HTTP_STREAM:
            qDebug() << "[Factory] HTTP_STREAM";
            fetcher = std::make_unique<MediaDataFetcher>();
            break;
        case VideoPlayerMode::RTMP_STREAM:
            qDebug() << "[Factory] RTMP_STREAM";
            fetcher = std::make_unique<MediaDataFetcher>();
            break;
        default:
            qDebug() << "[Factory] 未知模式:" << static_cast<int>(sourceInfo.mode);
            return nullptr;
        }

        if (fetcher) {
            qDebug() << "[Factory] 调用 fetcher->init()，fetcher 地址:" << fetcher.get();
            bool initResult = fetcher->init(sourceInfo);
            qDebug() << "[Factory] init() 返回:" << initResult;

            // 重要：如果初始化失败，返回 nullptr
            if (!initResult) {
                qDebug() << "[Factory] 错误：fetcher 初始化失败，返回 nullptr";
                return nullptr;
            }
            qDebug() << "[Factory] fetcher 初始化成功";
        }

        qDebug() << "[Factory::createFetcher(SourceInfo)] === 结束 ===";

        return fetcher;
    }

    static std::unique_ptr<AVDataFetcher> createFetcher(const QString& sourceUrl)
    {
        qDebug() << "[Factory::createFetcher] 原始URL:" << sourceUrl;

        // 先解析源信息
        auto resolver = AVSourceResolver::resolveSourceInfo(sourceUrl);

        qDebug() << "[Factory::createFetcher] 解析完成:";
        qDebug() << "  mode:" << static_cast<int>(resolver.mode);
        qDebug() << "  url:" << resolver.url;
        qDebug() << "  isValid:" << resolver.isValid;
        qDebug() << "  nativePath:" << resolver.nativePath;
        qDebug() << "  id: " << resolver._id;

        // 创建对应的获取器
        return createFetcher(resolver);
    }
};

#endif //RINZEPLAYER_AVDATAFETCHERFACTORY_H