# AGENTS.md — 类 Magpie 窗口超分放大工具（Linux / Wayland / Galgame 专属）

> 最后更新：2026-10-17
> 本文件是项目的唯一指导规范：定义目标、记录架构路线、规定验证关卡与设计标准。
> 状态标记：**[已定]** 用户已确认；**[进行中]** 当前研发重点。

---

## 1. 目标 **[已定]**

在 Linux (KDE Plasma 6 Wayland) 环境下，针对 **Galgame / 视觉小说 / 二次元游戏**（Wine / Proton / XWayland），实现体验与画质看齐 Windows [Blinue/Magpie](https://github.com/Blinue/Magpie) 的独立超分放大工具：
1. **热键全局触发**：在游戏运行中，随时按全局快捷键（`Ctrl + Alt + M`）一键等比全屏放大；再次按下恢复；
2. **专为二次元超分**：接入 Anime4K、AMD FSR 1.0、CAS 锐化、Lanczos 等算法，彻底解决低分辨率立绘与文字发糊；
3. **触控与鼠标 100% 对齐**：原生支持触摸屏直接点选、鼠标点击、按住拖动、滚轮历史（Backlog）；
4. **零桌面崩溃风险**：作为独立用户态程序运行，绝不侵入 KWin 主进程，保障桌面环境 100% 稳定安全。

---

## 2. 架构路线：独立全屏超分覆盖层（Independent Scaling Overlay）**[已定]**

经过实测，KWin 效果插件方案受制于 Linux 硬件级指针瞬移限制（`supportsWarping() == false`）导致点击与触控无法对齐。
本项目全面采用与 Windows 原版 Magpie 一致的**独立全屏覆盖层架构**：

```
+-----------------------------------------------------------+
|               Magpie Linux (独立超分覆盖层)                 |
|                                                           |
| 1. 画面采集           2. 超分着色器管线       3. 协议级输入回传 |
| [XComposite /       --> [OpenGL / Vulkan]  --> [XTest /      |
|  PipeWire DMA-BUF]      FSR / Anime4K / CAS     XSendEvent] |
+-----------------------------------------------------------+
       |                                              |
  (抓取游戏显存)                                 (定向注入点击与触控)
       v                                              v
+-----------------------------------------------------------+
|              目标 Galgame (Wine / Proton 窗口)             |
+-----------------------------------------------------------+
```

### 关键子系统设计
1. **捕获子系统（Capture Engine）**：
   - 目标为 Galgame（Wine/Proton 运行于 XWayland 体系）：通过 `XCompositeNameWindowPixmap` 直接提取目标窗口的独立离屏显存 Pixmap，转换为 OpenGL 纹理，零授权弹窗，极低 CPU 占用。
   - 备用：Wayland 窗口支持 PipeWire / DMA-BUF 零拷贝直传。
2. **超分与呈现子系统（Shader & Presentation Engine）**：
   - 独立全屏透明/黑底窗口（Qt6 / GLFW），独占渲染；
   - 拥有独立的现代 OpenGL 4.6 / Vulkan 运行时，无状态机冲突，可自由调用 Compute Shader 运行重型 Anime4K / ACNet 与 FSR；
   - 精准等比居中算法，黑边自动留白。
3. **输入与触控回传子系统（Input Forwarding Engine）**：
   - 覆盖层独占接收全局触屏手指点击（TouchDown/Motion）与鼠标按键；
   - 严格单向将屏幕触点折算为游戏窗口本地坐标：
     `x_game = (x_screen - targetX) / scale`, `y_game = (y_screen - targetY) / scale`；
   - 通过 `XTestFakeButtonEvent` / `XSendEvent` 直接注入进目标游戏窗口的 Window ID，触控与点击像素级 1:1 对齐。

---

## 3. 验证关卡（Gates）

### Gate 1 — 窗口画面捕获原型
- **标准**：独立程序能获取当前激活窗口或指定 Galgame 窗口的实时画面，并在 OpenGL 窗口中流畅预览。
- **依据**：`XComposite` Pixmap 提取或 PipeWire 捕获。

### Gate 2 — 全屏覆盖层与居中缩放
- **标准**：全局快捷键（`Ctrl + Alt + M`）激活时，弹出全屏置顶覆盖层，画面等比居中放大铺满屏幕；再次按下平滑退出。

### Gate 3 — 触控与鼠标 1:1 精确回传
- **标准**：全屏覆盖层截获鼠标与触摸屏操作，将点击、拖拽、滚轮精确投递给游戏窗口；手指点击游戏选项落点精准。

### Gate 4 — 着色器链集成（Anime4K / FSR / CAS）
- **标准**：参考 `reference/Magpie/src/Effects` 着色器，移植并接入 FSR、CAS 及 Anime4K 算法链，实现二次元画质超分。

---

## 4. 参考实现与资产

- 本地参考仓库：`reference/Magpie/`
- 着色器目录：`reference/Magpie/src/Effects/`（包含 ACNet, Anime4K, FSR, CAS, Lanczos 等）
