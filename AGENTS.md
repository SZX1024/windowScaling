# AGENTS.md — 类 Magpie 窗口超分放大工具（Linux / Wayland / Galgame 专属）

> 最后更新：2026-10-17
> 本文件为项目唯一指导技术规范：定义目标、记录 Windows 原版 Magpie 完整功能全景清单、规划 Linux 本地化落地方案与架构。
> 状态标记：**[已实现]** 当前版本已交付并实测通过；**[规划中]** 下阶段扩展特性。

---

## 1. 目标 **[已定]**

在 Linux (KDE Plasma 6 / Wayland) 环境下，针对 **Galgame / 视觉小说 / 二次元游戏**（运行于 Wine / Proton / XWayland 环境），全面对标 Windows 原版 [Blinue/Magpie](https://github.com/Blinue/Magpie)，打造一款**开箱即用、带现代桌面 GUI、系统托盘常驻、纯 GPU 硬件加速多算法顺序串联管线、触控与鼠标 1:1 精准对齐**的桌面独立超分辨率放大工具。

---

## 2. Windows 原版 Magpie 完整功能全景清单（对标参考）

通过深入查阅 `reference/Magpie` 源码（`src/Magpie/` 与 `docs/`），原版 Magpie 具备的核心功能矩阵如下：

### A. 缩放与比例模式 (Scaling Modes)
1. **等比填充 (Fit / Auto)**：严格保持源窗口宽高比，居中放大至屏幕极限，边缘自动添加黑边 (Pillarbox/Letterbox)。**[已实现]**
2. **整数倍缩放 (Integer Scale)**：按 1x, 2x, 3x 等整数倍无损放大，专为复古点阵像素游戏设计，避免任何插值伪影。**[已实现]**
3. **全屏拉伸 (Stretch / Fill)**：忽略原比例强制填满整个屏幕。**[已实现]**
4. **窗口化缩放 (Windowed Mode)**：放大后依然保留窗口外壳，以自定义倍率在桌面上显示。**[规划中]**
5. **画面裁剪 (Cropping)**：自动或手动裁剪源窗口的黑边或边框。**[规划中]**

### B. 核心超分辨率算法与多算法串联流水线 (Effects Pipeline)
参考 `reference/Magpie/src/Effects` 与 `EffectDrawer.cpp`：
1. **多算法顺序串联执行 (Multi-Effect Chaining)**：
   - **原版机制**：Magpie 允许在一个配置档（Profile）中添加**多个有序排列的效果**（`std::vector<EffectDrawer> _effectDrawers`）。前一个效果的输出纹理作为后一个效果的输入纹理（`inOutTexture`）。
   - **Linux 实现方案**：利用现代 OpenGL 4.5+ / Vulkan 显存中的 **Ping-Pong 帧缓冲对象（FBOs）**，全在显卡显存中流水线流转，CPU 零开销。
   - **典型 Galgame 组合**：
     - `Anime4K (线条重构/暗部细化) -> AMD CAS (字体自适应对比度锐化)`
     - `Lanczos3 (高保真插值) -> AMD CAS (立绘与文字抗模糊)`
     - `Bicubic -> Deband (去除二次元天空与渐变背景色带断层)`。**[规划中]**
2. **内置算法库（30+ 种）**：
   - **AMD FidelityFX CAS**：自适应对比度锐化（带 Sharpness 参数）。**[已实现]**
   - **Anime4K 系列**：
     - `Restore` / `Thin_HQ`：动漫暗部线条细化与轮廓重构。**[已实现]**
     - `Upscale` (Mode A/B/C) 与 `Denoise` 降噪。**[规划中]**
   - **ACNet**：适合动漫风格图像的 CNN 神经网络超分辨率算法，兼具强力降噪。**[规划中]**
   - **AMD FidelityFX FSR 1.0**：EASU 边缘自适应空间放大 + RCAS 稳健锐化。**[规划中]**
   - **CuNNy / CuNNy2**：专为视觉小说/Galgame 优化的神经超分模型。**[规划中]**
   - **Lanczos3 / Jinc**：高保真 Sinc 抗振铃插值滤波。**[已实现]**
   - **Bilinear / Bicubic (Catmull-Rom/Mitchell)**：基础双线性与双三次插值。**[已实现]**
   - **去色带 (Deband)**：消除二次元立绘渐变色块与天空暗部的色彩断层。**[规划中]**
   - **像素艺术滤镜 (xBRZ / MMPX)**：像素游戏专属平滑放大。**[规划中]**
   - **CRT 模拟着色器 (Easymode / Geom / Hyllian / Lottes)**：复古街机扫描线与荧光屏模拟。**[规划中]**
   - **抗锯齿 (FXAA / SMAA)**：快速形态抗锯齿。**[规划中]**

### C. 全屏置顶悬浮工具栏与性能诊断 (In-Game Overlay Toolbar & Profiler)
参考 `reference/Magpie/src/Magpie.Core/OverlayDrawer.cpp`（基于 ImGui / DirectX 实现的置顶工具栏）：
1. **顶部悬浮工具栏 (Top Floating Toolbar)**:
   - 居中吸附于屏幕顶端：`SetNextWindowPos((DisplaySize.x - windowWidth) / 2, 0)`；
   - 交互模式：
     - **AutoHide（自动隐藏）**：鼠标靠近屏幕顶部时平滑淡入滑出，移开后半透明/隐藏；
     - **Pinned（图钉锁定）**：常驻锁定在屏幕顶端。**[规划中]**
2. **一键全屏无损超分截屏 (Take Screenshot)**:
   - 工具栏带有相机图标按钮；
   - 点击时直接从 GPU 渲染的最终超分辨率帧缓冲（FBO）中抓取无损画面；
   - 自动按模板命名（如 `Screenshot_YYYYMMDD_HHMMSS.png`）存入相册目录（`~/Pictures/Magpie_Screenshots/`），并发出桌面通知。**[规划中]**
3. **实时性能开销看板 (Profiler / FPS Monitor)**:
   - 点击工具栏诊断图标（Profiler）展开悬浮性能小窗；
   - 监控指标包含：
     - **GPU 设备名称**：识别当前运行的显卡（如 `Intel Arc RGB Controller` / 核显）；
     - **捕获管线状态**：显示当前捕获后端（如 `XComposite + MIT-SHM` / `PipeWire`）；
     - **实时显示帧率 (FPS)**：按 500ms 滑动窗口平滑统计实际帧率；
     - **单帧全链路耗时 (Frame Time)**：单帧毫秒级延迟（如 `6.9 ms`）；
     - **多算法着色器耗时 (Effect Timings)**：细分展示算法链中每个 Pass 的具体 GPU 计算耗时（如 `Anime4K: 1.2ms, CAS: 0.4ms`）；
     - **分辨率映射指标**：源窗口分辨率 $\to$ 目标全屏分辨率（如 `1280x720 -> 2560x1440`）。**[规划中]**

### D. 交互与控制体系 (Input & UI)
1. **现代 GUI 桌面面板**：
   - 窗口实时识别、缩放模式、算法选择、参数调节滑块（CAS 锐度、线条强度等）。**[已实现]**
2. **系统托盘后台常驻 (System Tray Icon)**：
   - 启动后自动最小化到托盘；
   - 点击主界面“关闭”按钮默认收缩至托盘，后台持续待命监听热键；
   - 托盘右键菜单快速放大、打开设置、彻底退出。**[已实现]**
3. **全局热键监听 (Global Shortcut)**：
   - 系统级全局热键（默认 `Ctrl + Alt + M`）一键放大/恢复。**[已实现]**
   - 全屏中快捷键（按 `1/2/3/4` 实时对比算法，按 `Esc` 退出）。**[已实现]**
4. **游戏进程配置档 (Profiles)**：
   - 按游戏窗口名/进程名独立保存对应的缩放模式、算法及参数（换游戏自动切换预设）。**[规划中]**
5. **倒计时延时捕获 (Countdown Seconds)**：
   - 支持倒计时秒数切回游戏。**[已实现]**

### E. 输入与触控映射 (Input & Touch System)
1. **触控屏 1:1 绝对对齐 (Touch Support)**：
   - 手指触控点击（TouchDown）、滑动（TouchMotion）精准映射还原回游戏窗口。**[已实现]**
2. **鼠标自由光标与按键直投**：
   - 悬停时光标全屏绝对自由移动（绝不被拉扯/吸附至右侧）；
   - 点击、拖动、滚轮文本历史通过 `XSendEvent` 协议直投目标游戏，100% 绝对对齐。**[已实现]**
3. **后台防休眠与绝对置顶 (Focus & Stacking Guard)**：
   - 覆盖层带 `Qt::WindowDoesNotAcceptFocus`，游戏始终持有前台焦点，Live2D 与动画永不停机；**[已实现]**
   - EWMH `_NET_WM_STATE_BELOW` 锁定游戏图层，无论怎么点击游戏绝不翻上来遮挡；**[已实现]**
   - 游戏内部退出时 8ms 探针自毁覆盖层，绝无黑屏残留。**[已实现]**

---

## 3. 技术路线与硬件加速架构

架构全景：
```
+-----------------------------------------------------------------------------------------+
|                                Magpie Linux 桌面主程序                                   |
|                                                                                         |
|  +--------------------------------+   +-----------------------+   +------------------+  |
|  |     GUI 设置主面板 (MainWindow)|   |   系统托盘 (TrayIcon)  |   | 全局热键监听服务  |  |
|  | - 游戏窗口下拉列表与状态监控   |   | - 启动后自动常驻托盘   |   | - 默认 Ctrl+Alt+M|  |
|  | - 缩放模式 (等比Fit / 整数倍)  |   | - 托盘右键快捷菜单     |   | - 支持按键捕获配置|  |
|  | - 算法链配置与参数调节滑块     |   | - 单击/双击唤出设置    |   +------------------+  |
|  +--------------------------------+   +-----------------------+                         |
|                                                                                         |
|  +-----------------------------------------------------------------------------------+  |
|  |                全屏独占超分呈现层 (Fullscreen Scaling Overlay)                     |  |
|  |                                                                                   |  |
|  |  1. 显存零拷贝捕获         2. GPU 硬件加速多算法串联管线      3. 协议级输入定向直投   |  |
|  |  [XComposite/MIT-SHM]  --> [GPU Ping-Pong FBO 显存链]   --> [XSendEvent]          |  |
|  |                             - Pass 1: Anime4K Line Restore   - 触控屏手指绝对      |  |
|  |                             - Pass 2: AMD FidelityFX CAS     - 鼠标点击/按住拖拽   |  |
|  |                             - Pass 3: 屏幕等比居中呈现        - 滚轮文本回看        |  |
|  |                                                                                   |  |
|  |  4. 全屏悬浮工具栏 (Top Floating Toolbar): 截图 (Screenshot) / 性能看板 (Profiler)|  |
|  +-----------------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------------+
                                      |                     ^
                                (提取游戏显存)         (直接注入点击/触控)
                                      v                     |
+-----------------------------------------------------------------------------------------+
|                       目标 Galgame (Wine / Proton 游戏窗口)                              |
+-----------------------------------------------------------------------------------------+
```

---

## 4. 交付文件与使用说明

1. 编译可执行程序：`bin/magpie-overlay`
2. 启动器便捷脚本：`bin/magpie`
   - 图形面板模式：`./bin/magpie`
   - 静默托盘常驻：`./bin/magpie --tray`
   - 延时 2 秒全屏：`./bin/magpie --delay 2`
   - 窗口列表查看：`./bin/magpie --list`
3. 桌面快捷方式：`magpie-overlay.desktop`（复制到 `~/.local/share/applications/` 即可在开始菜单中搜索打开）。
