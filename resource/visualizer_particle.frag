#version 330 core
// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

in vec4 vColor;
out vec4 FragColor;

void main() {
    float dist = distance(gl_PointCoord, vec2(0.5, 0.5));
    if (dist > 0.5) {
        discard;
    }
    FragColor = vColor;
}
