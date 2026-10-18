#include "profile_manager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

QJsonObject EffectPass::toJson() const
{
    QJsonObject obj;
    obj["name"] = name;
    obj["scalingType"] = scalingType;
    obj["scaleX"] = (double)scaleX;
    obj["scaleY"] = (double)scaleY;

    QJsonObject paramObj;
    for (auto it = parameters.begin(); it != parameters.end(); ++it) {
        paramObj[it.key()] = (double)it.value();
    }
    obj["parameters"] = paramObj;

    return obj;
}

EffectPass EffectPass::fromJson(const QJsonObject &obj)
{
    EffectPass p;
    p.name = obj["name"].toString();
    p.scalingType = obj["scalingType"].toString("Fit");
    p.scaleX = (float)obj["scaleX"].toDouble(1.0);
    p.scaleY = (float)obj["scaleY"].toDouble(1.0);

    QJsonObject paramObj = obj["parameters"].toObject();
    for (auto it = paramObj.begin(); it != paramObj.end(); ++it) {
        p.parameters[it.key()] = (float)it.value().toDouble();
    }

    return p;
}

QJsonObject ScalingModeConfig::toJson() const
{
    QJsonObject obj;
    obj["name"] = name;

    QJsonArray effArr;
    for (const auto &eff : effects) {
        effArr.append(eff.toJson());
    }
    obj["effects"] = effArr;

    return obj;
}

ScalingModeConfig ScalingModeConfig::fromJson(const QJsonObject &obj)
{
    ScalingModeConfig cfg;
    cfg.name = obj["name"].toString();

    QJsonArray effArr = obj["effects"].toArray();
    for (const auto &val : effArr) {
        cfg.effects.push_back(EffectPass::fromJson(val.toObject()));
    }

    return cfg;
}

QJsonObject GameProfile::toJson() const
{
    QJsonObject obj;
    obj["name"] = name;
    obj["windowClassPattern"] = windowClassPattern;
    obj["windowTitlePattern"] = windowTitlePattern;
    obj["scalingModeName"] = scalingModeName;

    QJsonArray chainArr;
    for (const auto &effect : effectChain) {
        chainArr.append(effect);
    }
    obj["effectChain"] = chainArr;

    obj["scalingMode"] = scalingMode;
    obj["windowScale"] = (double)windowScale;
    obj["sharpness"] = (double)sharpness;
    obj["animeStrength"] = (double)animeStrength;
    obj["debandThreshold"] = (double)debandThreshold;
    obj["debandRange"] = debandRange;
    obj["crtScanline"] = (double)crtScanline;

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
    p.scalingModeName = obj["scalingModeName"].toString("Lanczos");

    QJsonArray chainArr = obj["effectChain"].toArray();
    for (const auto &val : chainArr) {
        p.effectChain.append(val.toString());
    }
    if (p.effectChain.isEmpty()) {
        p.effectChain << "Lanczos";
    }

    p.scalingMode = obj["scalingMode"].toInt(0);
    p.windowScale = (float)obj["windowScale"].toDouble(1.5);
    p.sharpness = (float)obj["sharpness"].toDouble(0.45);
    p.animeStrength = (float)obj["animeStrength"].toDouble(0.5);
    p.debandThreshold = (float)obj["debandThreshold"].toDouble(0.04);
    p.debandRange = obj["debandRange"].toInt(16);
    p.crtScanline = (float)obj["crtScanline"].toDouble(0.5);

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
    createDefaultScalingModes();
    createDefaultProfiles();
    load();
}

QString ProfileManager::configFilePath() const
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/windowScaling";
    QDir().mkpath(configDir);
    return configDir + "/profiles.json";
}

