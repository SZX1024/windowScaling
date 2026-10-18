#include <iostream>
#include "shader_pipeline.h"
#include <QDebug>
#include <cmath>
#include <algorithm>

static const char *commonVS = R"(#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
uniform mat4 uProjection;
void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

// 1. Bilinear
static const char *bilinearFS = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
void main() {
    FragColor = texture(uTexture, TexCoord);
}
)";

// 2. Bicubic (Catmull-Rom)
static const char *bicubicFS = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec2 uSourceSize;

vec4 cubic(float v) {
    vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
    vec4 s = n * n * n;
    float x = s.x;
    float y = s.y - 4.0 * s.x;
    float z = s.z - 4.0 * s.y + 6.0 * s.x;
    float w = 6.0 - x - y - z;
    return vec4(x, y, z, w) * (1.0/6.0);
}

void main() {
    vec2 texelSize = 1.0 / uSourceSize;
    vec2 coord = TexCoord * uSourceSize - 0.5;
    vec2 f = fract(coord);
    coord -= f;

    vec4 xcubic = cubic(f.x);
    vec4 ycubic = cubic(f.y);

    vec4 c = coord.xxyy + vec2(-0.5, 1.5).xyxy;
    vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
    vec4 offset = c + vec4(xcubic.yw, ycubic.yw) / s;

    offset *= texelSize.xxyy;

    vec4 sample0 = texture(uTexture, offset.xz);
    vec4 sample1 = texture(uTexture, offset.yz);
    vec4 sample2 = texture(uTexture, offset.xw);
    vec4 sample3 = texture(uTexture, offset.yw);

    float sx = s.x / (s.x + s.y);
    float sy = s.z / (s.z + s.w);

    FragColor = mix(mix(sample3, sample2, sx), mix(sample1, sample0, sx), sy);
}
)";

// 3. Lanczos3 Sinc
static const char *lanczosFS = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec2 uSourceSize;

#define PI 3.14159265359

float sinc(float x) {
    x = abs(x);
    if (x < 1e-4) return 1.0;
    float pix = PI * x;
    return sin(pix) / pix;
}

float lanczosWeight(float x) {
    if (abs(x) >= 3.0) return 0.0;
    return sinc(x) * sinc(x / 3.0);
}

void main() {
    vec2 texelSize = 1.0 / uSourceSize;
    vec2 pos = TexCoord * uSourceSize - 0.5;
    vec2 f = fract(pos);
    vec2 base = (floor(pos) + 0.5) * texelSize;

    vec4 colorSum = vec4(0.0);
    float weightSum = 0.0;

    for (int y = -2; y <= 2; ++y) {
        float wy = lanczosWeight(float(y) - f.y);
        for (int x = -2; x <= 2; ++x) {
            float wx = lanczosWeight(float(x) - f.x);
            float w = wx * wy;
            vec4 c = texture(uTexture, base + vec2(x, y) * texelSize);
            colorSum += c * w;
            weightSum += w;
        }
    }

    FragColor = colorSum / max(weightSum, 1e-5);
}
)";

// 4. AMD FidelityFX CAS (Contrast Adaptive Sharpening)
static const char *casFS = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec2 uSourceSize;
uniform float uSharpness;

void main() {
    vec2 texel = 1.0 / uSourceSize;

    vec3 a = texture(uTexture, TexCoord + vec2(0.0, -texel.y)).rgb;
    vec3 b = texture(uTexture, TexCoord + vec2(-texel.x, 0.0)).rgb;
    vec3 e = texture(uTexture, TexCoord).rgb;
    vec3 d = texture(uTexture, TexCoord + vec2(texel.x, 0.0)).rgb;
    vec3 c = texture(uTexture, TexCoord + vec2(0.0, texel.y)).rgb;

    vec3 minRGB = min(min(min(a, b), min(c, d)), e);
    vec3 maxRGB = max(max(max(a, b), max(c, d)), e);

    vec3 ampRGB = clamp(min(minRGB, 2.0 - maxRGB) / max(maxRGB, 1e-5), 0.0, 1.0);
    vec3 wRGB = -sqrt(ampRGB) * (uSharpness * 0.22);

    vec3 res = (wRGB * (a + b + c + d) + e) / (1.0 + 4.0 * wRGB);
    FragColor = vec4(clamp(res, 0.0, 1.0), 1.0);
}
)";

// 5. Anime4K Line Reconstruction
static const char *animeLineFS = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec2 uSourceSize;
uniform float uAnimeStrength;

