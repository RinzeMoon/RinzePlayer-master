//
// Created by lsy on 2026/1/16.
//

#ifndef RINZEPLAYER_AVSOURCERESOLVER_H
#define RINZEPLAYER_AVSOURCERESOLVER_H


#include <QString>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QtNetwork/QtNetwork>
#include "../Global/Global.h"

using RinGlobal::AVDataType;
using RinGlobal::VideoPlayerMode;
using RinGlobal::SourceInfo;

struct HTTPStreamInfo {
    QString url;                    // 完整URL
    QString protocol;               // http 或 https
    QString host;                   // 主机名
    int port = 0;                   // 端口
    QString path;                   // 路径部分
    QString query;                  // 查询参数
    QString fragment;               // 片段
    QString fileName;               // 文件名（如果有）
    bool isHLS = false;             // 是否为HLS流（.m3u8）
    bool isDASH = false;            // 是否为DASH流（.mpd）
    QString userAgent;              // 用户代理（可选）

    // 获取标准化URL（确保格式正确）
    QString normalizedUrl() const {
        QUrl qurl(url);
        return qurl.toString(QUrl::FullyEncoded);
    }

    // 判断是否需要SSL
    bool isSecure() const {
        return protocol == "https";
    }

    // 获取主机地址（包含端口）
    QString hostAddress() const {
        if (port > 0 && port != 80 && port != 443) {
            return host + ":" + QString::number(port);
        }
        return host;
    }
};

// RTMP流信息结构
struct RTMPStreamInfo {
    QString url;                    // 完整URL
    QString host;                   // 主机名
    int port = 1935;                // RTMP默认端口
    QString app;                    // 应用名称
    QString stream;                 // 流名称
    QString tcUrl;                  // tcUrl（连接URL）
    QString pageUrl;                // 页面URL（可选）
    QString swfUrl;                 // SWF URL（可选）
    QString flashVer;               // Flash版本（可选）
    bool live = true;               // 是否为直播流

    // 解析RTMP URL：rtmp://host:port/app/stream
    void parseFromUrl(const QString& rtmpUrl) {
        url = rtmpUrl;

        QUrl qurl(rtmpUrl);
        host = qurl.host();
        port = qurl.port(1935); // 默认RTMP端口

        // 解析app和stream
        QString path = qurl.path();
        if (!path.isEmpty() && path.startsWith('/')) {
            path = path.mid(1); // 移除开头的斜杠
        }

        int slashPos = path.indexOf('/');
        if (slashPos != -1) {
            app = path.left(slashPos);
            stream = path.mid(slashPos + 1);
        } else {
            app = path;
            stream = "live"; // 默认流名称
        }

        // 构建tcUrl
        tcUrl = QString("rtmp://%1:%2/%3").arg(host).arg(port).arg(app);
    }

    // 获取播放URL格式
    QString playbackUrl() const {
        return QString("rtmp://%1:%2/%3/%4").arg(host).arg(port).arg(app).arg(stream);
    }
};

struct NetworkInfo {
    QString localIP;                // 本地IP地址
    QString networkInterface;       // 网络接口名称
    int bandwidthEstimate = 0;      // 带宽估计（bps）
    int latency = 0;                // 延迟（ms）

    // 获取本地IP地址
    static QString getLocalIP() {
        QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
        for (int i = 0; i < ipAddressesList.size(); ++i) {
            if (ipAddressesList.at(i) != QHostAddress::LocalHost &&
                ipAddressesList.at(i).toIPv4Address()) {
                return ipAddressesList.at(i).toString();
                }
        }
        return "127.0.0.1";
    }
};

class AVSourceResolver
{
public:
    // 解析资源类型
    static VideoPlayerMode resolveSourceType(const QString& source)
    {
        if (source.isEmpty()) {
            return VideoPlayerMode::LOCAL_FILE;
        }

        QString lowerSource = source.toLower();

        // RTMP流
        if (lowerSource.startsWith("rtmp://")) {
            return VideoPlayerMode::RTMP_STREAM;
        }

        // HTTP/HTTPS流
        if (lowerSource.startsWith("http://") || lowerSource.startsWith("https://")) {
            return VideoPlayerMode::HTTP_STREAM;
        }

        // 文件协议
        if (lowerSource.startsWith("file://")) {
            return VideoPlayerMode::LOCAL_FILE;
        }

        // 其他协议（如RTSP等）可以在这里扩展

        // 默认情况下，认为是本地文件
        return VideoPlayerMode::RTMP_STREAM;
    }

