#version 330 core
// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

layout(location = 0) in vec2 aPos;

out vec2 vTexCoord;

void main() {
    vTexCoord = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
