#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <vector>
#include <memory>

struct EffectPass {
    QString name;
    QString scalingType{"Fit"}; // "Fit", "Normal", "Fill"
    float scaleX{1.0f};
    float scaleY{1.0f};
    QMap<QString, float> parameters;

    QJsonObject toJson() const;
    static EffectPass fromJson(const QJsonObject &obj);
};

struct ScalingModeConfig {
    QString name;
    std::vector<EffectPass> effects;

    QString description() const {
        QStringList names;
        for (const auto &e : effects) {
            names << e.name;
        }
        return names.join(", ");
    }

    QJsonObject toJson() const;
    static ScalingModeConfig fromJson(const QJsonObject &obj);
};

struct GameProfile {
    QString name;
    QString windowClassPattern;
    QString windowTitlePattern;

    QString scalingModeName{"Lanczos"};
    QStringList effectChain; // Fallback or active chain
    int scalingMode{0};      // 0: Fit, 1: Integer, 2: Stretch, 3: Windowed
    float windowScale{1.5f}; // For Windowed mode
    float sharpness{0.45f};  // For CAS
    float animeStrength{0.5f}; // For Anime4K
    float debandThreshold{0.04f};
    int debandRange{16};
    float crtScanline{0.5f};

    // Cropping margins (pixels to trim from original game before scaling)
    int cropLeft{0};
    int cropTop{0};
    int cropRight{0};
    int cropBottom{0};

    // Overlay Toolbar & Profiler settings
    bool toolbarPinned{false};
    bool showProfiler{false};

    QJsonObject toJson() const;
    static GameProfile fromJson(const QJsonObject &obj);
};

class ProfileManager {
public:
    static ProfileManager &instance();

    bool load();
    bool save();

    // Scaling Modes (Pipelines from screenshot)
    std::vector<ScalingModeConfig> listScalingModes() const;
    ScalingModeConfig getScalingModeByName(const QString &name) const;
    void saveScalingMode(const ScalingModeConfig &mode);
    void deleteScalingMode(const QString &name);

    // Profiles
    GameProfile getProfileForWindow(const QString &winClass, const QString &winTitle);
    void setProfileForWindow(const QString &key, const GameProfile &profile);

    GameProfile currentProfile() const { return m_currentProfile; }
    void setCurrentProfile(const GameProfile &p) { m_currentProfile = p; }

    QStringList listProfileNames() const;
    GameProfile getProfileByName(const QString &name) const;
    void saveProfile(const GameProfile &profile);
    void deleteProfile(const QString &name);

private:
    ProfileManager();
    QString configFilePath() const;
    void createDefaultProfiles();
    void createDefaultScalingModes();

    QMap<QString, GameProfile> m_profiles;
    std::vector<ScalingModeConfig> m_scalingModes;
    GameProfile m_currentProfile;
    QString m_lastMatchedKey;
};
