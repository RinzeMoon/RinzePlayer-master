#include "OpenGLRender.h"
#include <QDebug>
#include <QMatrix4x4>
#include <cstring> // 内存操作头文件

OpenGLRender::OpenGLRender(QWidget* parent)
    : QOpenGLWidget(parent)
{
    // OpenGL 3.3核心配置
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(3, 3);
    format.setSwapInterval(1); // 垂直同步，避免画面撕裂
    setFormat(format);
}

OpenGLRender::~OpenGLRender() {
    cleanupGL();
}

// 新增：设置色彩空间接口（解码端调用）
void OpenGLRender::setColorSpace(ColorSpace color_space) {
    QMutexLocker locker(&frame_mutex_);
    color_space_ = color_space;
}

void OpenGLRender::initializeGL() {
    initializeOpenGLFunctions();

    // 基础配置
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glDisable(GL_DEPTH_TEST); // 2D渲染不需要深度测试
    glDisable(GL_CULL_FACE);  // 禁用面剔除，避免渲染不全
    glEnable(GL_TEXTURE_2D);  // 启用2D纹理

    // 初始化着色器和缓冲区
    if (!initShaders() || !initBuffers()) {
        qCritical() << "OpenGL资源初始化失败";
        emit renderError("Failed to initialize OpenGL resources");
        return;
    }

    is_initialized_ = true;
}

void OpenGLRender::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);

    QMutexLocker locker(&frame_mutex_);
    if (!current_frame_) {
        qWarning() << "当前无有效视频帧对象";
        return;
    }

    auto vf = current_frame_->videoFrame();
    if (!vf) {
        qWarning() << "视频帧数据结构体为空";
        return;
    }

    // 获取帧数据指针
    uint8_t** frame_data = vf->getData();
    if (!frame_data) {
        qWarning() << "视频帧数据指针数组为空";
        return;
    }

    uint8_t* y_data = frame_data[0];
    uint8_t* u_data = frame_data[1];
    uint8_t* v_data = frame_data[2];

    // 校验Y分量指针
    if (y_data == nullptr || reinterpret_cast<uintptr_t>(y_data) < 0x100) {
        qWarning() << "无效的Y分量指针：地址=" << reinterpret_cast<void*>(y_data);
        return;
    }

    // YUV420P格式校验
    if (vf->pix_fmt == AV_PIX_FMT_YUV420P) {
        if (u_data == nullptr || v_data == nullptr) {
            qWarning() << "YUV420P帧缺少U/V分量指针";
            return;
        }
        if (reinterpret_cast<uintptr_t>(u_data) < 0x100 || reinterpret_cast<uintptr_t>(v_data) < 0x100) {
            qWarning() << "U/V分量指针地址非法：u=" << reinterpret_cast<void*>(u_data) << " v=" << reinterpret_cast<void*>(v_data);
            return;
        }
    }

    // 尺寸变化处理（兼容奇数分辨率）
    if (video_width_ != vf->width || video_height_ != vf->height) {
        video_width_ = vf->width;
        video_height_ = vf->height;
        calculateViewport();

        if (video_width_ <= 0 || video_height_ <= 0) {
            qWarning() << "无效的视频尺寸：" << video_width_ << "x" << video_height_;
            return;
        }

        bool init_ok = false;
        if (vf->pix_fmt == AV_PIX_FMT_YUV420P) {
            init_ok = initTextures(video_width_, video_height_, VideoFormat::YUV420P);
        } else if (vf->pix_fmt == AV_PIX_FMT_NV12) {
            init_ok = initTextures(video_width_, video_height_, VideoFormat::NV12);
        } else {
            qWarning() << "不支持的像素格式：" << av_get_pix_fmt_name(vf->pix_fmt);
            return;
        }

        if (!init_ok) {
            qWarning() << "纹理初始化失败";
            return;
        }

        // 修复：视口设置移到纹理初始化后，确保尺寸匹配
        glViewport((this->width() - view_w_)/2, (this->height() - view_h_)/2, view_w_, view_h_);
    }

    // 更新纹理并渲染
    if (yuv_program_ && yuv_program_->isLinked()) {
        switch (vf->pix_fmt) {
        case AV_PIX_FMT_YUV420P:
            if (u_data != nullptr && v_data != nullptr && reinterpret_cast<uintptr_t>(u_data) > 0x100) {
                int y_linesize = vf->linesize[0];
                int u_linesize = vf->linesize[1];
                int v_linesize = vf->linesize[2];
                updateYUV420PTexture(y_data, u_data, v_data, vf->width, vf->height,
                                     y_linesize, u_linesize, v_linesize);
            } else {
                qWarning() << "U/V分量指针无效，无法更新纹理";
                return;
            }
            break;

        case AV_PIX_FMT_NV12:
            if (u_data != nullptr && reinterpret_cast<uintptr_t>(u_data) > 0x100) {
                int y_linesize = vf->linesize[0];
                int uv_linesize = vf->linesize[1];
                updateNV12Texture(y_data, u_data, vf->width, vf->height,
                                  y_linesize, uv_linesize);
            } else {
                qWarning() << "NV12 UV分量指针无效，无法更新纹理";
                return;
            }
            break;

        default:
            qWarning() << "不支持的像素格式：" << av_get_pix_fmt_name(vf->pix_fmt);
            return;
        }

        // 绑定着色器
        if (yuv_program_->bind()) {
            bool texture_valid = true;
            for (int i = 0; i < 3; i++) {
                if (textures_[i] == 0) {
                    texture_valid = false;
                    break;
                }
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, textures_[i]);
                QString uniform_name = QString("tex_%1").arg(i);
                yuv_program_->setUniformValue(uniform_name.toUtf8().constData(), i);
            }

            // 传递视频格式
            int format_flag = (vf->pix_fmt == AV_PIX_FMT_YUV420P) ? 0 : 1;
            yuv_program_->setUniformValue("video_format", format_flag);

            // 新增：传递色彩空间（动态适配BT601/BT709/BT2020）
            int color_space_flag = static_cast<int>(color_space_);
            yuv_program_->setUniformValue("color_space", color_space_flag);

            if (!texture_valid) {
                qWarning() << "纹理ID无效，无法绘制";
                yuv_program_->release();
                return;
            }

            vao_.bind();
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); // 绘制全屏四边形
            vao_.release();
            yuv_program_->release();
        } else {
            qWarning() << "着色器绑定失败";
        }
    } else {
        qWarning() << "着色器未初始化或未链接";
    }
}