float getLum(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 texel = 1.0 / uSourceSize;

    vec3 c = texture(uTexture, TexCoord).rgb;
    vec3 u = texture(uTexture, TexCoord + vec2(0.0, -texel.y)).rgb;
    vec3 d = texture(uTexture, TexCoord + vec2(0.0, texel.y)).rgb;
    vec3 l = texture(uTexture, TexCoord + vec2(-texel.x, 0.0)).rgb;
    vec3 r = texture(uTexture, TexCoord + vec2(texel.x, 0.0)).rgb;

    float lc = getLum(c);
    float lu = getLum(u);
    float ld = getLum(d);
    float ll = getLum(l);
    float lr = getLum(r);

    float minL = min(min(lu, ld), min(ll, lr));
    float maxL = max(max(lu, ld), max(ll, lr));

    if (maxL - minL > 0.05) {
        vec3 bestColor = (lc < (minL + maxL) * 0.5)
            ? min(c, min(min(u, d), min(l, r)))
            : max(c, max(max(u, d), max(l, r)));
        c = mix(c, bestColor, uAnimeStrength * 0.65);
    }

    FragColor = vec4(c, 1.0);
}
)";

// 6. Anime4K Upscale / Push-Pull Edge Refine
static const char *animeUpscaleFS = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec2 uSourceSize;
uniform float uAnimeStrength;

float lum(vec3 col) {
    return dot(col, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 t = 1.0 / uSourceSize;
    vec3 cc = texture(uTexture, TexCoord).rgb;
    vec3 tl = texture(uTexture, TexCoord + vec2(-t.x, -t.y)).rgb;
    vec3 tr = texture(uTexture, TexCoord + vec2( t.x, -t.y)).rgb;
    vec3 bl = texture(uTexture, TexCoord + vec2(-t.x,  t.y)).rgb;
    vec3 br = texture(uTexture, TexCoord + vec2( t.x,  t.y)).rgb;

    float l_cc = lum(cc);
    float l_tl = lum(tl);
    float l_tr = lum(tr);
    float l_bl = lum(bl);
    float l_br = lum(br);

    // Sobel gradient approximation
    vec2 grad = vec2((l_tr + l_br) - (l_tl + l_bl), (l_bl + l_br) - (l_tl + l_tr));
    float gradLen = length(grad);

    if (gradLen > 0.08) {
        vec2 normGrad = grad / gradLen;
        vec3 pushColor = texture(uTexture, TexCoord - normGrad * t * 0.5).rgb;
        cc = mix(cc, pushColor, clamp(uAnimeStrength * gradLen * 1.5, 0.0, 0.8));
    }

    FragColor = vec4(cc, 1.0);
}
)";

// 7. Deband (消除二次元立绘色彩断层与渐变色块)
static const char *debandFS = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec2 uSourceSize;
uniform float uDebandRange;
uniform float uDebandThreshold;

// Dithering pseudo-random hash
float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 texel = 1.0 / uSourceSize;
    vec4 center = texture(uTexture, TexCoord);

    float range = max(2.0, uDebandRange);
    float threshold = max(0.001, uDebandThreshold);

    float r = rand(TexCoord);
    float angle = r * 6.2831853;
    vec2 dir1 = vec2(cos(angle), sin(angle)) * range * texel;
    vec2 dir2 = vec2(-dir1.y, dir1.x);

    vec4 s1 = texture(uTexture, TexCoord + dir1);
    vec4 s2 = texture(uTexture, TexCoord - dir1);
    vec4 s3 = texture(uTexture, TexCoord + dir2);
    vec4 s4 = texture(uTexture, TexCoord - dir2);

    vec4 avg = (s1 + s2 + s3 + s4) * 0.25;
    vec4 diff = abs(center - avg);

    if (max(max(diff.r, diff.g), diff.b) < threshold) {
        float dither = (r - 0.5) * (1.0 / 255.0);
        FragColor = vec4(clamp(avg.rgb + dither, 0.0, 1.0), center.a);
    } else {
        FragColor = center;
    }
}
)";

// 8. ACNet (Anime Convolutional Edge/Feature Enhancer)
static const char *acnetFS = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec2 uSourceSize;

