#pragma once

#include <QOpenGLShaderProgram>
#include <QMatrix4x4>
#include <QSize>
#include <memory>
#include <string>

enum class ScalerType {
    Bilinear,
    Lanczos,
    CAS,
    AnimeThin
};

class ShaderPipeline {
public:
    ShaderPipeline();
    ~ShaderPipeline();

    bool init();
    void setScaler(ScalerType type);
    ScalerType currentScaler() const { return m_currentScaler; }
    const char *currentScalerName() const;

    QOpenGLShaderProgram *activeProgram();
    void setUniforms(const QMatrix4x4 &proj, const QSize &sourceSize, float sharpness = 0.5f);

private:
    bool buildShaders();

    ScalerType m_currentScaler{ScalerType::CAS}; // CAS default for crisp Galgame text!
    std::unique_ptr<QOpenGLShaderProgram> m_bilinearProgram;
    std::unique_ptr<QOpenGLShaderProgram> m_lanczosProgram;
    std::unique_ptr<QOpenGLShaderProgram> m_casProgram;
    std::unique_ptr<QOpenGLShaderProgram> m_animeProgram;
};