void OpenGLRender::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    calculateViewport();
    glViewport((w - view_w_)/2, (h - view_h_)/2, view_w_, view_h_);
}

void OpenGLRender::renderFrame(const std::shared_ptr<AVData>& frame) {
    if (!frame || !frame->videoFrame() || !is_initialized_) {
        return;
    }

    QMutexLocker locker(&frame_mutex_);
    current_frame_ = frame;
    update(); // 触发paintGL渲染
}

void OpenGLRender::clear() {
    QMutexLocker locker(&frame_mutex_);
    current_frame_.reset();
    makeCurrent();
    glClear(GL_COLOR_BUFFER_BIT);
    doneCurrent();
    update();
}

// 修复编译错误：用this->区分width/height成员函数和参数
void OpenGLRender::setVideoSize(int width, int height) {
    video_width_ = width;
    video_height_ = height;
    calculateViewport();
    glViewport((this->width() - view_w_)/2, (this->height() - view_h_)/2, view_w_, view_h_);
    update();
}

// 修复编译错误：用this->区分width/height成员函数
void OpenGLRender::setAspectRatioMode(Qt::AspectRatioMode mode) {
    aspect_mode_ = mode;
    calculateViewport();
    glViewport((this->width() - view_w_)/2, (this->height() - view_h_)/2, view_w_, view_h_);
    update();
}