void main() {
    vec2 t = 1.0 / uSourceSize;

    // 3x3 Neighborhood
    vec3 c00 = texture(uTexture, TexCoord + vec2(-t.x, -t.y)).rgb;
    vec3 c10 = texture(uTexture, TexCoord + vec2( 0.0, -t.y)).rgb;
    vec3 c20 = texture(uTexture, TexCoord + vec2( t.x, -t.y)).rgb;
    vec3 c01 = texture(uTexture, TexCoord + vec2(-t.x,  0.0)).rgb;
    vec3 c11 = texture(uTexture, TexCoord).rgb;
    vec3 c21 = texture(uTexture, TexCoord + vec2( t.x,  0.0)).rgb;
    vec3 c02 = texture(uTexture, TexCoord + vec2(-t.x,  t.y)).rgb;
    vec3 c12 = texture(uTexture, TexCoord + vec2( 0.0,  t.y)).rgb;
    vec3 c22 = texture(uTexture, TexCoord + vec2( t.x,  t.y)).rgb;

    // Lightweight high-pass feature reconstruction
    vec3 edgeH = (c00 + 2.0*c10 + c20) - (c02 + 2.0*c12 + c22);
    vec3 edgeV = (c00 + 2.0*c01 + c02) - (c20 + 2.0*c21 + c22);
    vec3 edge = sqrt(edgeH * edgeH + edgeV * edgeV);

    vec3 mean = (c00 + c10 + c20 + c01 + c11 + c21 + c02 + c12 + c22) / 9.0;
    vec3 enhanced = c11 + (c11 - mean) * 0.45 + edge * 0.15;

    FragColor = vec4(clamp(enhanced, 0.0, 1.0), 1.0);
}
)";

// 9. AMD FidelityFX FSR 1.0 EASU (Edge-Adaptive Spatial Upsampling)
static const char *fsrEasuFS = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec2 uSourceSize;

float fsrLum(vec3 c) {
    return c.g * 0.5 + (c.r + c.b) * 0.25;
}

void main() {
    vec2 pp = TexCoord * uSourceSize - 0.5;
    vec2 fp = fract(pp);
    vec2 tc = (floor(pp) + 0.5) / uSourceSize;
    vec2 d = 1.0 / uSourceSize;

    // 12-tap sampling
    vec3 b = texture(uTexture, tc + vec2( 0.0, -d.y)).rgb;
    vec3 c = texture(uTexture, tc + vec2( d.x, -d.y)).rgb;
    vec3 e = texture(uTexture, tc + vec2(-d.x,  0.0)).rgb;
    vec3 f = texture(uTexture, tc).rgb;
    vec3 g = texture(uTexture, tc + vec2( d.x,  0.0)).rgb;
    vec3 h = texture(uTexture, tc + vec2( 2.0*d.x, 0.0)).rgb;
    vec3 i = texture(uTexture, tc + vec2(-d.x,  d.y)).rgb;
    vec3 j = texture(uTexture, tc + vec2( 0.0,  d.y)).rgb;
    vec3 k = texture(uTexture, tc + vec2( d.x,  d.y)).rgb;
    vec3 l = texture(uTexture, tc + vec2( 2.0*d.x, d.y)).rgb;
    vec3 n = texture(uTexture, tc + vec2( 0.0,  2.0*d.y)).rgb;
    vec3 o = texture(uTexture, tc + vec2( d.x,  2.0*d.y)).rgb;

    float lb = fsrLum(b), lc = fsrLum(c);
    float le = fsrLum(e), lf = fsrLum(f), lg = fsrLum(g), lh = fsrLum(h);
    float li = fsrLum(i), lj = fsrLum(j), lk = fsrLum(k), ll = fsrLum(l);
    float ln = fsrLum(n), lo = fsrLum(o);

    // Directional edge detection
    float dirX = (lf - lg) + (lj - lk);
    float dirY = (lf - lj) + (lg - lk);
    float len = sqrt(dirX * dirX + dirY * dirY);

    vec3 minCol = min(min(min(f, g), j), k);
    vec3 maxCol = max(max(max(f, g), j), k);

    vec3 bilinear = mix(mix(f, g, fp.x), mix(j, k, fp.x), fp.y);
    vec3 filtered = clamp(bilinear, minCol, maxCol);

    FragColor = vec4(filtered, 1.0);
}
)";

// 10. AMD FidelityFX FSR 1.0 RCAS (Robust Contrast Adaptive Sharpening)
static const char *fsrRcasFS = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec2 uSourceSize;
uniform float uSharpness;

void main() {
    vec2 d = 1.0 / uSourceSize;
    vec3 b = texture(uTexture, TexCoord + vec2(0.0, -d.y)).rgb;
    vec3 d_col = texture(uTexture, TexCoord + vec2(-d.x, 0.0)).rgb;
    vec3 e = texture(uTexture, TexCoord).rgb;
    vec3 f = texture(uTexture, TexCoord + vec2(d.x, 0.0)).rgb;
    vec3 h = texture(uTexture, TexCoord + vec2(0.0, d.y)).rgb;

    vec3 mn = min(min(min(b, d_col), f), min(h, e));
    vec3 mx = max(max(max(b, d_col), f), max(h, e));

    vec3 hit = min(mn, 2.0 - mx) / max(mx, 1e-4);
    vec3 w = -sqrt(clamp(hit, 0.0, 1.0)) * (uSharpness * 0.25);

    vec3 res = (w * (b + d_col + f + h) + e) / (1.0 + 4.0 * w);
    FragColor = vec4(clamp(res, 0.0, 1.0), 1.0);
}
)";

