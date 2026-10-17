#pragma once

#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QMatrix4x4>
#include <QSize>
#include <QRect>
#include <memory>
#include <string>

enum class ScalerType {
    Bilinear = 0,
    Lanczos = 1,
    CAS = 2,
    Anime4K = 3
};

enum class ScalingMode {
    Fit = 0,     // Aspect-preserving with black bars (Pillarbox/Letterbox)
    Integer = 1, // Integer multiple scaling (1x, 2x, 3x, etc.)
    Stretch = 2  // Fill entire display
};

class ShaderPipeline : protected QOpenGLFunctions {
public:
    ShaderPipeline();
    ~ShaderPipeline();

    bool init();

    void setScaler(ScalerType type);
    ScalerType currentScaler() const { return m_currentScaler; }
    const char *currentScalerName() const;

    void setScalingMode(ScalingMode mode) { m_scalingMode = mode; }
    ScalingMode scalingMode() const { return m_scalingMode; }

    void setSharpness(float s) { m_sharpness = std::clamp(s, 0.0f, 1.0f); }
    float sharpness() const { return m_sharpness; }

    void setAnimeStrength(float a) { m_animeStrength = std::clamp(a, 0.0f, 1.0f); }
    float animeStrength() const { return m_animeStrength; }

    QRect calculateTargetRect(const QSize &sourceSize, const QSize &viewportSize) const;

    // GPU Multi-pass execution
    void render(GLuint inputTextureId, const QSize &sourceSize, const QRect &targetRect, const QSize &viewportSize);

private:
    bool buildShaders();
    void ensureFbo(const QSize &size);

    ScalerType m_currentScaler{ScalerType::CAS};
    ScalingMode m_scalingMode{ScalingMode::Fit};
    float m_sharpness{0.45f};
    float m_animeStrength{0.5f};

    std::unique_ptr<QOpenGLShaderProgram> m_bilinearProgram;
    std::unique_ptr<QOpenGLShaderProgram> m_lanczosProgram;
    std::unique_ptr<QOpenGLShaderProgram> m_casProgram;
    std::unique_ptr<QOpenGLShaderProgram> m_animeProgram;

    std::unique_ptr<QOpenGLFramebufferObject> m_fboPing;
    std::unique_ptr<QOpenGLFramebufferObject> m_fboPong;
};