bool OpenGLRender::initShaders() {
    if (yuv_program_) {
        delete yuv_program_;
        yuv_program_ = nullptr;
    }

    yuv_program_ = new QOpenGLShaderProgram(this);

    // 顶点着色器（保持不变，全屏渲染）
    const QString vertexShader = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoord;
        out vec2 texCoord;

        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            texCoord = aTexCoord;
        }
    )";

    // 片段着色器（核心修改：动态色彩空间+高精度UV采样）
    const QString fragmentShader = R"(
        #version 330 core
        in vec2 texCoord;
        out vec4 FragColor;

        uniform sampler2D tex_0; // Y
        uniform sampler2D tex_1; // U/NV12-UV
        uniform sampler2D tex_2; // V（NV12时未使用）
        uniform int video_format; // 0=YUV420P, 1=NV12
        uniform int color_space;  // 0=BT601,1=BT709,2=BT2020

        void main() {
            // 高精度采样Y分量
            float y = texture(tex_0, texCoord).r;
            float u = 0.0;
            float v = 0.0;

            // 优化UV采样：基于实际纹理尺寸取整，避免浮点偏移
            if (video_format == 0) { // YUV420P
                vec2 uv_texCoord = vec2(
                    floor(texCoord.x * float(textureSize(tex_0, 0).x)) / float(textureSize(tex_1, 0).x * 2.0),
                    floor(texCoord.y * float(textureSize(tex_0, 0).y)) / float(textureSize(tex_1, 0).y * 2.0)
                );
                u = texture(tex_1, uv_texCoord).r - 0.5;
                v = texture(tex_2, uv_texCoord).r - 0.5;
            } else if (video_format == 1) { // NV12
                vec2 uv_texCoord = vec2(
                    floor(texCoord.x * float(textureSize(tex_0, 0).x)) / float(textureSize(tex_1, 0).x * 2.0),
                    floor(texCoord.y * float(textureSize(tex_0, 0).y)) / float(textureSize(tex_1, 0).y * 2.0)
                );
                vec2 uv = texture(tex_1, uv_texCoord).rg - vec2(0.5, 0.5);
                u = uv.r;
                v = uv.g;
            }

            // 动态YUV转RGB系数（适配不同色彩空间）
            float r, g, b;
            if (color_space == 0) { // BT601（标清）
                r = clamp(y + 1.402 * v, 0.0, 1.0);
                g = clamp(y - 0.34414 * u - 0.71414 * v, 0.0, 1.0);
                b = clamp(y + 1.772 * u, 0.0, 1.0);
            } else if (color_space == 1) { // BT709（高清）
                r = clamp(y + 1.5748 * v, 0.0, 1.0);
                g = clamp(y - 0.1873 * u - 0.4681 * v, 0.0, 1.0);
                b = clamp(y + 1.8556 * u, 0.0, 1.0);
            } else { // BT2020（4K）
                r = clamp(y + 1.4746 * v, 0.0, 1.0);
                g = clamp(y - 0.2718 * u - 0.5436 * v, 0.0, 1.0);
                b = clamp(y + 1.8814 * u, 0.0, 1.0);
            }

            FragColor = vec4(r, g, b, 1.0);
        }
    )";

    // 编译着色器
    if (!yuv_program_->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader)) {
        qCritical() << "顶点着色器编译失败:" << yuv_program_->log();
        return false;
    }

    if (!yuv_program_->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader)) {
        qCritical() << "片段着色器编译失败:" << yuv_program_->log();
        return false;
    }

    // 链接着色器
    if (!yuv_program_->link()) {
        qCritical() << "着色器链接失败:" << yuv_program_->log();
        return false;
    }

    return true;
}

bool OpenGLRender::initBuffers() {
    vao_.create();
    vbo_.create();

    vao_.bind();
    vbo_.bind();
    // 分配顶点+纹理坐标内存
    vbo_.allocate(sizeof(vertices_) + sizeof(tex_coords_));

    // 写入顶点和纹理坐标
    vbo_.write(0, vertices_, sizeof(vertices_));
    vbo_.write(sizeof(vertices_), tex_coords_, sizeof(tex_coords_));

    // 绑定顶点属性
    if (yuv_program_->bind()) {
        // 顶点坐标属性（location 0）
        yuv_program_->enableAttributeArray(0);
        yuv_program_->setAttributeBuffer(0, GL_FLOAT, 0, 2, 0);

        // 纹理坐标属性（location 1）
        yuv_program_->enableAttributeArray(1);
        yuv_program_->setAttributeBuffer(1, GL_FLOAT, sizeof(vertices_), 2, 0);

        yuv_program_->release();
    }

    vao_.release();
    vbo_.release();

    return true;
}