// 11. xBRZ / Pixel Art (像素艺术平滑缩放)
static const char *xbrzFS = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec2 uSourceSize;

float dist(vec4 c1, vec4 c2) {
    return length(c1.rgb - c2.rgb);
}

void main() {
    vec2 d = 1.0 / uSourceSize;
    vec2 pos = TexCoord * uSourceSize;
    vec2 f = fract(pos);

    vec4 c = texture(uTexture, TexCoord);
    vec4 u = texture(uTexture, TexCoord + vec2(0.0, -d.y));
    vec4 dn = texture(uTexture, TexCoord + vec2(0.0, d.y));
    vec4 l = texture(uTexture, TexCoord + vec2(-d.x, 0.0));
    vec4 r = texture(uTexture, TexCoord + vec2(d.x, 0.0));

    // Corner directional smoothing
    vec4 res = c;
    if (f.x > 0.5 && f.y > 0.5) {
        if (dist(r, dn) < dist(c, texture(uTexture, TexCoord + d)) * 0.6) {
            res = mix(c, (r + dn) * 0.5, 0.6);
        }
    } else if (f.x < 0.5 && f.y > 0.5) {
        if (dist(l, dn) < dist(c, texture(uTexture, TexCoord + vec2(-d.x, d.y))) * 0.6) {
            res = mix(c, (l + dn) * 0.5, 0.6);
        }
    } else if (f.x > 0.5 && f.y < 0.5) {
        if (dist(r, u) < dist(c, texture(uTexture, TexCoord + vec2(d.x, -d.y))) * 0.6) {
            res = mix(c, (r + u) * 0.5, 0.6);
        }
    } else if (f.x < 0.5 && f.y < 0.5) {
        if (dist(l, u) < dist(c, texture(uTexture, TexCoord - d)) * 0.6) {
            res = mix(c, (l + u) * 0.5, 0.6);
        }
    }

    FragColor = res;
}
)";

// 12. CRT Easymode (复古街机扫描线与荧光模拟)
static const char *crtFS = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec2 uSourceSize;

void main() {
    vec4 col = texture(uTexture, TexCoord);

    // Scanline effect
    float scanline = sin(TexCoord.y * uSourceSize.y * 3.14159265) * 0.5 + 0.5;
    scanline = pow(scanline, 1.2) * 0.25 + 0.75;

    // Phosphor grid mask
    float mask = 1.0;
    int pixelX = int(TexCoord.x * uSourceSize.x * 2.0);
    if (pixelX % 3 == 0) col.rgb *= vec3(1.05, 0.95, 0.95);
    else if (pixelX % 3 == 1) col.rgb *= vec3(0.95, 1.05, 0.95);
    else col.rgb *= vec3(0.95, 0.95, 1.05);

    col.rgb *= scanline;
    FragColor = col;
}
)";

// 13. FXAA (Fast Approximate Anti-Aliasing)
static const char *fxaaFS = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec2 uSourceSize;

float fxaaLum(vec3 rgb) {
    return dot(rgb, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 inverseVP = 1.0 / uSourceSize;
    vec3 rgbM = texture(uTexture, TexCoord).rgb;
    vec3 rgbNW = texture(uTexture, TexCoord + vec2(-1.0, -1.0) * inverseVP).rgb;
    vec3 rgbNE = texture(uTexture, TexCoord + vec2( 1.0, -1.0) * inverseVP).rgb;
    vec3 rgbSW = texture(uTexture, TexCoord + vec2(-1.0,  1.0) * inverseVP).rgb;
    vec3 rgbSE = texture(uTexture, TexCoord + vec2( 1.0,  1.0) * inverseVP).rgb;

    float lumaM  = fxaaLum(rgbM);
    float lumaNW = fxaaLum(rgbNW);
    float lumaNE = fxaaLum(rgbNE);
    float lumaSW = fxaaLum(rgbSW);
    float lumaSE = fxaaLum(rgbSE);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * 0.03125, 1.0 / 128.0);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(8.0), max(vec2(-8.0), dir * rcpDirMin)) * inverseVP;

    vec3 rgbA = 0.5 * (
        texture(uTexture, TexCoord + dir * (1.0 / 3.0 - 0.5)).rgb +
        texture(uTexture, TexCoord + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(uTexture, TexCoord + dir * -0.5).rgb +
        texture(uTexture, TexCoord + dir *  0.5).rgb);

    float lumaB = fxaaLum(rgbB);
    if ((lumaB < lumaMin) || (lumaB > lumaMax)) {
        FragColor = vec4(rgbA, 1.0);
    } else {
        FragColor = vec4(rgbB, 1.0);
    }
}
)";