    // 规范化本地文件路径（跨平台处理）
    static QString normalizeLocalFilePath(const QString& filePath)
    {
        if (filePath.isEmpty()) {
            return QString("NULL");
        }

        // 如果已经是file://协议，直接返回
        if (filePath.startsWith("file://")) {
            return filePath;
        }

        // 使用Qt的跨平台方式处理文件路径
        QFileInfo fileInfo(filePath);

        // 如果是相对路径，转换为绝对路径
        QString absolutePath;
        if (fileInfo.isRelative()) {
            // 相对于应用程序目录
            absolutePath = QDir::cleanPath(QCoreApplication::applicationDirPath() + QDir::separator() + filePath);
        } else {
            absolutePath = QDir::cleanPath(filePath);
        }

        // 转换为file:// URL格式
        QUrl fileUrl = QUrl::fromLocalFile(absolutePath);
        return fileUrl.toString().trimmed();
    }

    // 将本地文件路径转换为平台相关的格式（用于显示等）
    static QString toPlatformNativePath(const QString& filePath)
    {
        if (filePath.startsWith("file://")) {
            QUrl url(filePath);
            return url.toLocalFile();
        }
        return filePath;
    }

    // 判断是否是有效的本地文件路径（跨平台验证）
    static bool isValidLocalFile(const QString& filePath)
    {
        QString localPath;

        if (filePath.startsWith("file://")) {
            QUrl url(filePath);
            localPath = url.toLocalFile();
        } else {
            localPath = filePath;
        }

        // 检查文件是否存在
        QFileInfo fileInfo(localPath);
        return fileInfo.exists() && fileInfo.isFile();
    }

    // 获取文件大小（跨平台）
    static qint64 getFileSize(const QString& filePath)
    {
        QString localPath;

        if (filePath.startsWith("file://")) {
            QUrl url(filePath);
            localPath = url.toLocalFile();
        } else {
            localPath = filePath;
        }

        QFileInfo fileInfo(localPath);
        return fileInfo.exists() ? fileInfo.size() : -1;
    }

    // 获取文件扩展名（跨平台）
    static QString getFileExtension(const QString& filePath)
    {
        QFileInfo fileInfo(filePath);
        return fileInfo.suffix().toLower();
    }

    // 判断是否是音视频文件（根据扩展名）
    static bool isMediaFile(const QString& filePath)
    {
        QString ext = getFileExtension(filePath);

        // 常见的音视频文件扩展名
        static const QStringList mediaExtensions = {
            "mp4", "avi", "mkv", "mov", "wmv", "flv", "mpg", "mpeg",
            "mp3", "wav", "wma", "aac", "flac", "ogg", "m4a"
        };

        return mediaExtensions.contains(ext);
    }

    // 解析并返回资源信息
    static SourceInfo resolveSourceInfo(const QString& source)
    {
        SourceInfo info;
        info.mode = resolveSourceType(source);

        qDebug() << "[AVSourceResolver::resolveSourceInfo] 源:" << source;
        qDebug() << "[AVSourceResolver::resolveSourceInfo] 解析的模式:" << static_cast<int>(info.mode);

        switch (info.mode) {
        case VideoPlayerMode::LOCAL_FILE: {
                info.url = normalizeLocalFilePath(source);
                info.nativePath = toPlatformNativePath(info.url);

                QFileInfo fileInfo(info.nativePath);
                info.fileName = fileInfo.fileName();
                info.fileExtension = getFileExtension(source);
                info.fileSize = getFileSize(info.url);
                info.isValid = isValidLocalFile(info.url);
                info._id = idGen::generateFileId(info.url);

                qDebug() << "[AVSourceResolver::resolveSourceInfo] 本地文件信息:";
                qDebug() << "  url:" << info.url;
                qDebug() << "  nativePath:" << info.nativePath;
                qDebug() << "  isValid:" << info.isValid;
                qDebug() << "  fileSize:" << info.fileSize << "\nid: " << info._id;
                break;
        }

            case VideoPlayerMode::HTTP_STREAM:
            case VideoPlayerMode::RTMP_STREAM: {
                info.url = source;
                info.nativePath = "";  // 网络流没有本地路径

                // 从URL中提取文件名
                QUrl url(source);
                QString path = url.path();
                if (!path.isEmpty()) {
                    QFileInfo fileInfo(path);
                    info.fileName = fileInfo.fileName();
                    info.fileExtension = fileInfo.suffix().toLower();
                }
                info.isValid = url.isValid() && !url.scheme().isEmpty();
                break;
            }
        }

        return info;
    }

    // 平台特定的辅助函数（如果需要）
    #ifdef Q_OS_WIN
    static QString getWindowsSpecialFolder(const QString& folderName)
    {
        // 可以根据需要添加Windows特殊文件夹的处理
        // 例如：Documents, Videos, Music等
        return "";
    }
    #endif

    #ifdef Q_OS_LINUX
    static QString getLinuxHomeFolder()
    {
        return QDir::homePath();
    }
    #endif

    #ifdef Q_OS_MAC
    static QString getMacApplicationsFolder()
    {
        return "/Applications";
    }
    #endif
};

#endif //RINZEPLAYER_AVSOURCERESOLVER_H