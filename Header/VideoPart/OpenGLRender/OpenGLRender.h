#ifndef OPENGLRENDER_H
#define OPENGLRENDER_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMutex>
#include <atomic>
#include <memory>

#include "../AVRes/AVData.h"
#include "Global.h"

using VideoUtil::VideoFrame;

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavcodec/avcodec.h>
}

class OpenGLRender : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    enum class VideoFormat {
        YUV420P,
        NV12
    };

    // 新增：色彩空间枚举（适配标清/高清/4K）
    enum class ColorSpace {
        BT601,  // 标清 720x480/576
        BT709,  // 高清 1080P
        BT2020  // 4K/8K
    };

    explicit OpenGLRender(QWidget* parent = nullptr);
    ~OpenGLRender() override;

    // 核心渲染接口
    void renderFrame(const std::shared_ptr<AVData>& frame);
    void clear();

    // 基础设置
    void setVideoSize(int width, int height);
    void setAspectRatioMode(Qt::AspectRatioMode mode = Qt::KeepAspectRatio);
    // 新增：设置色彩空间（由解码端调用）
    void setColorSpace(ColorSpace color_space);

    // 【添加】恢复被删掉的initialize函数（匹配LocalFilePlayer的调用）
    bool initialize();

    // 【添加】恢复被删掉的渲染控制函数
    void pauseRendering();
    void stopRendering();
public slots:
    void onFrameReady(std::shared_ptr<AVData> frame) {
    renderFrame(frame); // 直接调用核心渲染接口
    }
signals:
    void renderError(const QString& error);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    // 核心初始化
    bool initShaders();
    bool initBuffers();
    bool initTextures(int width, int height, VideoFormat format);

    // 纹理更新（修复：添加linesize参数）
    void updateYUV420PTexture(const uint8_t* y, const uint8_t* u, const uint8_t* v,
                              int width, int height,
                              int y_linesize, int u_linesize, int v_linesize);
    void updateNV12Texture(const uint8_t* y, const uint8_t* uv, int width, int height,
                           int y_linesize, int uv_linesize);

    // 视口计算
    void calculateViewport();

    // 资源清理
    void cleanupGL();

    // 新增：内存对齐工具函数（适配OpenGL纹理要求）
    int alignTo4(int value) { return (value + 3) & ~3; }

private:
    // OpenGL核心资源
    QOpenGLShaderProgram* yuv_program_ = nullptr;
    QOpenGLBuffer vbo_;
    QOpenGLVertexArrayObject vao_;
    GLuint textures_[3] = {0, 0, 0};
    bool textures_initialized_ = false;

    // 视频参数
    int video_width_ = 0;
    int video_height_ = 0;
    VideoFormat video_format_ = VideoFormat::YUV420P;
    Qt::AspectRatioMode aspect_mode_ = Qt::KeepAspectRatio;
    // 新增：当前色彩空间（默认BT601兼容标清）
    ColorSpace color_space_ = ColorSpace::BT601;

    // 渲染状态
    std::atomic<bool> is_initialized_{false};
    QMutex frame_mutex_;
    std::shared_ptr<AVData> current_frame_;

    // 顶点/纹理坐标（核心数据）
    const float vertices_[8] = {
        -1.0f,  1.0f,  // 左上
        -1.0f, -1.0f,  // 左下
         1.0f,  1.0f,  // 右上
         1.0f, -1.0f   // 右下
    };
    // 纹理坐标（与顶点一一对应，YUV420P采样关键）
    const float tex_coords_[8] = {
        0.0f, 0.0f,  // 左上
        0.0f, 1.0f,  // 左下
        1.0f, 0.0f,  // 右上
        1.0f, 1.0f   // 右下
    };

    // 新增：视口计算结果缓存
    int view_w_ = 0;
    int view_h_ = 0;
};

#endif // OPENGLRENDER_H