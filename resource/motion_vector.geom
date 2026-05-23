#version 330 core

layout(points) in;
layout(line_strip, max_vertices = 6) out;

// 输入
in vec2 vMotion[];
in float vSource[];

// 输出
out vec4 gColor;

uniform float uArrowSize = 0.02;  // 箭头大小
uniform float uLineWidth = 1.0;

// 根据方向和大小计算颜色
vec4 getDirectionColor(vec2 dir, float source) {
    float angle = atan(dir.y, dir.x);
    float hue = (angle + 3.14159) / (2.0 * 3.14159); // 0-1
    
    // 前向用蓝色调，后向用红色调
    vec3 color;
    if (source < 0.0) {
        // 前向：蓝色到青色
        color = mix(vec3(0.0, 0.5, 1.0), vec3(0.0, 1.0, 1.0), hue);
    } else {
        // 后向：红色到黄色
        color = mix(vec3(1.0, 0.0, 0.0), vec3(1.0, 1.0, 0.0), hue);
    }
    
    // 运动大小影响亮度
    float len = length(dir);
    float brightness = 0.5 + 0.5 * clamp(len / 0.1, 0.0, 1.0);
    return vec4(color * brightness, 0.8);
}

void main() {
    vec2 pos = gl_in[0].gl_Position.xy;
    vec2 motion = vMotion[0];
    float source = vSource[0];
    
    gColor = getDirectionColor(motion, source);
    
    float len = length(motion);
    if (len < 0.001) {
        return; // 太小不绘制
    }
    
    vec2 dir = normalize(motion);
    vec2 perp = vec2(-dir.y, dir.x); // 垂直方向
    
    // 1. 绘制箭头线
    gl_Position = vec4(pos, 0.0, 1.0);
    EmitVertex();
    
    gl_Position = vec4(pos + motion, 0.0, 1.0);
    EmitVertex();
    
    EndPrimitive();
    
    // 2. 绘制箭头头部
    vec2 arrowTip = pos + motion;
    vec2 arrowLeft = arrowTip - dir * uArrowSize + perp * uArrowSize * 0.5;
    vec2 arrowRight = arrowTip - dir * uArrowSize - perp * uArrowSize * 0.5;
    
    gl_Position = vec4(arrowLeft, 0.0, 1.0);
    EmitVertex();
    
    gl_Position = vec4(arrowTip, 0.0, 1.0);
    EmitVertex();
    
    gl_Position = vec4(arrowRight, 0.0, 1.0);
    EmitVertex();
    
    EndPrimitive();
}
