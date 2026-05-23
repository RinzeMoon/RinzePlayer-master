#version 330 core

// 输入：位置和运动向量数据
layout(location = 0) in vec2 aPos;          // 位置（标准化设备坐标）
layout(location = 1) in vec2 aMotion;      // 运动向量（像素坐标）
layout(location = 2) in float aSource;     // 来源 (-1前向, 1后向)

// 输出到几何着色器
out vec2 vMotion;
out float vSource;

uniform float uScale = 0.1;     // 箭头缩放比例
uniform vec2 uResolution;       // 视频分辨率

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    
    // 将运动向量从像素坐标转换为标准化设备坐标
    vMotion = aMotion / uResolution * uScale;
    vSource = aSource;
}
