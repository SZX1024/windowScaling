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

// 2. Lanczos3 Sinc
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

// 3. AMD FidelityFX CAS
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

// 4. Anime4K Line Reconstruction
static const char *animeFS = R"(#version 330 core
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

    if (maxL - minL > 0.06) {
        vec3 bestColor = (lc < (minL + maxL) * 0.5)
            ? min(c, min(min(u, d), min(l, r)))
            : max(c, max(max(u, d), max(l, r)));
        c = mix(c, bestColor, uAnimeStrength * 0.6);
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
    initializeOpenGLFunctions();
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

    // Anime4K
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
    case ScalerType::CAS: return "AMD FidelityFX CAS (Adaptive Sharpening)";
    case ScalerType::Anime4K: return "Anime4K Line Reconstruction (2D Anime)";
    default: return "Unknown";
    }
}

QRect ShaderPipeline::calculateTargetRect(const QSize &sourceSize, const QSize &viewportSize) const
{
    if (sourceSize.width() <= 0 || sourceSize.height() <= 0 ||
        viewportSize.width() <= 0 || viewportSize.height() <= 0) {
        return QRect();
    }

    const int screenW = viewportSize.width();
    const int screenH = viewportSize.height();

    int scaledW = screenW;
    int scaledH = screenH;

    switch (m_scalingMode) {
    case ScalingMode::Fit: {
        const qreal scale = std::min((qreal)screenW / sourceSize.width(),
                                     (qreal)screenH / sourceSize.height());
        scaledW = std::lround(sourceSize.width() * scale);
        scaledH = std::lround(sourceSize.height() * scale);
        break;
    }
    case ScalingMode::Integer: {
        int intScale = std::max(1, std::min(screenW / sourceSize.width(),
                                            screenH / sourceSize.height()));
        scaledW = sourceSize.width() * intScale;
        scaledH = sourceSize.height() * intScale;
        break;
    }
    case ScalingMode::Stretch:
        scaledW = screenW;
        scaledH = screenH;
        break;
    }

    const int targetX = (screenW - scaledW) / 2;
    const int targetY = (screenH - scaledH) / 2;

    return QRect(targetX, targetY, scaledW, scaledH);
}

void ShaderPipeline::render(GLuint inputTextureId, const QSize &sourceSize, const QRect &targetRect, const QSize &viewportSize)
{
    QOpenGLShaderProgram *prog = nullptr;
    switch (m_currentScaler) {
    case ScalerType::Bilinear: prog = m_bilinearProgram.get(); break;
    case ScalerType::Lanczos:  prog = m_lanczosProgram.get(); break;
    case ScalerType::CAS:      prog = m_casProgram.get(); break;
    case ScalerType::Anime4K:  prog = m_animeProgram.get(); break;
    default: prog = m_casProgram.get(); break;
    }

    if (!prog) return;

    // Viewport is screen
    glViewport(0, 0, viewportSize.width(), viewportSize.height());

    QMatrix4x4 proj;
    proj.ortho(0.0f, viewportSize.width(), viewportSize.height(), 0.0f, -1.0f, 1.0f);

    prog->bind();
    prog->setUniformValue("uProjection", proj);
    prog->setUniformValue("uTexture", 0);
    prog->setUniformValue("uSourceSize", QVector2D(sourceSize.width(), sourceSize.height()));
    prog->setUniformValue("uSharpness", m_sharpness);
    prog->setUniformValue("uAnimeStrength", m_animeStrength);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTextureId);

    // Quad coordinates for targetRect
    const float x1 = targetRect.left();
    const float y1 = targetRect.top();
    const float x2 = targetRect.right() + 1.0f;
    const float y2 = targetRect.bottom() + 1.0f;

    const float quadVertices[] = {
        x1, y1, 0.0f, 0.0f,
        x1, y2, 0.0f, 1.0f,
        x2, y1, 1.0f, 0.0f,
        x2, y1, 1.0f, 0.0f,
        x1, y2, 0.0f, 1.0f,
        x2, y2, 1.0f, 1.0f,
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
