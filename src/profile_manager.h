#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QMap>
#include <memory>

struct GameProfile {
    QString name;
    QString windowClassPattern;
    QString windowTitlePattern;

    QStringList effectChain; // e.g. ["Anime4K", "CAS"]
    int scalingMode{0};      // 0: Fit, 1: Integer, 2: Stretch, 3: Windowed
    float windowScale{1.5f}; // For Windowed mode
    float sharpness{0.45f};  // For CAS
    float animeStrength{0.5f}; // For Anime4K

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

    QMap<QString, GameProfile> m_profiles;
    GameProfile m_currentProfile;
    QString m_lastMatchedKey;
};