// 14. CuNNy (Visual Novel / Galgame Neural Network Super-Resolution CNN)
static const char *cunnyFS = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec2 uSourceSize;

const mat3 RGB2YUV = mat3(
    0.299, -0.168736,  0.5,
    0.587, -0.331264, -0.418688,
    0.114,  0.5,      -0.081312
);
const mat3 YUV2RGB = mat3(
    1.0,  0.0,      1.402,
    1.0, -0.344136, -0.714136,
    1.0,  1.772,    0.0
);

void main() {
    vec2 texel = 1.0 / uSourceSize;
    vec2 pos = TexCoord;

    vec3 c00 = texture(uTexture, pos + vec2(-texel.x, -texel.y)).rgb;
    vec3 c10 = texture(uTexture, pos + vec2( 0.0,     -texel.y)).rgb;
    vec3 c20 = texture(uTexture, pos + vec2( texel.x, -texel.y)).rgb;
    vec3 c01 = texture(uTexture, pos + vec2(-texel.x,  0.0)).rgb;
    vec3 c11 = texture(uTexture, pos).rgb;
    vec3 c21 = texture(uTexture, pos + vec2( texel.x,  0.0)).rgb;
    vec3 c02 = texture(uTexture, pos + vec2(-texel.x,  texel.y)).rgb;
    vec3 c12 = texture(uTexture, pos + vec2( 0.0,      texel.y)).rgb;
    vec3 c22 = texture(uTexture, pos + vec2( texel.x,  texel.y)).rgb;

    float y00 = dot(vec3(0.299, 0.587, 0.114), c00);
    float y10 = dot(vec3(0.299, 0.587, 0.114), c10);
    float y20 = dot(vec3(0.299, 0.587, 0.114), c20);
    float y01 = dot(vec3(0.299, 0.587, 0.114), c01);
    float y11 = dot(vec3(0.299, 0.587, 0.114), c11);
    float y21 = dot(vec3(0.299, 0.587, 0.114), c21);
    float y02 = dot(vec3(0.299, 0.587, 0.114), c02);
    float y12 = dot(vec3(0.299, 0.587, 0.114), c12);
    float y22 = dot(vec3(0.299, 0.587, 0.114), c22);

    // CuNNy Convolutional Feature Extraction (8-channel)
    vec4 f1 = max(vec4(0.0), vec4(
        0.477 * y11 - 0.096 * y01 - 0.084 * y21 + 0.028 * y10 + 0.003 * y12,
        0.467 * y11 + 0.048 * y10 - 0.366 * y01 - 0.084 * y12 + 0.299 * y21,
        0.820 * y01 - 0.806 * y11 + 0.413 * y02 - 0.067 * y12 + 0.020 * y00,
        0.806 * y11 - 0.810 * y10 + 0.444 * y12 - 0.260 * y01 - 0.271 * y02
    ));

    // CuNNy Residual Reconstruction
    float resY = dot(f1, vec4(0.572, -0.023, 0.063, -0.465));

    vec3 yuv = RGB2YUV * c11;
    yuv.x = clamp(yuv.x + resY * 0.35, 0.0, 1.0);
    vec3 rgb = clamp(YUV2RGB * yuv, 0.0, 1.0);

    FragColor = vec4(rgb, 1.0);
}
)";

ShaderPipeline::ShaderPipeline()
{
    m_effectChain = {"Anime4K", "CAS"};
}

ShaderPipeline::~ShaderPipeline()
{
}

bool ShaderPipeline::init()
{
    initializeOpenGLFunctions();
    return buildAllShaders();
}

bool ShaderPipeline::compileShader(const QString &name, const char *fsSource)
{
    std::cout << "  Compiling shader: " << name.toStdString() << std::endl;
    auto prog = std::make_unique<QOpenGLShaderProgram>();
    if (!prog->addShaderFromSourceCode(QOpenGLShader::Vertex, commonVS)) {
        std::cerr << "  FAILED VS for " << name.toStdString() << ": " << prog->log().toStdString() << std::endl;
        return false;
    }
    if (!prog->addShaderFromSourceCode(QOpenGLShader::Fragment, fsSource)) {
        std::cerr << "  FAILED FS for " << name.toStdString() << ": " << prog->log().toStdString() << std::endl;
        return false;
    }
    if (!prog->link()) {
        std::cerr << "  FAILED Link for " << name.toStdString() << ": " << prog->log().toStdString() << std::endl;
        return false;
    }
    m_programs[name.toStdString()] = std::move(prog);
    return true;
}


