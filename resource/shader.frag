#version 330 core
// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

in vec2 TexCoord;

uniform int pixFormat = -1;
uniform sampler2D yTex;
uniform sampler2D uTex;
uniform sampler2D vTex;
uniform sampler2D aTex;
uniform sampler2D subTex;
uniform bool haveSubTex = false;
uniform bool showSub = true;
uniform float brightness = 0.0;
uniform float contrast = 1.0;
uniform float saturation = 1.0;
uniform int colorSpace = 0; // 0=BT.601, 1=BT.709, 2=BT.2020, 3=Dolby Vision
uniform bool fullRange = false;

out vec4 FragColor;

// YUV to RGB conversion functions
vec3 yuv2rgb601(float y, float u, float v) {
    y = y * 1.1643835616;
    return vec3(
        y + 1.5960264901 * v - 0.8742015846,
        y - 0.3917622901 * u - 0.8129676469 * v + 0.5316678233,
        y + 2.0172321429 * u - 1.0856349101
    );
}

vec3 yuv2rgb709(float y, float u, float v) {
    y = y * 1.1643835616;
    return vec3(
        y + 1.7927410714 * v - 0.9729450750,
        y - 0.2132486143 * u - 0.5329093286 * v + 0.3014826655,
        y + 2.1124017857 * u - 1.1334022179
    );
}

vec3 yuv2rgb2020(float y, float u, float v) {
    y = y * 1.1643835616;
    return vec3(
        y + 1.6786746170 * v - 0.9156862745,
        y - 0.1873264824 * u - 0.6504243272 * v + 0.3474509804,
        y + 2.1417717718 * u - 1.1482352941
    );
}

// PQ EOTF (Perceptual Quantizer Electro-Optical Transfer Function)
float pqEOTF(float x) {
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    
    float xp = pow(max(x, 0.0), 1.0 / m2);
    float num = max(xp - c1, 0.0);
    float den = max(c2 - c3 * xp, 1e-9);
    return pow(num / den, 1.0 / m1);
}

// Dolby Vision ICtCp to RGB conversion
vec3 ictcp2rgb(vec3 ictcp) {
    // ICtCp -> LMS
    mat3 ictcp2lms = mat3(
        1.0, 1.0, 1.0,
        0.0094, -0.111, 0.8462,
        0.7347, -0.6836, -0.0511
    );
    vec3 lms = ictcp2lms * ictcp;
    
    // Apply PQ EOTF
    lms.r = pqEOTF(lms.r);
    lms.g = pqEOTF(lms.g);
    lms.b = pqEOTF(lms.b);
    
    // LMS -> BT.2020 RGB
    mat3 lms2rgb2020 = mat3(
        1.6489, -0.3476, -0.2535,
        -0.6797, 1.6484, 0.0032,
        0.0254, -0.0748, 1.0494
    );
    vec3 rgb2020 = lms2rgb2020 * lms;
    
    // BT.2020 RGB -> sRGB
    mat3 rgb2020torgb = mat3(
        1.6605, -0.1246, -0.0182,
        -0.5876, 1.1329, -0.1006,
        -0.0728, -0.0083, 1.1187
    );
    vec3 rgb = rgb2020torgb * rgb2020;
    
    // Simple tone mapping
    rgb = rgb / (rgb + vec3(1.0));
    
    return rgb;
}

void main() {
    vec4 color;
    if (pixFormat == 0) { // RGB_PACKED
        color = vec4(texture(yTex, TexCoord).rgb, 1.0);
    } else if (pixFormat == 1) { // RGBA_PACKED
        color = texture(yTex, TexCoord);
    } else if (pixFormat == 2 || pixFormat == 3) { // RGB_PLANAR / RGBA_PLANAR
        float r = texture(yTex, TexCoord).r;
        float g = texture(uTex, TexCoord).r;
        float b = texture(vTex, TexCoord).r;
        float a = (pixFormat == 3) ? texture(aTex, TexCoord).r : 1.0;
        color = vec4(r, g, b, a);
    } else if (pixFormat == 4 || pixFormat == 5) { // Y / YA
        float y = texture(yTex, TexCoord).r;
        float a = (pixFormat == 5) ? texture(uTex, TexCoord).r : 1.0;
        color = vec4(y, y, y, a);
    } else if (pixFormat == 6 || pixFormat == 7) { // YUV / YUVA
        float y = texture(yTex, TexCoord).r;
        float u = texture(uTex, TexCoord).r;
        float v = texture(vTex, TexCoord).r;
        float a = (pixFormat == 7) ? texture(aTex, TexCoord).r : 1.0;
        
        // Convert from full range to limited range if needed
        if (fullRange) {
            y = y * (235.0 / 255.0) + 16.0 / 255.0;
            u = u * (224.0 / 255.0) + 16.0 / 255.0;
            v = v * (224.0 / 255.0) + 16.0 / 255.0;
        }
        
        vec3 rgb;
        if (colorSpace == 0) {
            rgb = yuv2rgb601(y, u, v);
        } else if (colorSpace == 1) {
            rgb = yuv2rgb709(y, u, v);
        } else if (colorSpace == 2) {
            rgb = yuv2rgb2020(y, u, v);
        } else { // Dolby Vision
            // First decode as BT.2020
            vec3 yuv = vec3(y, u, v);
            vec3 yuv_norm = vec3(
                (yuv.x - 16.0/255.0) * (255.0/(235.0-16.0)),
                (yuv.y - 16.0/255.0) * (255.0/(240.0-16.0)),
                (yuv.z - 16.0/255.0) * (255.0/(240.0-16.0))
            );
            
            // Convert YUV BT.2020 to ICtCp
            float y_lin = yuv_norm.x;
            float cb = yuv_norm.y - 0.5;
            float cr = yuv_norm.z - 0.5;
            
            // YCbCr BT.2020 to RGB BT.2020
            float r_2020 = y_lin + 1.4746 * cr;
            float g_2020 = y_lin - 0.16455 * cb - 0.57135 * cr;
            float b_2020 = y_lin + 1.8814 * cb;
            
            // Clamp values
            vec3 rgb_2020 = clamp(vec3(r_2020, g_2020, b_2020), 0.0, 1.0);
            
            // Apply gamma correction (simple version)
            rgb_2020 = pow(rgb_2020, vec3(2.2));
            
            // Simple color correction for Dolby Vision look
            rgb = rgb_2020;
            
            // Fallback to BT.709 conversion if the above is not good
            // Try both BT.2020 and BT.709 and choose the better one
            vec3 rgb_a = yuv2rgb2020(y, u, v);
            vec3 rgb_b = yuv2rgb709(y, u, v);
            
            // Mix them a bit, prioritize BT.709
            rgb = mix(rgb_a, rgb_b, 0.5);
        }
        color = vec4(rgb, a);
    } else {
        color = vec4(1.0, 0.0, 0.0, 1.0);
    }

    // Apply brightness
    color.rgb += brightness;
    color.rgb = clamp(color.rgb, 0.0, 1.0);

    // Apply contrast
    color.rgb = ((color.rgb - 0.5) * contrast) + 0.5;
    color.rgb = clamp(color.rgb, 0.0, 1.0);

    // Apply saturation
    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    color.rgb = mix(vec3(gray), color.rgb, saturation);
    color.rgb = clamp(color.rgb, 0.0, 1.0);

    if (haveSubTex && showSub) {
        vec4 subColor = texture(subTex, TexCoord);
        color = mix(color, subColor, subColor.a);
    }

    FragColor = color;
}
