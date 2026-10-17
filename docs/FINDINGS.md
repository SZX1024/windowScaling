# FINDINGS — 实验结论与证据记录

## Gate 0: 环境与最小空插件验证（实测通过）

- **验证时间**: 2026-10-17
- **系统环境**: Fedora 44, KWin 6.7.5 (`kwin-devel-6.7.5-1.fc44`, `qt6-qtbase-devel-6.11.2`, `extra-cmake-modules-6.31.0`)

### 实测结果与证据
1. **构建系统与依赖**:
   - 依赖项全部就绪，CMake 成功找到 `KWin`、`Qt6`、`KF6`。
   - 成功编译生成 `build/bin/kwin/effects/plugins/kwin4_effect_magpie.so`。
2. **插件元数据与接口绑定**:
   - 采用 `KWin::EffectPluginFactory` 与 `Q_PLUGIN_METADATA`，包含 `magpieeffect.moc`。
   - 使用独立测试程序检验：
     - `Loader metaData valid: 1`
     - `IID: "org.kde.kwin.EffectPluginFactory6.7.5"`
     - `KPluginMetaData valid: 1 id: kwin4_effect_magpie name: Magpie Scaling`
     - `KPluginMetaData::findPlugins("kwin/effects/plugins")` 通过 `QT_PLUGIN_PATH` 成功检索到插件。
3. **安全调试途径核实**:
   - 确认 `kwin_wayland` 支持 `--wayland-display` 窗口化嵌套运行 (`kwin_wayland --socket wayland-kwin-test --width 1280 --height 720`)，提供无桌面崩溃风险的沙盒调试能力。

---

## Gate 1: libkwin 内部符号检查（实测结果）

- **检查时间**: 2026-10-17
- **系统版本**: Fedora (FC44), KWin 6.7.5 (`kwin-6.7.5-1.fc44.x86_64`, `kwin-libs-6.7.5-1.fc44.x86_64`)
- **动态库路径**: `/usr/lib64/libkwin.so.6.7.5`

### 导出的关键符号 (实测)
通过 `nm -D --defined-only /usr/lib64/libkwin.so.6 | c++filt` 检查，确认以下关键符号均已被动态导出：
1. `KWin::PointerInputRedirection::warp(QPointF const&)` (地址: 0x00235410)
2. `KWin::InputRedirection::warpPointer(QPointF const&)` (地址: 0x00190500)
3. `KWin::InputRedirection::installInputEventSpy(KWin::InputEventSpy*)` (地址: 0x00180280)
4. `KWin::PointerInputRedirection::processMotionAbsolute(QPointF const&, ...)`
5. `KWin::PointerInputRedirection::processButton(...)`
6. `KWin::PointerInputRedirection::processAxis(...)`

### 头文件验证 (实测)
- `kwin-devel` 在 Fedora 上完整包含了内部核心头文件：
  - `/usr/include/kwin/pointer_input.h` (明确包含 `void warp(const QPointF &pos);`)
  - `/usr/include/kwin/input.h`
  - `/usr/include/kwin/input_event_spy.h`
  - `/usr/include/kwin/effect/offscreeneffect.h`