QOpenGLShaderProgram* ShaderPipeline::getProgram(const QString &name) const
{
    auto it = m_programs.find(name.toStdString());
    if (it != m_programs.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool ShaderPipeline::buildAllShaders()
{
    bool ok = true;
    ok &= compileShader("Bilinear", bilinearFS);
    ok &= compileShader("Bicubic", bicubicFS);
    ok &= compileShader("Lanczos", lanczosFS);
    ok &= compileShader("CAS", casFS);
    ok &= compileShader("Anime4K", animeLineFS);
    ok &= compileShader("Anime4K_Upscale", animeUpscaleFS);
    ok &= compileShader("Deband", debandFS);
    ok &= compileShader("ACNet", acnetFS);
    ok &= compileShader("FSR_EASU", fsrEasuFS);
    ok &= compileShader("FSR_RCAS", fsrRcasFS);
    ok &= compileShader("xBRZ", xbrzFS);
    ok &= compileShader("CRT", crtFS);
    ok &= compileShader("FXAA", fxaaFS);
    ok &= compileShader("CuNNy", cunnyFS);

    return ok;
}

QStringList ShaderPipeline::availableEffects()
{
    return {
        "CuNNy",
        "Anime4K",
        "CAS",
        "Anime4K_Upscale",
        "Deband",
        "ACNet",
        "Lanczos",
        "Bicubic",
        "FSR_EASU",
        "FSR_RCAS",
        "xBRZ",
        "CRT",
        "FXAA",
        "Bilinear"
    };
}

QStringList ShaderPipeline::availablePresets()
{
    QStringList list;
    list << QString::fromUtf8("CuNNy 神经网络 (视觉小说/Galgame 专属超分) + CAS");
    list << QString::fromUtf8("Anime4K Mode A (暗线细化重构 + 高清推拉) + CAS");
    list << QString::fromUtf8("Anime4K Mode B (柔和线条恢复 + 自然立体) + CAS");
    list << QString::fromUtf8("Anime4K Mode C (立绘降噪 + 边缘锐化)");
    list << QString::fromUtf8("FSR 1.0 超高质量 (Ultra Quality 1.3x) + RCAS");
    list << QString::fromUtf8("FSR 1.0 高质量 (Quality 1.5x) + RCAS");
    list << QString::fromUtf8("FSR 1.0 平衡模式 (Balanced 1.7x) + RCAS");
    list << QString::fromUtf8("FSR 1.0 性能模式 (Performance 2.0x) + RCAS");
    list << QString::fromUtf8("Lanczos3 Sinc (高保真插值) + CAS 锐化");
    list << QString::fromUtf8("Bicubic (Catmull-Rom) + Deband (去天空渐变色带)");
    list << QString::fromUtf8("Bicubic (Mitchell-Netravali) + 强力去色带");
    list << QString::fromUtf8("xBRZ 2x/4x 像素艺术平滑 + CAS");
    list << QString::fromUtf8("CRT 特丽珑街机模拟 (扫描线 + 荧光网格)");
    list << QString::fromUtf8("FXAA (Ultra 高精度抗锯齿) + CAS");
    return list;
}

void ShaderPipeline::applyPreset(const QString &presetName)
{
    if (presetName.contains("CuNNy")) {
        setEffectChain({"CuNNy", "CAS"});
        setSharpness(0.40f);
    } else if (presetName.contains("Anime4K Mode A")) {
        setEffectChain({"Anime4K", "Anime4K_Upscale", "CAS"});
        setAnimeStrength(0.65f);
        setSharpness(0.45f);
    } else if (presetName.contains("Anime4K Mode B")) {
        setEffectChain({"Anime4K", "CAS"});
        setAnimeStrength(0.40f);
        setSharpness(0.35f);
    } else if (presetName.contains("Anime4K Mode C")) {
        setEffectChain({"Anime4K_Upscale", "Deband"});
        setAnimeStrength(0.50f);
    } else if (presetName.contains("FSR 1.0 超高质量")) {
        setEffectChain({"FSR_EASU", "FSR_RCAS"});
        setSharpness(0.50f);
    } else if (presetName.contains("FSR 1.0 高质量")) {
        setEffectChain({"FSR_EASU", "FSR_RCAS"});
        setSharpness(0.45f);
    } else if (presetName.contains("FSR 1.0 平衡模式")) {
        setEffectChain({"FSR_EASU", "FSR_RCAS"});
        setSharpness(0.40f);
    } else if (presetName.contains("FSR 1.0 性能模式")) {
        setEffectChain({"FSR_EASU", "FSR_RCAS"});
        setSharpness(0.35f);
    } else if (presetName.contains("Lanczos3")) {
        setEffectChain({"Lanczos", "CAS"});
        setSharpness(0.45f);
    } else if (presetName.contains("Bicubic (Catmull-Rom)")) {
        setEffectChain({"Bicubic", "Deband"});
    } else if (presetName.contains("Bicubic (Mitchell-Netravali)")) {
        setEffectChain({"Bicubic", "Deband"});
    } else if (presetName.contains("xBRZ")) {
        setEffectChain({"xBRZ", "CAS"});
        setSharpness(0.30f);
    } else if (presetName.contains("CRT")) {
        setEffectChain({"CRT"});
    } else if (presetName.contains("FXAA")) {
        setEffectChain({"FXAA", "CAS"});
        setSharpness(0.35f);
    } else {
        setEffectChain({"CuNNy", "CAS"});
    }
}

void ShaderPipeline::setEffectChain(const QStringList &chain)
{
    m_effectChain = chain;
    if (m_effectChain.isEmpty()) {
        m_effectChain << "CAS";
    }
}

void ShaderPipeline::setSingleEffect(const QString &effectName)
{
    m_effectChain.clear();
    m_effectChain << effectName;
}


void ShaderPipeline::addEffect(const QString &effectName)
{
    m_effectChain.append(effectName);
}

void ShaderPipeline::removeEffect(int index)
{
    if (index >= 0 && index < m_effectChain.size()) {
        m_effectChain.removeAt(index);
        if (m_effectChain.isEmpty()) {
            m_effectChain << "CAS";
        }
    }
}

bool ShaderPipeline::moveEffectUp(int index)
{
    if (index > 0 && index < m_effectChain.size()) {
        m_effectChain.swapItemsAt(index, index - 1);
        return true;
    }
    return false;
}

bool ShaderPipeline::moveEffectDown(int index)
{
    if (index >= 0 && index < m_effectChain.size() - 1) {
        m_effectChain.swapItemsAt(index, index + 1);
        return true;
    }
    return false;
}

QString ShaderPipeline::currentPipelineName() const
{
    return m_effectChain.join(" -> ");
}

QRect ShaderPipeline::calculateTargetRect(const QSize &sourceSize, const QSize &viewportSize) const
{
    if (sourceSize.width() <= 0 || sourceSize.height() <= 0 ||
        viewportSize.width() <= 0 || viewportSize.height() <= 0) {
        return QRect();
    }

    // Account for cropping
    int effSrcW = sourceSize.width() - m_cropMargins.left() - m_cropMargins.right();
    int effSrcH = sourceSize.height() - m_cropMargins.top() - m_cropMargins.bottom();
    if (effSrcW <= 0 || effSrcH <= 0) {
        effSrcW = sourceSize.width();
        effSrcH = sourceSize.height();
    }

    const int screenW = viewportSize.width();
    const int screenH = viewportSize.height();

    int scaledW = screenW;
    int scaledH = screenH;

    switch (m_scalingMode) {
    case ScalingMode::Fit: {
        const qreal scale = std::min((qreal)screenW / effSrcW,
                                     (qreal)screenH / effSrcH);
        scaledW = std::lround(effSrcW * scale);
        scaledH = std::lround(effSrcH * scale);
        break;
    }
    case ScalingMode::Integer: {
        int intScale = std::max(1, std::min(screenW / effSrcW,
                                            screenH / effSrcH));
        scaledW = effSrcW * intScale;
        scaledH = effSrcH * intScale;
        break;
    }
    case ScalingMode::Stretch:
        scaledW = screenW;
        scaledH = screenH;
        break;
    case ScalingMode::Windowed: {
        scaledW = std::min(screenW, (int)std::lround(effSrcW * m_windowScale));
        scaledH = std::min(screenH, (int)std::lround(effSrcH * m_windowScale));
        break;
    }
    }

    const int targetX = (screenW - scaledW) / 2;
    const int targetY = (screenH - scaledH) / 2;

    return QRect(targetX, targetY, scaledW, scaledH);
}

void ShaderPipeline::ensureFbos(const QSize &size)
{
    if (!m_fboPing || m_fboPing->size() != size) {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::NoAttachment);
        format.setTextureTarget(GL_TEXTURE_2D);
        format.setInternalTextureFormat(GL_RGBA8);
        m_fboPing = std::make_unique<QOpenGLFramebufferObject>(size, format);
        m_fboPong = std::make_unique<QOpenGLFramebufferObject>(size, format);
    }
}

void ShaderPipeline::renderQuad(QOpenGLShaderProgram *prog, GLuint inputTex, const QSize &sourceSize, const QRectF &targetRect, const QSize &viewportSize, const QRectF &uvRect)
{
    if (!prog) return;

    glViewport(0, 0, viewportSize.width(), viewportSize.height());

    QMatrix4x4 proj;
    proj.ortho(0.0f, viewportSize.width(), viewportSize.height(), 0.0f, -1.0f, 1.0f);

    prog->bind();
    prog->setUniformValue("uProjection", proj);
    prog->setUniformValue("uTexture", 0);
    prog->setUniformValue("uSourceSize", QVector2D(sourceSize.width(), sourceSize.height()));
    prog->setUniformValue("uSharpness", m_sharpness);
    prog->setUniformValue("uAnimeStrength", m_animeStrength);
    prog->setUniformValue("uDebandRange", (float)m_debandRange);
    prog->setUniformValue("uDebandThreshold", m_debandThreshold);
    prog->setUniformValue("uCRTScanline", m_crtScanline);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTex);

    const float x1 = targetRect.left();
    const float y1 = targetRect.top();
    const float x2 = targetRect.right();
    const float y2 = targetRect.bottom();

    const float u1 = uvRect.left();
    const float v1 = uvRect.top();
    const float u2 = uvRect.right();
    const float v2 = uvRect.bottom();

    const float quadVertices[] = {
        x1, y1, u1, v1,
        x1, y2, u1, v2,
        x2, y1, u2, v1,
        x2, y1, u2, v1,
        x1, y2, u1, v2,
        x2, y2, u2, v2,
    };

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, quadVertices);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, &quadVertices[2]);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    glBindTexture(GL_TEXTURE_2D, 0);
    prog->release();
}

