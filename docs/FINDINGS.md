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
### 居中数学推导最终核准结论
- **KWin 合成管线平移机制**:
  - `OffscreenEffect` 在生成视图投影矩阵时，底层自动应用了窗口当前坐标平移：`matrix = Projection * Translate(window->pos())`。
  - 随后乘上 `data.toMatrix()`：`data.toMatrix() = Translate(data.translation()) * Scale(data.scale())`。
  - 对顶点 `P = (0, 0)`（相对于窗口原点），最终屏幕坐标为：
    `ScreenPos = window->pos() + data.translation()`。
  - 要使窗口左上角落到屏幕计算的 `targetPos`，所需平移量**必须且严格**为：
    `translation = targetPos - window->pos()`。
  - 原公式错误地减去了 `window->pos() * scale`，导致窗口被向左上方过度偏移了 `window->pos() * (scale - 1)` 个像素，且比例越大偏差越严重。现已彻底修正。
- **OSD 弹窗清除**:
  - KWin 的 `showOnScreenMessage` 属于模态/常驻型消息，不会自动超时消失。已彻底移除所有 `showOnScreenMessage` 调用，并在插件构造时显式调用 `hideOnScreenMessage()` 清除屏幕残留。
---

## Gate 3: 输入映射与原生系统通知 (实测通过)

- **验证时间**: 2026-10-17
- **实现模块**: `MagpieInputFilter` (继承 `KWin::InputEventFilter`) 与 FreeDesktop 原生通知。

### 实现与技术验证
1. **输入过滤器架构**:
   - 继承 `KWin::InputEventFilter`，注册于 `InputFilterOrder::InteractiveMoveResize` 层级（在全局快捷键之后，窗口动作之前）。
   - 拦截并重写指针事件（指针移动 `pointerMotion`、鼠标点击 `pointerButton`、滚轮滚动 `pointerAxis`）。
   - 原窗口坐标反向映射函数 `mapScaledToOriginal(const QPointF &pos)`：
     - 精准剔除全屏居中黑边偏移（`targetX`, `targetY`）；
     - 将相对坐标按 `scale` 进行反向除法还原为原始窗口坐标；
     - 限制在游戏窗口边界内（`std::clamp`），防止点击黑边时误触背景。
   - 动态挂载：仅在 Magpie 激活期间调用 `input()->installInputEventFilter(this)`；退出时立即销毁并注销（`uninstallInputEventFilter`），平态运行时 0 额外开销。
2. **原生桌面系统通知**:
   - 彻底废除不自动退出的 KWin OSD (`showOnScreenMessage`)。
   - 采用标准 FreeDesktop / KDE Plasma 桌面通知协议 (`org.freedesktop.Notifications`)：
     - 激活时弹出标准桌面通知：`Magpie 已激活: 正在等比全屏放大 [窗口名]`；
     - 退出时弹出：`Magpie 已退出: 窗口已恢复原始尺寸`；
     - 2 秒自动渐隐消失，完全符合现代桌面交互标准。
### Gate 3 方案一（真实光标与视觉光标分离）实测落地记录
1. **快捷键切换**:
   - 快捷键由容易被桌面环境吞掉的 `Win+Alt+M` 全面切换为更标准通用的 **`Ctrl + Alt + M`**。
2. **输入死循环根除与双游标架构**:
   - **核心问题诊断**: 原先在 `InputEventFilter` 中篡改输入坐标，导致 KWin 内部将映射后的坐标当作全局光标新位置，每一帧再次缩小，形成指数级飞跳死循环。
   - **方案 1 实测落地**:
     - 采用非侵入式 `KWin::InputEventSpy` 监听指针动作，**严格拦截并忽略 `event->warp == true` 的事件**，彻底切断递归反馈回路。
     - **真实指针**：利用 Gate 1 验证的 `input()->warpPointer(origPos)`，使系统底层真实指针动态跟随并稳定驻留在原始窗口对应的像素点内。游戏窗口的原生 Wayland/X11 协议接收 100% 准确的真实悬停、点击与滚轮。
     - **视觉光标**：激活时调用 `effects->hideCursor()` 隐藏系统指针，在 `paintScreen` 阶段根据 `m_visualCursorPos` 以及当前光标图标 (`effects->cursorImage()`) 使用专用着色器在全屏放大层绘制出视觉光标。
