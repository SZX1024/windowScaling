#include <QCoreApplication>
#include <QPluginLoader>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <iostream>
#include <cmath>
#include <cassert>

#include <KPluginMetaData>

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

    // Fixed math: KWin projection already translates by winPos, so data translation is target - winPos
    const double tx = targetX - winX;
    const double ty = targetY - winY;

    return { scale, scaledW, scaledH, targetX, targetY, tx, ty };
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    QString pluginPath = QStringLiteral("/home/SZX10246/Projects/software/windowScaling/build/bin/kwin/effects/plugins/kwin4_effect_magpie.so");
    std::cout << "=== Gate 2 Test Runner ===" << std::endl;
    std::cout << "Testing plugin: " << pluginPath.toStdString() << std::endl;

    // 1. QPluginLoader verification
    QPluginLoader loader(pluginPath);
    QJsonObject metaData = loader.metaData();
    bool loaderMetaValid = !metaData.isEmpty();
    std::cout << "[TEST 1] QPluginLoader metaData valid: " << (loaderMetaValid ? "PASS" : "FAIL") << std::endl;
    assert(loaderMetaValid);

    // 2. KPluginMetaData verification
    KPluginMetaData kdata(pluginPath);
    bool kdataValid = kdata.isValid();
    std::cout << "[TEST 2] KPluginMetaData valid: " << (kdataValid ? "PASS" : "FAIL") << std::endl;
    assert(kdataValid);

    // 3. Test mathematical centering cases
    // Case A: 800x600 in 1920x1080 (4:3 in 16:9)
    auto resA = computeMagpieTransform({100, 100, 800, 600}, {0, 0, 1920, 1080});
    assert(std::abs(resA.scale - 1.8) < 1e-5);
    assert(std::abs(resA.targetX - 240.0) < 1e-5);
    assert(std::abs(resA.targetY - 0.0) < 1e-5);
    assert(std::abs(resA.tx - 140.0) < 1e-5);
    assert(std::abs(resA.ty - (-100.0)) < 1e-5);
    std::cout << "[TEST 3] Math Case A (4:3 in 16:9): PASS" << std::endl;

    // Case B: 1280x720 in 2048x1152 (1.25 fractional scale screen)
    auto resB = computeMagpieTransform({200, 150, 1280, 720}, {0, 0, 2048, 1152});
    assert(std::abs(resB.scale - 1.6) < 1e-5);
    assert(std::abs(resB.targetX - 0.0) < 1e-5);
    assert(std::abs(resB.targetY - 0.0) < 1e-5);
    assert(std::abs(resB.tx - (-200.0)) < 1e-5);
    assert(std::abs(resB.ty - (-150.0)) < 1e-5);
    std::cout << "[TEST 4] Math Case B (16:9 in 2048x1152 HiDPI): PASS" << std::endl;

    std::cout << "=== ALL VERIFICATIONS PASSED ===" << std::endl;
    return 0;
}
