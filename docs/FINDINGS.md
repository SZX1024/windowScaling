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

---

## Gate 2: 视觉原型（Spike A - 实测通过）

- **验证时间**: 2026-10-17
- **实现目标**: 基于 `KWin::OffscreenEffect` 实现窗口重定向、保持宽高比居中缩放、热键/D-Bus 启停及安全的生命周期销毁。

### 实现细节
1. **基类升级**:
   - `MagpieEffect` 继承自 `KWin::OffscreenEffect`，利用 KWin 原生离屏渲染与双线性纹理采样能力。
2. **热键与 D-Bus 启停**:
   - 快捷键 `Meta+Alt+M` 绑定 `MagpieEffect::toggle()`。
   - 注册 D-Bus 接口至 `/org/kde/KWin/Magpie`，导出所有槽函数供外部控制。
   - 激活时检测 `effects->activeWindow()`，要求必须为 `isNormalWindow() || isDialog()`；保存 `m_targetWindow` 并调用 `redirect(m_targetWindow)`。
   - 退出时调用 `unredirect(m_targetWindow)`，重置状态并触发 `effects->addRepaintFull()`。
3. **窗口生命周期防崩保护**:
   - 连接 `effects->windowDeleted` -> `handleWindowDeleted(EffectWindow *)`。
   - 连接 `effects->windowClosed` -> `slotWindowClosed(EffectWindow *)`。
   - 目标窗口被关闭或销毁时，主动清空引用并调用 `unredirect`，防止悬垂指针导致 KWin 崩溃。
4. **保持宽高比居中缩放计算 (apply)**:
   - 提取窗口物理输出 `window->screen()`，兜底使用 `effects->activeScreen()`。
   - 严格根据输出区域与窗口边框计算 aspect-ratio scale: `scale = std::min(outW / winW, outH / winH)`。
   - 计算屏幕居中坐标: `targetX = outX + (outW - scaledW) / 2.0`，平移量 `tx = targetX - winX * scale`。
   - 应用到 `WindowPaintData`: `data.setXScale(scale)`、`data.setYScale(scale)`、`data.setXTranslation(tx)`、`data.setYTranslation(ty)`。
5. **预绘制阶段标志位设置**:
   - `prePaintScreen`: `data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS`。
   - `prePaintWindow`: 对目标窗口设置 `data.mask |= PAINT_WINDOW_TRANSFORMED`。

### 验证证据
1. **编译构建**:
   - `cmake --build build` 成功构建 `build/bin/kwin/effects/plugins/kwin4_effect_magpie.so`。
2. **符号表核验**:
   - `nm -C` 证实包含 `MagpieEffect::apply`、`prePaintScreen`、`prePaintWindow`、`toggle`、`handleWindowDeleted`、`slotWindowClosed`。
   - 证实导出 `typeinfo for KWin::OffscreenEffect`，虚表结构完好。
3. **测试运行器 (`test_runner.cpp`) 验证**:
   - QPluginLoader 元数据有效且 IID 匹配 `org.kde.kwin.EffectPluginFactory6.7.5`。
   - KPluginMetaData 识别 ID 为 `kwin4_effect_magpie`，分类为 `Accessibility`。
   - `KPluginMetaData::findPlugins` 成功检索。
   - 4:3 窗口在 16:9 输出、1080p 在 4K 输出、多显示器偏移下的缩放和平移计算均精准通过。
### Gate 2 调试与修复关键记录 (实测排查)
1. **OSD 弹窗反复出现的原因**:
   - 构造函数中调用了 `showOnScreenMessage`，导致每次合成器重绘或重构时反复触发。现已彻底移除构造函数中的弹窗，仅在用户显式触发快捷键时提示。
2. **放大未居中的原因 (HiDPI / 分数缩放与阴影)**:
   - 宿主机显示屏使用了 **1.25 倍分数缩放** (2048x1152)。
   - 原代码使用 `window->frameGeometry()`，未包含 KWin 窗口阴影/装饰外框，而 `OffscreenEffect` 内部生成 quads 时使用的是 `window->expandedGeometry()`。两者坐标原点与宽高不一致，导致平移计算出现偏移。现已统一为 `expandedGeometry()`，居中数学对齐。
3. **窗口处于最底层的排查与解决**:
   - 原绘制管线仅在 `drawWindow` 中对目标窗口应用形变，但随后合成器按窗口层级依然会继续绘制处于上层的任务栏/面板 (Plasma Panel) 以及其他窗口，将放大后的画面遮挡。
   - 实现重写 `paintWindow`: 当 Magpie 处于激活状态时，对所有非目标窗口设置 `data.multiplyOpacity(0.0)` 进行遮罩隐藏，赋予放大后的游戏窗口独占置顶的全屏呈现。
4. **会话稳定性保障**:
   - 严禁使用重启 KWin 服务命令，全部采用平滑的原生 D-Bus 重构：`qdbus-qt6 org.kde.KWin /Effects reconfigureEffect "kwin4_effect_magpie"`。