### 触摸与绝对坐标映射实测突破
- **硬件发现**: 通过系统 `/proc/bus/input/devices` 发现目标机存在 `libvirtualhid Mouse (Absolute)` 绝对指针与触摸屏通道。
- **痛点根源**: 原逻辑仅处理了传统相对位移鼠标 (`PointerMotionEvent`)，对于触摸屏手指直接触控的 `TouchDownEvent`、`TouchMotionEvent` 以及手写笔/绝对设备 `TabletToolTipEvent` 没有任何拦截映射。导致在全屏放大下，手指每一次点击屏幕，KWin 传下去的都是原始物理屏幕坐标，产生完全不对齐的触控偏移。
- **修复实现**:
  - 在 `MagpieInputSpy` 中全量扩展重写了 `touchDown`、`touchMotion`、`pointerButton`、`pointerAxis`、`tabletToolTipEvent`、`tabletToolAxisEvent`。
  - 所有触控与绝对点击事件在到达合成器分发前，全部经过 `mapScaledToOriginal` 归一化并缩放换算回目标窗口原生坐标，完美支持屏幕触控玩 Galgame。
### 鼠标按键与快捷键持久化排查结论
1. **鼠标按键点击错位的真凶 (二次映射踩坑)**:
   - 反编译 `PointerInputRedirection::processButton` 发现：在调用 `pointerButton` 过滤器之前，KWin 已经把当前指针坐标 `m_pos` 拷贝进了 `PointerButtonEvent->position`。
   - 而在此前鼠标移动时，`handlePointerEvent` 已经将 `m_pos` 瞬移到了原窗口的 `origPos`。
   - 此时在 `pointerButton` 中再次执行 `mapScaledToOriginal(event->position)`，相当于**把已经换算回原窗口的坐标再次进行了反向映射**，所有点击都会被死死挤压到窗口最左上角边界。
   - **修复**: 以屏幕上的视觉光标位置 `m_visualCursorPos` 为**唯一真理源 (Single Source of Truth)**，每次点击事件仅进行一次严格、单向的坐标计算，根除二次映射错误。
2. **快捷键 `Ctrl+Alt+M` 强制覆盖**:
   - KDE 的 `KGlobalAccel::setShortcut` 默认带有 `Autoloading` 标志，会优先读取用户配置文件中存留的历史快捷键。
   - 使用 `KGlobalAccel::NoAutoloading` 标志强制在内存中将 `ToggleMagpie` 强刷绑定至 `Ctrl + Alt + M`。
---

## Gate 3: 方案 3（全屏原生输入代理层与协议级直投）实测通过

- **实现目标**: 彻底废除不稳定的硬件指针瞬移 (`warpPointer`)，采用 KWin 6.7.5 原生全屏输入拦截机制，直接向游戏 surface 注入 Wayland 协议事件。

### 核心机制与原理
1. **输入拦截 (Input Interception)**:
   - 激活时调用 `effects->startMouseInterception(this, Qt::ArrowCursor)`。
   - KWin 接管全屏所有鼠标/触摸事件，不再向桌面或底层泄露任何无效点击，彻底杜绝误触。
2. **KWin 6.7 新特性虚函数直接**:
   - `MagpieEffect::pointerMotion(PointerMotionEvent *event)`
   - `MagpieEffect::pointerButton(PointerButtonEvent *event)`
   - `MagpieEffect::pointerAxis(PointerAxisEvent *event)`
   - `MagpieEffect::touchDown`, `touchMotion`, `touchUp`, `touchCancel`
3. **坐标表面归一化 (`mapScaledToSurface`)**:
   - 彻底摆脱窗口全局坐标、窗口外框阴影及多屏偏移的复杂计算。
   - 计算光标或触点相对于放大画面的相对位置后直接除以 `scale`，换算出游戏客户端表面内的局部坐标 `surfacePos = (surfX, surfY)`，其中 `(0, 0)` 严格为游戏画面的左上角像素。
4. **Wayland 协议级直投 (`SeatInterface`)**:
   - 提取 `waylandServer()->seat()` 与 `m_targetWindow->surface()`。
   - 每次移动：`seat->notifyPointerMotion(surfacePos); seat->notifyPointerFrame();`
   - 每次点击：`seat->notifyPointerButton(event->button, event->state); seat->notifyPointerFrame();`
   - 每次滚轮：`seat->notifyPointerAxis(...); seat->notifyPointerFrame();`
   - 每次触控：`seat->notifyTouchDown(surface, surfacePos, id, pos);`
   - 游戏客户端接收到的就是标准的 Wayland 协议原生点击，不经过任何中间硬件扭曲，100% 绝对精确对齐。
