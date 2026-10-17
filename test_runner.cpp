#include <QCoreApplication>
#include <QPluginLoader>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <iostream>
#include <cmath>
#include <cassert>

#include <KPluginMetaData>

// Replicate geometry centering math for exact verification
struct RectFMock {
    double x;
    double y;
    double width;
    double height;
};

struct ScaleResult {
    double scale;
    double scaledW;
    double scaledH;
    double targetX;
    double targetY;
    double tx;
    double ty;
};

ScaleResult computeMagpieTransform(const RectFMock &winGeom, const RectFMock &outputRect) {
    const double winW = winGeom.width;
    const double winH = winGeom.height;
    const double winX = winGeom.x;
    const double winY = winGeom.y;

    const double outW = outputRect.width;
    const double outH = outputRect.height;
    const double outX = outputRect.x;
    const double outY = outputRect.y;

    const double scale = std::min(outW / winW, outH / winH);
    const double scaledW = winW * scale;
    const double scaledH = winH * scale;

    const double targetX = outX + (outW - scaledW) / 2.0;
    const double targetY = outY + (outH - scaledH) / 2.0;

    const double tx = targetX - winX * scale;
    const double ty = targetY - winY * scale;

    return { scale, scaledW, scaledH, targetX, targetY, tx, ty };
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    QString pluginPath = QStringLiteral("build/bin/kwin/effects/plugins/kwin4_effect_magpie.so");
    std::cout << "=== Gate 2 Test Runner ===" << std::endl;
    std::cout << "Testing plugin: " << pluginPath.toStdString() << std::endl;

    // 1. QPluginLoader verification
    QPluginLoader loader(pluginPath);
    QJsonObject metaData = loader.metaData();
    bool loaderMetaValid = !metaData.isEmpty();
    std::cout << "[TEST 1] QPluginLoader metaData valid: " << (loaderMetaValid ? "PASS" : "FAIL") << std::endl;
    assert(loaderMetaValid);

    QString iid = metaData.value(QStringLiteral("IID")).toString();
    std::cout << "         IID: " << iid.toStdString() << std::endl;
    assert(iid.startsWith(QStringLiteral("org.kde.kwin.EffectPluginFactory")));

    // 2. KPluginMetaData verification
    KPluginMetaData kdata(pluginPath);
    bool kdataValid = kdata.isValid();
    std::cout << "[TEST 2] KPluginMetaData valid: " << (kdataValid ? "PASS" : "FAIL") << std::endl;
    assert(kdataValid);

    std::cout << "         Plugin ID: " << kdata.pluginId().toStdString() << std::endl;
    assert(kdata.pluginId() == QStringLiteral("kwin4_effect_magpie"));

    std::cout << "         Plugin Name: " << kdata.name().toStdString() << std::endl;
    assert(kdata.name() == QStringLiteral("Magpie Scaling"));

    std::cout << "         Category: " << kdata.category().toStdString() << std::endl;
    assert(kdata.category() == QStringLiteral("Accessibility"));

    // 3. Plugin discovery via KPluginMetaData::findPlugins
    qputenv("QT_PLUGIN_PATH", "build/bin");
    const auto discovered = KPluginMetaData::findPlugins(QStringLiteral("kwin/effects/plugins"));
    bool foundDiscovered = false;
    for (const auto &p : discovered) {
        if (p.pluginId() == QStringLiteral("kwin4_effect_magpie")) {
            foundDiscovered = true;
            break;
        }
    }
    std::cout << "[TEST 3] Discovery in kwin/effects/plugins: " << (foundDiscovered ? "PASS" : "FAIL") << std::endl;
    assert(foundDiscovered);

    // 4. Test aspect-preserving scaling & centering math
    std::cout << "[TEST 4] Aspect-preserving scaling & centering math" << std::endl;

    // Case A: 800x600 window on 1920x1080 screen at (0,0)
    {
        RectFMock win{100, 150, 800, 600};
        RectFMock out{0, 0, 1920, 1080};
        auto res = computeMagpieTransform(win, out);
        // scale = min(1920/800=2.4, 1080/600=1.8) = 1.8
        assert(std::abs(res.scale - 1.8) < 1e-6);
        assert(std::abs(res.scaledW - 1440.0) < 1e-6);
        assert(std::abs(res.scaledH - 1080.0) < 1e-6);
        assert(std::abs(res.targetX - 240.0) < 1e-6);
        assert(std::abs(res.targetY - 0.0) < 1e-6);

        // Verify transformed window bounds: x' = winX * scale + tx
        double mappedX = win.x * res.scale + res.tx;
        double mappedY = win.y * res.scale + res.ty;
        double mappedRight = (win.x + win.width) * res.scale + res.tx;
        double mappedBottom = (win.y + win.height) * res.scale + res.ty;

        assert(std::abs(mappedX - res.targetX) < 1e-6);
        assert(std::abs(mappedY - res.targetY) < 1e-6);
        assert(std::abs(mappedRight - (res.targetX + res.scaledW)) < 1e-6);
        assert(std::abs(mappedBottom - (res.targetY + res.scaledH)) < 1e-6);
        std::cout << "         Case A (4:3 in 16:9): PASS (scale=" << res.scale 
                  << ", target=" << res.targetX << "," << res.targetY 
                  << " " << res.scaledW << "x" << res.scaledH << ")" << std::endl;
    }

    // Case B: 1920x1080 window on 4K display (3840x2160) at (0,0)
    {
        RectFMock win{200, 100, 1920, 1080};
        RectFMock out{0, 0, 3840, 2160};
        auto res = computeMagpieTransform(win, out);
        assert(std::abs(res.scale - 2.0) < 1e-6);
        assert(std::abs(res.scaledW - 3840.0) < 1e-6);
        assert(std::abs(res.scaledH - 2160.0) < 1e-6);
        assert(std::abs(res.targetX - 0.0) < 1e-6);
        assert(std::abs(res.targetY - 0.0) < 1e-6);
        std::cout << "         Case B (1080p in 4K): PASS (scale=2.0)" << std::endl;
    }

    // Case C: Multi-monitor offset: screen at (1920, 0), size 1920x1080
    {
        RectFMock win{2000, 100, 1280, 720};
        RectFMock out{1920, 0, 1920, 1080};
        auto res = computeMagpieTransform(win, out);
        // scale = min(1920/1280=1.5, 1080/720=1.5) = 1.5
        assert(std::abs(res.scale - 1.5) < 1e-6);
        assert(std::abs(res.scaledW - 1920.0) < 1e-6);
        assert(std::abs(res.scaledH - 1080.0) < 1e-6);
        assert(std::abs(res.targetX - 1920.0) < 1e-6);
        assert(std::abs(res.targetY - 0.0) < 1e-6);

        double mappedX = win.x * res.scale + res.tx;
        assert(std::abs(mappedX - 1920.0) < 1e-6);
        std::cout << "         Case C (Multi-monitor offset (1920,0)): PASS" << std::endl;
    }

    std::cout << "=== ALL VERIFICATIONS PASSED ===" << std::endl;
    return 0;
}