bool OpenGLRender::initTextures(int width, int height, VideoFormat format) {
    // 清理旧纹理
    if (textures_initialized_) {
        glDeleteTextures(3, textures_);
        textures_initialized_ = false;
    }

    // 生成3个纹理对象（Y/U/V）
    glGenTextures(3, textures_);
    for (int i = 0; i < 3; i++) {
        glBindTexture(GL_TEXTURE_2D, textures_[i]);
        // 纹理过滤：线性插值，避免锯齿
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // 纹理环绕：边缘夹紧，避免重复
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // 预分配纹理内存（兼容奇数分辨率）
    int uv_width = (width + 1) / 2;  // 奇数宽度向上取整
    int uv_height = (height + 1) / 2;// 奇数高度向上取整

    if (format == VideoFormat::YUV420P) {
        // Y纹理（单通道）
        glBindTexture(GL_TEXTURE_2D, textures_[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

        // U纹理（单通道）
        glBindTexture(GL_TEXTURE_2D, textures_[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, uv_width, uv_height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

        // V纹理（单通道）
        glBindTexture(GL_TEXTURE_2D, textures_[2]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, uv_width, uv_height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    } else if (format == VideoFormat::NV12) {
        // Y纹理（单通道）
        glBindTexture(GL_TEXTURE_2D, textures_[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

        // UV纹理（RG双通道）
        glBindTexture(GL_TEXTURE_2D, textures_[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, uv_width, uv_height, 0, GL_RG, GL_UNSIGNED_BYTE, nullptr);
    }

    textures_initialized_ = true;
    return true;
}

void OpenGLRender::updateYUV420PTexture(const uint8_t* y, const uint8_t* u, const uint8_t* v,
                                        int width, int height,
                                        int y_linesize, int u_linesize, int v_linesize) {
    // 1. 入参合法性校验
    if (!y || !u || !v || width <= 0 || height <= 0) {
        qWarning() << "[updateYUV420PTexture] 无效入参：y=" << reinterpret_cast<const void*>(y)
                  << " u=" << reinterpret_cast<const void*>(u) << " v=" << reinterpret_cast<const void*>(v)
                  << " 尺寸=" << width << "x" << height;
        return;
    }
    if (y_linesize <= 0 || u_linesize <= 0 || v_linesize <= 0) {
        qWarning() << "[updateYUV420PTexture] 非法行步长：y=" << y_linesize
                  << " u=" << u_linesize << " v=" << v_linesize;
        return;
    }

    // 2. 兼容奇数分辨率的UV尺寸
    int uv_width = (width + 1) / 2;
    int uv_height = (height + 1) / 2;
    if (uv_width <= 0 || uv_height <= 0) {
        qWarning() << "[updateYUV420PTexture] UV平面尺寸非法：" << uv_width << "x" << uv_height;
        return;
    }

    // 3. 内存对齐（匹配OpenGL 4字节对齐要求）
    int y_align = alignTo4(width);
    int uv_align = alignTo4(uv_width);

    // 4. 临时缓冲区：去除FFmpeg linesize冗余字节
    // Y分量缓冲区
    uint8_t* y_buf = new uint8_t[y_align * height];
    memset(y_buf, 0, y_align * height); // 初始化避免脏数据
    for (int i = 0; i < height; i++) {
        memcpy(y_buf + i * y_align, y + i * y_linesize, width);
    }

    // U分量缓冲区
    uint8_t* u_buf = new uint8_t[uv_align * uv_height];
    memset(u_buf, 0, uv_align * uv_height);
    for (int i = 0; i < uv_height; i++) {
        memcpy(u_buf + i * uv_align, u + i * u_linesize, uv_width);
    }

    // V分量缓冲区
    uint8_t* v_buf = new uint8_t[uv_align * uv_height];
    memset(v_buf, 0, uv_align * uv_height);
    for (int i = 0; i < uv_height; i++) {
        memcpy(v_buf + i * uv_align, v + i * v_linesize, uv_width);
    }

    // 5. 更新Y纹理
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures_[0]);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, y_align); // 对齐后的行步长
    glTexSubImage2D(
        GL_TEXTURE_2D, 0, 0, 0,
        width, height, GL_RED, GL_UNSIGNED_BYTE, y_buf
    );
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    // 6. 更新U纹理
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures_[1]);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, uv_align);
    glTexSubImage2D(
        GL_TEXTURE_2D, 0, 0, 0,
        uv_width, uv_height, GL_RED, GL_UNSIGNED_BYTE, u_buf
    );
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    // 7. 更新V纹理
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, textures_[2]);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, uv_align);
    glTexSubImage2D(
        GL_TEXTURE_2D, 0, 0, 0,
        uv_width, uv_height, GL_RED, GL_UNSIGNED_BYTE, v_buf
    );
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    // 8. 释放临时缓冲区
    delete[] y_buf;
    delete[] u_buf;
    delete[] v_buf;

    // 9. 解绑纹理
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLRender::updateNV12Texture(const uint8_t* y, const uint8_t* uv, int width, int height,
                                     int y_linesize, int uv_linesize) {
    if (!textures_initialized_ || !y || !uv || width <=0 || height <=0) {
        qWarning() << "updateNV12Texture参数无效："
                   << "y=" << (void*)y << " uv=" << (void*)uv
                   << " size=" << width << "x" << height;
        return;
    }

    if (reinterpret_cast<uintptr_t>(y) < 0x100 || reinterpret_cast<uintptr_t>(uv) < 0x100) {
        qWarning() << "无效的帧数据指针（地址过小）："
                   << "y=" << (void*)y << " uv=" << (void*)uv;
        return;
    }

    // 兼容奇数分辨率
    int uv_width = (width + 1) / 2;
    int uv_height = (height + 1) / 2;

    // 内存对齐
    int y_align = alignTo4(width);
    int uv_align = alignTo4(uv_width);

    // 1. Y分量临时缓冲区（去除linesize冗余）
    uint8_t* y_buf = new uint8_t[y_align * height];
    memset(y_buf, 0, y_align * height);
    for (int i = 0; i < height; i++) {
        memcpy(y_buf + i * y_align, y + i * y_linesize, width);
    }

    // 2. UV分量临时缓冲区（NV12是RG双通道，2字节/像素）
    uint8_t* uv_buf = new uint8_t[uv_align * uv_height * 2];
    memset(uv_buf, 0, uv_align * uv_height * 2);
    for (int i = 0; i < uv_height; i++) {
        memcpy(uv_buf + i * uv_align * 2, uv + i * uv_linesize, uv_width * 2);
    }

    // 3. 更新Y纹理
    glBindTexture(GL_TEXTURE_2D, textures_[0]);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, y_align);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, y_buf);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    // 4. 更新UV纹理
    glBindTexture(GL_TEXTURE_2D, textures_[1]);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, uv_align);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, uv_width, uv_height, GL_RG, GL_UNSIGNED_BYTE, uv_buf);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    // 5. 释放缓冲区
    delete[] y_buf;
    delete[] uv_buf;

    // 6. 解绑纹理
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLRender::calculateViewport() {
    int widget_w = this->width();
    int widget_h = this->height();

    if (video_width_ <= 0 || video_height_ <= 0) {
        view_w_ = widget_w;
        view_h_ = widget_h;
        return;
    }

    // 计算视频宽高比（兼容浮点精度）
    float video_aspect = static_cast<float>(video_width_) / video_height_;
    float widget_aspect = static_cast<float>(widget_w) / widget_h;

    view_w_ = widget_w;
    view_h_ = widget_h;

    // 保持视频比例，居中显示
    if (aspect_mode_ == Qt::KeepAspectRatio) {
        if (widget_aspect > video_aspect) {
            // 窗口更宽，按高度适配
            view_w_ = static_cast<int>(widget_h * video_aspect);
            view_h_ = widget_h;
        } else {
            // 窗口更高，按宽度适配
            view_w_ = widget_w;
            view_h_ = static_cast<int>(widget_w / video_aspect);
        }
    }
}

void OpenGLRender::cleanupGL() {
    makeCurrent(); // 确保OpenGL上下文有效

    // 释放着色器
    if (yuv_program_) {
        delete yuv_program_;
        yuv_program_ = nullptr;
    }

    // 释放纹理
    if (textures_initialized_) {
        glDeleteTextures(3, textures_);
        textures_initialized_ = false;
    }

    // 释放VAO/VBO
    vao_.destroy();
    vbo_.destroy();

    doneCurrent();
    is_initialized_ = false;
}

bool OpenGLRender::initialize() {
    if (!is_initialized_) {
        makeCurrent();
        initializeOpenGLFunctions();

        if (!initShaders() || !initBuffers()) {
            qWarning() << "Failed to initialize OpenGL resources";
            doneCurrent();
            return false;
        }

        is_initialized_ = true;
        doneCurrent();
    }
    return is_initialized_;
}

void OpenGLRender::pauseRendering() {
    update(); // 暂停时刷新画面
}

void OpenGLRender::stopRendering() {
    QMutexLocker locker(&frame_mutex_);
    current_frame_.reset();
    is_initialized_ = false;
    clear(); // 清空画面
}