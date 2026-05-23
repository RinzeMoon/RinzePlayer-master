#version 330 core
// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

layout(location = 0) in vec2 aPos;

uniform float uTime;
uniform float uBassLevel;
uniform float uMidLevel;
uniform float uTrebleLevel;

out vec4 vColor;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    gl_PointSize = 50.0; // 固定50像素！
    vColor = vec4(1.0, 0.0, 0.0, 1.0); // 红色！
}
