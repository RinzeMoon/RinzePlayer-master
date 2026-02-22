//
// Created by lsy on 2026/1/13.
//

#include "../Header/VideoPart/BasePlayerFrame.h"
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QLabel>

#include <QMessageBox>

#include "VideoPart/AVDataFetch/AVDataFetcher.h"
#include "VideoPart/AVDataFetch/LocalFileDataFetcher.h"
#include "../Header/Factory/AVDataFetcherFactory.h"
#include <QDebug>
#include <QTimer>

BasePlayerFrame::BasePlayerFrame(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_videoRenderer(new OpenGLRender(this))
    , m_AOutput(nullptr)
    , m_currentMode(VideoPlayerMode::LOCAL_FILE)
    , m_controller(nullptr)
    , m_avSync(nullptr)
    , m_Decoder(nullptr)
    , m_isPlaying(false)
{
    initUi();

    m_Decoder = std::make_shared<AVDecoder>();
    m_AOutput = new AudioOutput();
    m_avSync = new AVSync();
}

BasePlayerFrame::~BasePlayerFrame()
{
    // 停止当前播放
    if (m_dataFetcher) {
        // 可以添加停止逻辑
    }

}

void BasePlayerFrame::initUi()
{
    // 设置布局
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->addWidget(m_videoRenderer);

    // 设置视频渲染器
    m_videoRenderer->setMinimumSize(640, 480);
}

void BasePlayerFrame::connectSignals()
{
    // 如果需要连接信号，可以在这里实现
    // 暂时留空
}


void BasePlayerFrame::loadMedia(const QString& source, VideoPlayerMode mode)
{
    qDebug() << "BasePlayerFrame::loadMedia - source:" << source << "mode:" << static_cast<int>(mode);


    m_currentMode = mode;
    // 这里应该通过 ResMgr 来加载资源，但为了兼容性保留此方法
    // 实际播放逻辑在 setFetcher 中实现
}

bool BasePlayerFrame::setFetcher(std::unique_ptr<AVDataFetcher> fetcher)
{
    qDebug() << "BasePlayerFrame::setFetcher - 开始设置Fetcher";

    if (!fetcher) {
        qWarning() << "BasePlayerFrame::setFetcher - 传入的fetcher为空";
        return false;
    }

    // 停止当前播放
    if (m_isPlaying) {
        // 可以添加停止逻辑
        m_isPlaying = false;
        emit playbackStopped();
    }

    // 清理现有的Fetcher
    if (m_dataFetcher) {
        // 断开信号连接
        disconnect(m_dataFetcher.get(), nullptr, this, nullptr);
        m_dataFetcher.reset();
    }

    // 接管新的Fetcher
    m_dataFetcher = std::move(fetcher);
    qDebug() << "BasePlayerFrame::setFetcher - Fetcher地址:" << m_dataFetcher.get();

    // 更新当前源
    m_currentSource = m_dataFetcher->getSourceInfo();
    qDebug() << "BasePlayerFrame::setFetcher - 当前源:" << m_currentSource.url;

    // 连接Fetcher的信号
    connect(m_dataFetcher.get(), &AVDataFetcher::dataAvailable,
            this, [this]()
            {
                qDebug() << "[BasePlayerFrame] 数据已处理完成,即将进行解码";
            });


    connect(m_dataFetcher.get(), &AVDataFetcher::errorOccurred,
            this, [this](const QString& error) {
                qWarning() << "BasePlayerFrame - Fetcher错误:" << error;
                emit errorOccurred(error);
            });

    connect(m_dataFetcher.get(), &AVDataFetcher::stateChanged,
            this, [this](RinGlobal::TranState state) {
                qDebug() << "BasePlayerFrame - Fetcher状态变化:" << static_cast<int>(state);
            });

    connect(m_dataFetcher.get(), &AVDataFetcher::dataFinished,
            this, [this]() {
                qDebug() << "BasePlayerFrame - 数据读取完成";
            });

    // 如果是本地文件，可以获取文件信息
    if (LocalFileDataFetcher* localFetcher = dynamic_cast<LocalFileDataFetcher*>(m_dataFetcher.get())) {
        auto fileInfo = localFetcher->getFileInfo();
        qDebug() << "BasePlayerFrame - 文件信息:"
                 << "时长:" << fileInfo.durationMs << "ms,"
                 << "分辨率:" << fileInfo.videoWidth << "x" << fileInfo.videoHeight
                 << "FPS:" << fileInfo.fps;

        emit durationChanged(fileInfo.durationMs);
    }

    return true;
}

bool BasePlayerFrame::loadWithFetcher(VideoPlayerMode mode) {
    // 1. 基础校验
    if (!m_dataFetcher) {
        emit errorOccurred("Fetcher 实例为空");
        return false;
    }

    // 3. 初始化 Fetcher（仅调用 init，无逻辑处理）
    if (!m_dataFetcher->init(m_currentSource)) {
        emit errorOccurred("Fetcher 初始化失败");
        m_dataFetcher.reset();
        return false;
    }

    if (mode == RinGlobal::VideoPlayerMode::LOCAL_FILE) {
        // 1. 基础校验：确保Fetcher和Decoder实例有效（必须）
        auto localFetcher = m_dataFetcher.get();
        if (!localFetcher) {
            qCritical() << "loadWithFetcher: LocalFetcher 实例为空！";
            return false;
        }
        if (!m_Decoder) {
            qCritical() << "loadWithFetcher: AVDecoder 实例未初始化！";
            return false;
        }

        // 2. 核心：设置从LocalFetcher提取的time_base（你原有逻辑）
        AVRational audioTb = localFetcher->getAudioTimeBase();
        AVRational videoTb = localFetcher->getVideoTimeBase();
        // 兜底检查（防止time_base无效）
        if (audioTb.num == 0 || audioTb.den == 0) audioTb = av_make_q(1, 44100);
        if (videoTb.num == 0 || videoTb.den == 0) videoTb = av_make_q(1, 25);

        m_Decoder->setAudioTimeBase(audioTb);
        m_Decoder->setVideoTimeBase(videoTb);
        qDebug() << "LocalFile: 设置TimeBase"
                 << "\n  Video:" << videoTb.num << "/" << videoTb.den
                 << "\n  Audio:" << audioTb.num << "/" << audioTb.den;

        AVCodecParameters* videoParams = localFetcher->getVideoCodecParams();
        AVCodecParameters* audioParams = localFetcher->getAudioCodecParams();

        // 初始化视频解码器
        if (videoParams) {
            bool videoInitOk = m_Decoder->initVideoDecoder(videoParams,nullptr,0);
            if (!videoInitOk) {
                qCritical() << "LocalFile: 视频解码器初始化失败！";
                return false;
            }
            // 设置视频流索引（对应AVDecoder的setVideoStreamIndex）
            m_Decoder->setVideoStreamIndex(localFetcher->getVideoStreamIndex());
        }
        // 初始化音频解码器
        if (audioParams) {
            bool audioInitOk = m_Decoder->initAudioDecoder(audioParams,nullptr,0);
            if (!audioInitOk) {
                qCritical() << "LocalFile: 音频解码器初始化失败！";
                return false;
            }
            // 设置音频流索引（对应AVDecoder的setAudioStreamIndex）
            AVDecoder::AudioSpec audioSpec;
            audioSpec.sample_rate = audioParams->sample_rate;
            audioSpec.channels = audioParams->channels;
            audioSpec.format = static_cast<AVSampleFormat>(audioParams->format);  // 得到 fltp
            audioSpec.is_planar = av_sample_fmt_is_planar(audioSpec.format);      // 得到 true
            audioSpec.channel_layout = audioParams->channel_layout;

            AVDecoder::AudioSpec sdlSpec = audioSpec;
            if (sdlSpec.is_planar) {
                sdlSpec.format = static_cast<AVSampleFormat>(
                    av_get_packed_sample_fmt(sdlSpec.format)  // fltp -> flt
                );
                sdlSpec.is_planar = false;  // 标记为打包格式
            }

            qDebug() << "音频流真实参数:"
                     << "采样率=" << sdlSpec.sample_rate
                     << "声道=" << sdlSpec.channels
                     << "格式=" << av_get_sample_fmt_name(sdlSpec.format)
                     << "平面=" << sdlSpec.is_planar
                     << "声道布局=" << sdlSpec.channel_layout;

            // 这里应该是配置fetcher的接口
            m_Decoder->setAudioOutputSpec(sdlSpec);
            m_AOutput->initialize(sdlSpec);

            m_AOutput->setAudioCallback([this](uint8_t* stream, int len) {
                this->sdlAudioCallback(stream, len);
            });

        }

        // 4. 【关键补充】绑定数据源（对应AVDecoder的setVideo/AudioFetcher）
        // 绑定视频数据包获取器（PacketFetcher是std::function，绑定LocalFetcher的取包逻辑）
        m_Decoder->setVideoFetcher([localFetcher]() -> std::shared_ptr<AVData> {
            return localFetcher->getNextVideoPacket(); // LocalFetcher需实现fetchVideoPacket方法
        });
        // 绑定音频数据包获取器
        m_Decoder->setAudioFetcher([localFetcher]() -> std::shared_ptr<AVData> {
            return localFetcher->getNextAudioPacket(); // LocalFetcher需实现fetchAudioPacket方法
        });

        qDebug() << "LocalFile: AVDecoder 已启动解码，当前模式：LOCAL_FILE";

        // 7. （可选）绑定同步/状态信号（Qt信号槽，按需）
        connect(m_Decoder.get(), &AVDecoder::stateChanged, this, [this](AVDecoder::State state) {
            qDebug() << "AVDecoder状态变化：" << static_cast<int>(state);
            // 可处理状态变化（如暂停、停止、出错）
        });
        connect(m_Decoder.get(), &AVDecoder::errorOccurred, this, [this](const QString& error) {
            qCritical() << "AVDecoder错误：" << error;
        });

    }

    if (mode == VideoPlayerMode::HTTP_STREAM)
    {
        qDebug() << "等待处理";
    }


    // 6. 仅设置播放模式
    m_currentMode = mode;

    qDebug() << "Fetcher 资源处理完毕";
    return true;
}

bool BasePlayerFrame::startPlayback()
{
    qDebug() << "BasePlayerFrame::startPlayback - 开始播放";


    // 纯多余了
    return true;
}

// ========== 槽函数实现 ==========

void BasePlayerFrame::onPlayClicked()
{
    qDebug() << "onPlayClicked 开始播放,资源id" << m_currentSource._id;

    if (m_dataFetcher && !m_isPlaying) {
        if (startPlayback()) {
            m_isPlaying = true;
            emit playbackStarted();
        }
    }
}

void BasePlayerFrame::onPauseClicked()
{
    qDebug() << "BasePlayerFrame::onPauseClicked";

    if (m_isPlaying) {
        m_isPlaying = false;
        emit playbackPaused();
        // TODO: 实现暂停逻辑
    }
}

void BasePlayerFrame::onStopClicked()
{
    qDebug() << "BasePlayerFrame::onStopClicked";

    if (m_isPlaying || m_dataFetcher) {
        m_isPlaying = false;
        emit playbackStopped();
        // TODO: 实现停止逻辑

        // 可以清理Fetcher
        if (m_dataFetcher) {
            m_dataFetcher.reset();
        }
    }
}

void BasePlayerFrame::onSeekRequested(qint64 position)
{
    qDebug() << "BasePlayerFrame::onSeekRequested - position:" << position;
    // TODO: 实现跳转逻辑
}

void BasePlayerFrame::onVolumeChanged(int volume)
{
    qDebug() << "BasePlayerFrame::onVolumeChanged - volume:" << volume;
    // TODO: 实现音量控制
}

void BasePlayerFrame::onPositionChanged(qint64 position)
{
    // 这个信号应该由播放器内部触发
    // 暂时留空
}

void BasePlayerFrame::onDurationChanged(qint64 duration)
{
    // 这个信号已经由setFetcher触发
    // 暂时留空
}

void BasePlayerFrame::onDataPacketReady(std::shared_ptr<AVData> packet)
{
    // 这是最重要的函数 - 处理从Fetcher来的数据包

    if (!packet) {
        qWarning() << "BasePlayerFrame::onDataPacketReady - 收到空数据包";
        return;
    }

    // TODO: 根据数据类型处理
    // 1. 视频包 -> 解码 -> 渲染
    // 2. 音频包 -> 解码 -> 播放

    // 临时打印信息
    static int packetCount = 0;
    packetCount++;

    if (packetCount % 100 == 0) {
        qDebug() << "BasePlayerFrame::onDataPacketReady - 已处理" << packetCount << "个数据包";
    }

    // 简单演示：直接尝试解码视频帧
    onVideoFrameReady(packet);
}

void BasePlayerFrame::onVideoFrameReady(std::shared_ptr<AVData> frame)
{
    // TODO: 处理视频帧渲染
    if (!frame || !m_videoRenderer) {
        return;
    }
}

bool BasePlayerFrame::isPlaying() const
{
    return m_isPlaying;
}

const QString& BasePlayerFrame::currentSource() const
{
    return QString("这个方法要改BasePlayerFrame::currentSource ");
}

void BasePlayerFrame::onResReady(const SourceInfo &sf)
{
    qDebug() << "开始解析资源...";

    m_dataFetcher->init(sf);
    m_dataFetcher->start();
}


void BasePlayerFrame::setController(QWidget* controller)
{
    qDebug() << "BasePlayerFrame::setController - 开始设置控制器";
    if (m_controller) {  // 只有非空时才移除
        qDebug() << "移除旧控制器";

        // 断开父子关系 + 从布局移除
        m_controller->setParent(nullptr);
        if (m_mainLayout) {  // 校验布局是否有效
            m_mainLayout->removeWidget(m_controller);
        }

        // 延迟销毁（避免当前循环崩溃）+ 置空
        m_controller->deleteLater();
        m_controller = nullptr;  // 关键！置空避免野指针
    }
    // 设置新的控制器
    m_controller = controller;

    if (m_controller) {
        qDebug() << "添加新控制器到布局";
        m_mainLayout->addWidget(qobject_cast<QWidget*>(m_controller));
        qDebug() << "控制器设置完成";
    } else {
        qWarning() << "BasePlayerFrame::setController - 传入的控制器为空";
    }
}

void BasePlayerFrame::onPlayRequest(const QString id, VideoPlayerMode mode)
{
    qDebug() << "收到播放请求,处理播放" << "id: " << id;
    if (!m_dataFetcher)
    {
        qDebug() << "[Warn] 无资源";
        return;
    }

    if (getCurrentSource()._id == id)
    {
        qDebug() << "两个id相同,准备播放";
    }
    else
    {
        qDebug() << "ID 不同!";
        qDebug() << "CurrentId: " << getCurrentSource()._id;
        qDebug() << "SignalId;" << id;
    }

    qDebug() << "Load Fetcher";
    loadWithFetcher(mode);
    m_Decoder->start();

    qDebug() << "Load Fetcher 完成";

    // 启动音频提取线程（从解码器到AVSync）
    startAudioExtraction();

    // 启动音频播放线程（从AVSync到SDL）
    startAudioPlayback();

    // 启动视频提取线程
    startAsyncFrameExtraction();

}

void BasePlayerFrame::startAsyncFrameExtraction()
{
    m_shouldStopExtractor = false;
    // 1. 获取视频真实FPS，计算浮点型帧间隔
    double video_fps = m_Decoder->getFPS();
    if (video_fps <= 0) video_fps = 25.0;
    double frame_interval_ms_f = 1000.0 / video_fps;
    const int QUEUE_MAX_SIZE = 120; // 队列限流（进一步减少到120，避免队列过大）

    // 线程1：填充视频帧（限流保留，避免队列堆积）
    QFuture f1 = QtConcurrent::run([this, QUEUE_MAX_SIZE]() {
    int consecutive_full_count = 0;

    while (!m_shouldStopExtractor) {
        int queue_size = m_avSync->getVideoQueueSize();

        // 动态调整解码速度：队列越满，sleep越久
        if (queue_size >= QUEUE_MAX_SIZE) {
            // 队列满，sleep中等时间
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            consecutive_full_count++;
            if (consecutive_full_count % 100 == 0) {
                qDebug() << "[视频解码] 队列满，连续等待" << consecutive_full_count << "次";
            }
        } else if (queue_size >= QUEUE_MAX_SIZE * 0.8) {
            // 队列80%满，sleep短时间
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            consecutive_full_count = 0;
        } else if (queue_size >= QUEUE_MAX_SIZE * 0.5) {
            // 队列50%满，sleep很短时间
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
            consecutive_full_count = 0;
        } else {
            // 队列不紧张，不sleep
            consecutive_full_count = 0;
        }

        auto frame = m_Decoder->getNextVideoFrame(10);
        if (frame) {
            m_avSync->putVideo(frame);
        } else {
            // 没有帧时sleep一下，避免空转
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    qDebug() << "视频读取线程已退出";
});

    m_extractorFuture = QtConcurrent::run([this, frame_interval_ms_f]() {
        int render_count = 0;
        auto fps_start_time = std::chrono::steady_clock::now();
        // 新增：记录上一帧的渲染时间
        auto last_render_time = std::chrono::steady_clock::now();

        while (!m_shouldStopExtractor) {
            // 获取视频帧（AVSync内部会处理同步）
            auto frame = m_avSync->getNextVideoFrame(50);

            if (frame) {
                // 核心：计算需要等待的时间，保证按FPS渲染
                auto now = std::chrono::steady_clock::now();
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_render_time).count();
                // 计算需要补的sleep时间（保证每帧间隔≈理论帧间隔）
                int sleep_ms = static_cast<int>(frame_interval_ms_f - elapsed_ms);
                if (sleep_ms > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
                }

                // 渲染帧（优化：如果在GUI线程则直接调用，否则使用QueuedConnection）
                if (QThread::currentThread() == this->thread()) {
                    // 在GUI线程，直接调用
                    m_videoRenderer->renderFrame(frame);
                } else {
                    // 在其他线程，使用QueuedConnection
                    QMetaObject::invokeMethod(this, [this, frame]() {
                        m_videoRenderer->renderFrame(frame);
                    }, Qt::QueuedConnection);
                }

                // 更新上一帧渲染时间
                last_render_time = std::chrono::steady_clock::now();

                // 修复帧率统计：每10帧重置一次开始时间（统计实时帧率）
                render_count++;
                if (render_count % 10 == 0) {
                    auto fps_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        last_render_time - fps_start_time
                    ).count();
                    double actual_fps = fps_elapsed_ms > 0 ?
                        (render_count * 1000.0) / fps_elapsed_ms : 0;

                    double audio_time = m_avSync->getAudioClock();
                    qDebug() << "已渲染" << render_count << "帧 | 实时FPS:" << actual_fps
                             << " | 音频时钟:" << audio_time << "ms";
                    // 重置统计起点，下次统计新的10帧
                    fps_start_time = last_render_time;
                    render_count = 0;
                }
            } else {
                // 优化：没有帧时不要sleep太久，避免延迟
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });
}

void BasePlayerFrame::startAudioExtraction()
{
    m_shouldStopAudioExtractor = false;

    m_audioExtractorFuture = QtConcurrent::run([this]() {
        qDebug() << "音频提取线程启动";

        int no_audio_count = 0;
        const int MAX_NO_AUDIO_COUNT = 100;  // 最多连续100次没有音频才输出警告

        while (!m_shouldStopAudioExtractor) {
            try {
                // 从解码器获取音频帧（减少超时时间，提高响应速度）
                std::shared_ptr<AVData> audio_data = m_Decoder->getNextAudioFrame(-1);

                if (audio_data && audio_data->isAudio() && m_avSync) {
                    // 放入AVSync队列（用于音视频同步）
                    m_avSync->putAudio(audio_data);
                    no_audio_count = 0;  // 重置计数
                    // qDebug() << "音频帧放入AVSync, pts:" << audio_data->pts();
                } else {
                    no_audio_count++;
                    if (no_audio_count % MAX_NO_AUDIO_COUNT == 0) {
                        qDebug() << "[音频提取] 连续" << no_audio_count << "次未获取到音频帧"
                                 << "音频队列大小:" << (m_avSync ? m_avSync->getAudioQueueSize() : -1);
                    }
                }

            }
            catch (const std::exception& e) {
                qWarning() << "音频提取线程异常：" << e.what();
                QThread::msleep(10);
            }
        }

        qDebug() << "音频提取线程退出";
    });
}

void BasePlayerFrame::sdlAudioCallback(uint8_t* stream, int len) {
    memset(stream, 0, len);
    int bytes_filled = 0;

    // ========== 核心修复：正确处理初始同步 ==========
    static int64_t total_samples_played = 0;
    static double audio_base_pts_seconds = 0.0;
    static bool audio_base_set = false;
    static int audio_sample_rate = 0;
    static int audio_channels = 0;
    static int audio_bytes_per_sample = 2;

    // 新添加：跟踪第一个真实音频帧到达时的样本计数
    static int64_t samples_before_first_audio = 0;
    static bool first_audio_received = false;

    // 新增：重试机制和静音计数
    static int consecutive_silence_count = 0;
    static int total_silence_count = 0;
    const int MAX_RETRY_ATTEMPTS = 3;  // 最多重试3次
    const int MAX_CONSECUTIVE_SILENCE = 5;  // 最多连续5次静音才真正填充静音

    static int callback_count = 0;
    callback_count++;

    while (bytes_filled < len) {
        int retry_attempts = 0;
        std::shared_ptr<AVData> audio_data = nullptr;

        // 重试机制：最多重试MAX_RETRY_ATTEMPTS次
        while (retry_attempts < MAX_RETRY_ATTEMPTS && !audio_data) {
            try {
                audio_data = m_avSync->getNextAudioFrame(50);
            } catch (const std::exception& e) {
                qWarning() << "获取音频帧异常:" << e.what();
            }

            if (!audio_data || !audio_data->isAudio()) {
                retry_attempts++;
                if (retry_attempts < MAX_RETRY_ATTEMPTS) {
                    // 短暂等待后重试
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            } else {
                // 成功获取到音频数据，重置静音计数
                consecutive_silence_count = 0;
                break;
            }
        }

        // 情况1：重试后仍然没有获取到音频数据
        if (!audio_data || !audio_data->isAudio()) {
            consecutive_silence_count++;
            total_silence_count++;

            // 填充静音
            int bytes_to_fill = len - bytes_filled;
            memset(stream + bytes_filled, 0, bytes_to_fill);
            bytes_filled += bytes_to_fill;

            // 调试日志：每100次静音输出一次
            if (total_silence_count % 100 == 0) {
                qDebug() << "[SDL音频回调] 静音计数:" << total_silence_count 
                         << "连续静音:" << consecutive_silence_count
                         << "音频队列大小:" << m_avSync->getAudioQueueSize();
            }

            // 关键修改：在第一个音频帧到达前，不累计静音样本
            if (audio_sample_rate > 0 && audio_channels > 0 && audio_bytes_per_sample > 0) {
                int silent_samples = bytes_to_fill / (audio_channels * audio_bytes_per_sample);

                if (!first_audio_received) {
                    // 记录在第一个音频帧之前的静音样本
                    samples_before_first_audio += silent_samples;
                } else {
                    // 第一个音频帧之后，正常累计
                    total_samples_played += silent_samples;
                }
            }

            break;
        }

        auto audio_frame = audio_data->audioFrame();
        if (!audio_frame || !audio_frame->data[0]) {
            consecutive_silence_count++;
            continue;
        }

        // 1. 初始化音频基础参数
        if (audio_sample_rate == 0) {
            audio_sample_rate = audio_frame->sample_rate;
            audio_channels = audio_frame->channels;
            audio_bytes_per_sample = av_get_bytes_per_sample(audio_frame->sample_fmt);
            if (audio_bytes_per_sample <= 0) audio_bytes_per_sample = 2;
        }

        // 2. 第一个真实音频帧到达
        if (!first_audio_received) {
            // 设置基准PTS
            audio_base_pts_seconds = audio_data->pts() / 1000000.0;
            audio_base_set = true;
            first_audio_received = true;

            // 关键修复：调整基准PTS，减去已经播放的静音时长
            double silent_duration = (double)samples_before_first_audio / audio_sample_rate;
            audio_base_pts_seconds -= silent_duration;

            qDebug() << "[SDL音频回调] 第一个音频帧到达，静音样本:" << samples_before_first_audio
                     << "静音时长:" << silent_duration << "秒";
        }

        // 3. 计算并复制音频数据
        int frame_bytes = audio_frame->nb_samples * audio_channels * audio_bytes_per_sample;
        int bytes_needed = len - bytes_filled;
        int bytes_to_copy = std::min(frame_bytes, bytes_needed);
        int samples_copied = bytes_to_copy / (audio_bytes_per_sample * audio_channels);

        memcpy(stream + bytes_filled, audio_frame->data[0], bytes_to_copy);
        bytes_filled += bytes_to_copy;

        // 4. 累计播放样本数
        total_samples_played += samples_copied;

    }

    if (audio_base_set && audio_sample_rate > 0) {
        // 1. 计算已播放的音频时长（秒）
        double played_duration_seconds = (double)total_samples_played / audio_sample_rate;

        // 2. 当前音频时间（秒）= 基准PTS（秒）+ 已播放时长（秒）
        double current_audio_seconds = audio_base_pts_seconds + played_duration_seconds;

        // 3. 转换为毫秒
        double current_audio_ms = current_audio_seconds * 1000.0;

        // 4. 更新到AVSync
        m_avSync->setAudioClock(current_audio_ms);
    }
}

void BasePlayerFrame::startAudioPlayback()
{
    qDebug() << "开始播放....";

    // 音频帧预加载机制：在启动SDL音频之前，先预加载一定数量的音频帧
    qDebug() << "[音频预加载] 开始预加载音频帧...";
    const int PRELOAD_FRAMES = 100;  // 预加载100帧（约2-3秒的音频）
    int preload_count = 0;
    
    // 等待解码器启动
    QThread::msleep(100);
    
    // 预加载音频帧到AVSync队列
    for (int i = 0; i < PRELOAD_FRAMES * 2; i++) {  // 最多尝试2倍次数
        std::shared_ptr<AVData> audio_data = m_Decoder->getNextAudioFrame(50);
        if (audio_data && audio_data->isAudio()) {
            m_avSync->putAudio(audio_data);
            preload_count++;
            
            if (preload_count >= PRELOAD_FRAMES) {
                break;
            }
        } else {
            QThread::msleep(5);  // 短暂等待
        }
    }
    
    qDebug() << "[音频预加载] 完成，预加载了" << preload_count << "帧音频";

    m_AOutput->setAudioCallback([this](uint8_t* stream, int len)
    {
        this->sdlAudioCallback(stream, len);
    });

    bool t = m_AOutput->start();
    qDebug() << "[音频播放] " << t;
}


