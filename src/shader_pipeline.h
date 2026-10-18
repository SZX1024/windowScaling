#pragma once

#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QMatrix4x4>
#include <QSize>
#include <QRect>
#include <QMargins>
#include <QStringList>
#include <unordered_map>
#include <QElapsedTimer>
#include <memory>
#include <string>
#include <vector>

enum class ScalingMode {
    Fit = 0,     // Aspect-preserving with black bars (Pillarbox/Letterbox)
    Integer = 1, // Integer multiple scaling (1x, 2x, 3x, etc.)
    Stretch = 2, // Fill entire display
    Windowed = 3 // Windowed mode with custom scale
};

struct EffectPassInfo {
    QString name;
    float timeMs{0.0f};
};

class ShaderPipeline : protected QOpenGLFunctions {
public:
    ShaderPipeline();
    ~ShaderPipeline();

    bool init();

    // Effect Chain configuration
    void setEffectChain(const QStringList &chain);
    QStringList effectChain() const { return m_effectChain; }
    void setSingleEffect(const QString &effectName);
    QString currentPipelineName() const;

    // Presets
    static QStringList availableEffects();
    static QStringList availablePresets();
    void applyPreset(const QString &presetName);

    // Scaling Modes
    void setScalingMode(ScalingMode mode) { m_scalingMode = mode; }
    ScalingMode scalingMode() const { return m_scalingMode; }

    void setWindowScale(float s) { m_windowScale = std::max(0.5f, s); }
    float windowScale() const { return m_windowScale; }

    // Cropping Margins (pixels trimmed from original game)
    void setCropMargins(const QMargins &margins) { m_cropMargins = margins; }
    QMargins cropMargins() const { return m_cropMargins; }

    // Parameters
    void setSharpness(float s) { m_sharpness = std::clamp(s, 0.0f, 1.0f); }
    float sharpness() const { return m_sharpness; }

    void setAnimeStrength(float a) { m_animeStrength = std::clamp(a, 0.0f, 1.0f); }
    float animeStrength() const { return m_animeStrength; }

    QRect calculateTargetRect(const QSize &sourceSize, const QSize &viewportSize) const;

    // GPU Multi-pass execution with Ping-Pong FBOs
    void render(GLuint inputTextureId, const QSize &sourceSize, const QRect &targetRect, const QSize &viewportSize);

    // Profiler metrics
    const std::vector<EffectPassInfo> &lastPassTimings() const { return m_passTimings; }
    float lastTotalGpuTime() const { return m_lastTotalGpuTime; }

private:
    bool buildAllShaders();
    bool compileShader(const QString &name, const char *fsSource);
    void ensureFbos(const QSize &size);
    void renderQuad(QOpenGLShaderProgram *prog, GLuint inputTex, const QSize &sourceSize, const QRectF &targetRect, const QSize &viewportSize, const QRectF &uvRect);

    QStringList m_effectChain;
    ScalingMode m_scalingMode{ScalingMode::Fit};
    float m_windowScale{1.5f};
    QMargins m_cropMargins{0, 0, 0, 0};

    float m_sharpness{0.45f};
    float m_animeStrength{0.5f};

    std::unordered_map<std::string, std::unique_ptr<QOpenGLShaderProgram>> m_programs;
    QOpenGLShaderProgram* getProgram(const QString &name) const;
    std::unique_ptr<QOpenGLFramebufferObject> m_fboPing;
    std::unique_ptr<QOpenGLFramebufferObject> m_fboPong;

    std::vector<EffectPassInfo> m_passTimings;
    float m_lastTotalGpuTime{0.0f};
    QElapsedTimer m_timer;
};