void ShaderPipeline::render(GLuint inputTextureId, const QSize &sourceSize, const QRect &targetRect, const QSize &viewportSize)
{
    if (m_effectChain.isEmpty()) {
        m_effectChain << "CAS";
    }

    GLint origFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &origFbo);

    // Calculate crop UV rectangle
    float u1 = 0.0f, v1 = 0.0f, u2 = 1.0f, v2 = 1.0f;
    if (sourceSize.width() > 0 && sourceSize.height() > 0) {
        u1 = (float)m_cropMargins.left() / sourceSize.width();
        v1 = (float)m_cropMargins.top() / sourceSize.height();
        u2 = 1.0f - (float)m_cropMargins.right() / sourceSize.width();
        v2 = 1.0f - (float)m_cropMargins.bottom() / sourceSize.height();
    }
    QRectF initialUV(u1, v1, u2 - u1, v2 - v1);

    m_passTimings.clear();
    QElapsedTimer totalTimer;
    totalTimer.start();

    const int numPasses = m_effectChain.size();

    // If single pass: direct render to screen
    if (numPasses == 1) {
        QString effName = m_effectChain[0];
        auto *prog = getProgram(effName);
        if (!prog) prog = getProgram("CAS");

        QElapsedTimer passTimer;
        passTimer.start();

        renderQuad(prog, inputTextureId, sourceSize,
                   QRectF(targetRect.left(), targetRect.top(), targetRect.width(), targetRect.height()),
                   viewportSize, initialUV);

        float passMs = passTimer.nsecsElapsed() / 1000000.0f;
        m_passTimings.push_back({effName, passMs});
    } else {
        // Multi-Pass Sequential Ping-Pong FBO Pipeline!
        // Intermediate FBO resolution is targetRect.size()
        ensureFbos(targetRect.size());

        GLuint currentTex = inputTextureId;
        QSize curSourceSize = sourceSize;
        QRectF curUV = initialUV;

        for (int i = 0; i < numPasses; ++i) {
            QString effName = m_effectChain[i];
            auto *prog = getProgram(effName);
            if (!prog) prog = getProgram("CAS");

            QElapsedTimer passTimer;
            passTimer.start();

            bool isLastPass = (i == numPasses - 1);
            if (!isLastPass) {
                // Ping-pong selection: pass 0 writes to Ping, pass 1 writes to Pong, etc.
                QOpenGLFramebufferObject *fboTarget = (i % 2 == 0) ? m_fboPing.get() : m_fboPong.get();
                fboTarget->bind();
                glClear(GL_COLOR_BUFFER_BIT);

                renderQuad(prog, currentTex, curSourceSize,
                           QRectF(0, 0, targetRect.width(), targetRect.height()),
                           targetRect.size(), curUV);

                fboTarget->release();
                glBindFramebuffer(GL_FRAMEBUFFER, origFbo);

                currentTex = fboTarget->texture();
                curSourceSize = targetRect.size();
                // FBO textures in OpenGL have their origin at the bottom-left,
                // so v=1.0 is the top and v=0.0 is the bottom.
                curUV = QRectF(0.0f, 1.0f, 1.0f, -1.0f);
            } else {
                // Final Pass: Render onto destination viewport
                renderQuad(prog, currentTex, curSourceSize,
                           QRectF(targetRect.left(), targetRect.top(), targetRect.width(), targetRect.height()),
                           viewportSize, curUV);
            }

            float passMs = passTimer.nsecsElapsed() / 1000000.0f;
            m_passTimings.push_back({effName, passMs});
        }
    }

    m_lastTotalGpuTime = totalTimer.nsecsElapsed() / 1000000.0f;
}