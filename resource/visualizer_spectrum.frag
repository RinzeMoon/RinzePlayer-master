#version 330 core
// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

in vec2 vTexCoord;

uniform float uTime;
uniform float uBassLevel;
uniform float uMidLevel;
uniform float uTrebleLevel;
uniform float uSpectrum[256];
uniform vec2 uResolution;

out vec4 FragColor;

#define PI 3.14159265359

float hash(float n) {
    return fract(sin(n) * 43758.5453123);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    
    float a = hash(i.x + i.y * 57.0);
    float b = hash(i.x + 1.0 + i.y * 57.0);
    float c = hash(i.x + (i.y + 1.0) * 57.0);
    float d = hash(i.x + 1.0 + (i.y + 1.0) * 57.0);
    
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float f = 0.0;
    f += 0.5000 * noise(p); p *= 2.02;
    f += 0.2500 * noise(p); p *= 2.03;
    f += 0.1250 * noise(p); p *= 2.01;
    f += 0.0625 * noise(p);
    return f / 0.9375;
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    vec2 uv = vTexCoord;
    vec2 aspect = vec2(uResolution.x / uResolution.y, 1.0);
    vec2 pos = (uv - 0.5) * aspect;
    
    float totalEnergy = uBassLevel * 0.4 + uMidLevel * 0.35 + uTrebleLevel * 0.25;
    
    float bandIndex = uv.x * 255.0;
    int idxLow = int(floor(bandIndex));
    int idxHigh = idxLow + 1;
    float t = fract(bandIndex);
    
    float specLow = uSpectrum[clamp(idxLow, 0, 255)];
    float specHigh = uSpectrum[clamp(idxHigh, 0, 255)];
    float spectrum = mix(specLow, specHigh, t);
    
    float wave = sin(uv.x * 20.0 + uTime * 2.0 + spectrum * 5.0) * 0.1 * spectrum;
    wave += sin(uv.x * 40.0 - uTime * 3.0 + spectrum * 8.0) * 0.05 * spectrum;
    
    float fbmNoise = fbm(pos * 3.0 + uTime * 0.2);
    float distortion = (fbmNoise - 0.5) * 0.2 * totalEnergy;
    
    float centerDist = length(pos);
    float glow = exp(-centerDist * 3.0) * (0.5 + totalEnergy * 0.5);
    
    float waveY = 0.5 + wave + distortion;
    float barHeight = spectrum * 0.4 + uBassLevel * 0.2;
    float bar = smoothstep(uv.y, uv.y + 0.01, waveY + barHeight);
    bar -= smoothstep(uv.y, uv.y + 0.005, waveY);
    
    float spectrumBar = smoothstep(uv.y, uv.y + 0.002, 0.5 + spectrum * 0.4);
    spectrumBar *= smoothstep(uv.y + 0.002, uv.y, 0.5);
    
    float hue = uv.x * 0.3 + uTime * 0.1 + uBassLevel * 0.2;
    vec3 barColor = hsv2rgb(vec3(hue, 0.8, 0.5 + spectrum * 0.5));
    vec3 glowColor = hsv2rgb(vec3(hue + 0.5, 0.6, 0.3 + totalEnergy * 0.4));
    
    vec3 color = vec3(0.02);
    color += barColor * bar * 1.5;
    color += glowColor * glow * 0.8;
    color += barColor * spectrumBar * 0.5;
    
    float rings = sin(centerDist * 20.0 - uTime * 2.0 + uBassLevel * 3.0) * 0.5 + 0.5;
    rings *= exp(-centerDist * 2.0) * totalEnergy * 0.3;
    color += glowColor * rings;
    
    float vignette = 1.0 - dot(pos, pos) * 0.5;
    color *= vignette;
    
    FragColor = vec4(color, 1.0);
}
