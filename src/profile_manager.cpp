#include "profile_manager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

QJsonObject GameProfile::toJson() const
{
    QJsonObject obj;
    obj["name"] = name;
    obj["windowClassPattern"] = windowClassPattern;
    obj["windowTitlePattern"] = windowTitlePattern;

    QJsonArray chainArr;
    for (const auto &effect : effectChain) {
        chainArr.append(effect);
    }
    obj["effectChain"] = chainArr;

    obj["scalingMode"] = scalingMode;
    obj["windowScale"] = (double)windowScale;
    obj["sharpness"] = (double)sharpness;
    obj["animeStrength"] = (double)animeStrength;

    obj["cropLeft"] = cropLeft;
    obj["cropTop"] = cropTop;
    obj["cropRight"] = cropRight;
    obj["cropBottom"] = cropBottom;

    obj["toolbarPinned"] = toolbarPinned;
    obj["showProfiler"] = showProfiler;

    return obj;
}

GameProfile GameProfile::fromJson(const QJsonObject &obj)
{
    GameProfile p;
    p.name = obj["name"].toString();
    p.windowClassPattern = obj["windowClassPattern"].toString();
    p.windowTitlePattern = obj["windowTitlePattern"].toString();

    QJsonArray chainArr = obj["effectChain"].toArray();
    for (const auto &val : chainArr) {
        p.effectChain.append(val.toString());
    }
    if (p.effectChain.isEmpty()) {
        p.effectChain << "CAS";
    }

    p.scalingMode = obj["scalingMode"].toInt(0);
    p.windowScale = (float)obj["windowScale"].toDouble(1.5);
    p.sharpness = (float)obj["sharpness"].toDouble(0.45);
    p.animeStrength = (float)obj["animeStrength"].toDouble(0.5);

    p.cropLeft = obj["cropLeft"].toInt(0);
    p.cropTop = obj["cropTop"].toInt(0);
    p.cropRight = obj["cropRight"].toInt(0);
    p.cropBottom = obj["cropBottom"].toInt(0);

    p.toolbarPinned = obj["toolbarPinned"].toBool(false);
    p.showProfiler = obj["showProfiler"].toBool(false);

    return p;
}

ProfileManager &ProfileManager::instance()
{
    static ProfileManager s_inst;
    return s_inst;
}

ProfileManager::ProfileManager()
{
    createDefaultProfiles();
    load();
}

QString ProfileManager::configFilePath() const
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/windowScaling";
    QDir().mkpath(configDir);
    return configDir + "/profiles.json";
}

void ProfileManager::createDefaultProfiles()
{
    GameProfile def;
    def.name = "默认 (Default)";
    def.effectChain = {"CuNNy", "CAS"};
    def.scalingMode = 0; // Fit
    def.sharpness = 0.45f;
    def.animeStrength = 0.5f;
    m_profiles[def.name] = def;

    GameProfile galgame;
    galgame.name = "Galgame 视觉小说精细画质";
    galgame.effectChain = {"Anime4K", "CAS"};
    galgame.scalingMode = 0; // Fit
    galgame.sharpness = 0.55f;
    galgame.animeStrength = 0.65f;
    m_profiles[galgame.name] = galgame;

    GameProfile debandProfile;
    debandProfile.name = "去色彩断层 (Bicubic + Deband)";
    debandProfile.effectChain = {"Bicubic", "Deband"};
    debandProfile.scalingMode = 0;
    debandProfile.sharpness = 0.4f;
    m_profiles[debandProfile.name] = debandProfile;

    GameProfile retro;
    retro.name = "复古像素点阵 (Integer 2x + CRT)";
    retro.effectChain = {"CRT"};
    retro.scalingMode = 1; // Integer
    m_profiles[retro.name] = retro;

    m_currentProfile = def;
}

bool ProfileManager::load()
{
    QFile file(configFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();
    QJsonArray arr = root["profiles"].toArray();
    for (const auto &item : arr) {
        GameProfile p = GameProfile::fromJson(item.toObject());
        if (!p.name.isEmpty()) {
            m_profiles[p.name] = p;
        }
    }

    QString last = root["lastProfile"].toString();
    if (m_profiles.contains(last)) {
        m_currentProfile = m_profiles[last];
    }

    return true;
}

bool ProfileManager::save()
{
    QFile file(configFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QJsonObject root;
    QJsonArray arr;
    for (auto it = m_profiles.begin(); it != m_profiles.end(); ++it) {
        arr.append(it.value().toJson());
    }
    root["profiles"] = arr;
    root["lastProfile"] = m_currentProfile.name;

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

GameProfile ProfileManager::getProfileForWindow(const QString &winClass, const QString &winTitle)
{
    for (auto it = m_profiles.begin(); it != m_profiles.end(); ++it) {
        const auto &p = it.value();
        if (!p.windowClassPattern.isEmpty() && winClass.contains(p.windowClassPattern, Qt::CaseInsensitive)) {
            m_currentProfile = p;
            m_lastMatchedKey = p.name;
            return p;
        }
        if (!p.windowTitlePattern.isEmpty() && winTitle.contains(p.windowTitlePattern, Qt::CaseInsensitive)) {
            m_currentProfile = p;
            m_lastMatchedKey = p.name;
            return p;
        }
    }
    return m_currentProfile;
}

void ProfileManager::setProfileForWindow(const QString &key, const GameProfile &profile)
{
    m_profiles[key] = profile;
    m_currentProfile = profile;
    save();
}

QStringList ProfileManager::listProfileNames() const
{
    return m_profiles.keys();
}

GameProfile ProfileManager::getProfileByName(const QString &name) const
{
    if (m_profiles.contains(name)) {
        return m_profiles[name];
    }
    return m_currentProfile;
}

void ProfileManager::saveProfile(const GameProfile &profile)
{
    m_profiles[profile.name] = profile;
    m_currentProfile = profile;
    save();
}

void ProfileManager::deleteProfile(const QString &name)
{
    if (name == "默认 (Default)") return;
    m_profiles.remove(name);
    if (m_currentProfile.name == name) {
        m_currentProfile = m_profiles["默认 (Default)"];
    }
    save();
}