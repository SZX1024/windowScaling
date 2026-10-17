#include "shader_pipeline.h"
#include <QDebug>

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

// 2. Lanczos3 Sinc Resampler
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

// 3. AMD Contrast Adaptive Sharpening (CAS)
static const char *casFS = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec2 uSourceSize;
uniform float uSharpness; // 0.0 to 1.0

void main() {
    vec2 texel = 1.0 / uSourceSize;

    // Fetch cross neighborhood
    vec3 a = texture(uTexture, TexCoord + vec2(0.0, -texel.y)).rgb;
    vec3 b = texture(uTexture, TexCoord + vec2(-texel.x, 0.0)).rgb;
    vec3 e = texture(uTexture, TexCoord).rgb;
    vec3 d = texture(uTexture, TexCoord + vec2(texel.x, 0.0)).rgb;
    vec3 c = texture(uTexture, TexCoord + vec2(0.0, texel.y)).rgb;

    // Soft minimum and maximum
    vec3 minRGB = min(min(min(a, b), min(c, d)), e);
    vec3 maxRGB = max(max(max(a, b), max(c, d)), e);

    // Smooth limits
    vec3 ampRGB = clamp(min(minRGB, 2.0 - maxRGB) / max(maxRGB, 1e-5), 0.0, 1.0);
    vec3 wRGB = -sqrt(ampRGB) * (uSharpness * 0.18);

    // Filter
    vec3 res = (wRGB * (a + b + c + d) + e) / (1.0 + 4.0 * wRGB);
    FragColor = vec4(clamp(res, 0.0, 1.0), 1.0);
}
)";

// 4. Anime4K Line Thinning / Edge Enhancement
static const char *animeFS = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec2 uSourceSize;

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

    // Dark line thinning for anime art
    if (maxL - minL > 0.08) {
        // High contrast line edge: reconstruct sharper transition
        vec3 bestColor = (lc < (minL + maxL) * 0.5) ? min(c, min(min(u, d), min(l, r))) : max(c, max(max(u, d), max(l, r)));
        c = mix(c, bestColor, 0.4);
    }

    FragColor = vec4(c, 1.0);
}
)";

ShaderPipeline::ShaderPipeline()
{
}

ShaderPipeline::~ShaderPipeline()
{
}

bool ShaderPipeline::init()
{
    return buildShaders();
}

bool ShaderPipeline::buildShaders()
{
    // Bilinear
    m_bilinearProgram = std::make_unique<QOpenGLShaderProgram>();
    m_bilinearProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, commonVS);
    m_bilinearProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, bilinearFS);
    if (!m_bilinearProgram->link()) return false;

    // Lanczos
    m_lanczosProgram = std::make_unique<QOpenGLShaderProgram>();
    m_lanczosProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, commonVS);
    m_lanczosProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, lanczosFS);
    if (!m_lanczosProgram->link()) return false;

    // CAS
    m_casProgram = std::make_unique<QOpenGLShaderProgram>();
    m_casProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, commonVS);
    m_casProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, casFS);
    if (!m_casProgram->link()) return false;

    // Anime
    m_animeProgram = std::make_unique<QOpenGLShaderProgram>();
    m_animeProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, commonVS);
    m_animeProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, animeFS);
    if (!m_animeProgram->link()) return false;

    return true;
}

void ShaderPipeline::setScaler(ScalerType type)
{
    m_currentScaler = type;
}

const char *ShaderPipeline::currentScalerName() const
{
    switch (m_currentScaler) {
    case ScalerType::Bilinear: return "Bilinear (Fast)";
    case ScalerType::Lanczos: return "Lanczos3 Sinc Resampler";
    case ScalerType::CAS: return "AMD CAS (Adaptive Sharpening)";
    case ScalerType::AnimeThin: return "Anime4K Line Reconstruction";
    default: return "Unknown";
    }
}

QOpenGLShaderProgram *ShaderPipeline::activeProgram()
{
    switch (m_currentScaler) {
    case ScalerType::Bilinear: return m_bilinearProgram.get();
    case ScalerType::Lanczos: return m_lanczosProgram.get();
    case ScalerType::CAS: return m_casProgram.get();
    case ScalerType::AnimeThin: return m_animeProgram.get();
    default: return m_casProgram.get();
    }
}

void ShaderPipeline::setUniforms(const QMatrix4x4 &proj, const QSize &sourceSize, float sharpness)
{
    auto *prog = activeProgram();
    if (!prog) return;

    prog->bind();
    prog->setUniformValue("uProjection", proj);
    prog->setUniformValue("uTexture", 0);
    prog->setUniformValue("uSourceSize", QVector2D(sourceSize.width(), sourceSize.height()));
    prog->setUniformValue("uSharpness", sharpness);
}
