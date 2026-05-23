#version 330 core

// 输入颜色（从几何着色器）
in vec4 gColor;

// 输出
out vec4 FragColor;

void main() {
    FragColor = gColor;
}