void ProfileManager::createDefaultScalingModes()
{
    m_scalingModes.clear();

    // 1. Lanczos
    {
        ScalingModeConfig m;
        m.name = "Lanczos";
        EffectPass p;
        p.name = "Lanczos";
        p.scalingType = "Fit";
        m.effects.push_back(p);
        m_scalingModes.push_back(m);
    }
    // 2. FSR (EASU + RCAS sharpness=0.87)
    {
        ScalingModeConfig m;
        m.name = "FSR";
        EffectPass easu;
        easu.name = "FSR_EASU";
        easu.scalingType = "Fit";
        m.effects.push_back(easu);

        EffectPass rcas;
        rcas.name = "FSR_RCAS";
        rcas.scalingType = "None";
        rcas.parameters["sharpness"] = 0.87f;
        m.effects.push_back(rcas);

        m_scalingModes.push_back(m);
    }
    // 3. FSRCNNX
    {
        ScalingModeConfig m;
        m.name = "FSRCNNX";
        EffectPass p;
        p.name = "FSRCNNX";
        p.scalingType = "Fit";
        m.effects.push_back(p);
        m_scalingModes.push_back(m);
    }
    // 4. CuNNy
    {
        ScalingModeConfig m;
        m.name = "CuNNy";
        EffectPass p;
        p.name = "CuNNy";
        p.scalingType = "Fit";
        m.effects.push_back(p);
        m_scalingModes.push_back(m);
    }
    // 5. Anime4K
    {
        ScalingModeConfig m;
        m.name = "Anime4K";
        EffectPass p1;
        p1.name = "Anime4K";
        p1.parameters["animeStrength"] = 0.65f;
        m.effects.push_back(p1);

        EffectPass p2;
        p2.name = "Anime4K_Upscale";
        p2.scalingType = "Fit";
        m.effects.push_back(p2);

        m_scalingModes.push_back(m);
    }
    // 6. CRT-Geom
    {
        ScalingModeConfig m;
        m.name = "CRT-Geom";
        EffectPass p;
        p.name = "CRT";
        p.scalingType = "Fit";
        p.parameters["crtScanline"] = 0.50f;
        m.effects.push_back(p);
        m_scalingModes.push_back(m);
    }
    // 7. Integer Scale 2x
    {
        ScalingModeConfig m;
        m.name = "Integer Scale 2x";
        EffectPass p;
        p.name = "Bilinear";
        p.scalingType = "Normal";
        p.scaleX = 2.0f;
        p.scaleY = 2.0f;
        m.effects.push_back(p);
        m_scalingModes.push_back(m);
    }
}

void ProfileManager::createDefaultProfiles()
{
    m_profiles.clear();

    GameProfile def;
    def.name = "默认 (Default)";
    def.scalingModeName = "Lanczos";
    def.effectChain = {"Lanczos"};
    def.scalingMode = 0; // Fit
    m_profiles[def.name] = def;

    GameProfile galgame;
    galgame.name = "Galgame 视觉小说精细画质";
    galgame.scalingModeName = "CuNNy";
    galgame.effectChain = {"CuNNy", "CAS"};
    galgame.scalingMode = 0; // Fit
    galgame.sharpness = 0.50f;
    galgame.animeStrength = 0.65f;
    m_profiles[galgame.name] = galgame;

    GameProfile debandProfile;
    debandProfile.name = "去色彩断层 (Bicubic + Deband)";
    debandProfile.scalingModeName = "Bicubic + Deband";
    debandProfile.effectChain = {"Bicubic", "Deband"};
    debandProfile.scalingMode = 0;
    debandProfile.sharpness = 0.40f;
    m_profiles[debandProfile.name] = debandProfile;

    m_currentProfile = def;
}

std::vector<ScalingModeConfig> ProfileManager::listScalingModes() const
{
    return m_scalingModes;
}

ScalingModeConfig ProfileManager::getScalingModeByName(const QString &name) const
{
    for (const auto &m : m_scalingModes) {
        if (m.name == name) return m;
    }
    if (!m_scalingModes.empty()) return m_scalingModes[0];
    return ScalingModeConfig{};
}

void ProfileManager::saveScalingMode(const ScalingModeConfig &mode)
{
    for (size_t i = 0; i < m_scalingModes.size(); ++i) {
        if (m_scalingModes[i].name == mode.name) {
            m_scalingModes[i] = mode;
            save();
            return;
        }
    }
    m_scalingModes.push_back(mode);
    save();
}

void ProfileManager::deleteScalingMode(const QString &name)
{
    for (auto it = m_scalingModes.begin(); it != m_scalingModes.end(); ++it) {
        if (it->name == name) {
            m_scalingModes.erase(it);
            save();
            return;
        }
    }
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

    // Load Scaling Modes
    if (root.contains("scalingModes")) {
        QJsonArray smArr = root["scalingModes"].toArray();
        if (!smArr.isEmpty()) {
            m_scalingModes.clear();
            for (const auto &item : smArr) {
                m_scalingModes.push_back(ScalingModeConfig::fromJson(item.toObject()));
            }
        }
    }

    // Load Profiles
    if (root.contains("profiles")) {
        QJsonArray arr = root["profiles"].toArray();
        for (const auto &item : arr) {
            GameProfile p = GameProfile::fromJson(item.toObject());
            if (!p.name.isEmpty()) {
                m_profiles[p.name] = p;
            }
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

    // Save Scaling Modes
    QJsonArray smArr;
    for (const auto &m : m_scalingModes) {
        smArr.append(m.toJson());
    }
    root["scalingModes"] = smArr;

    // Save Profiles
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
