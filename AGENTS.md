# AGENTS.md — 类 Magpie 窗口放大工具（KDE Plasma / Wayland）

> 最后更新：2026-10-17
> 本文件是给 coding agent 读的项目说明：声明需求、记录路线、规定验证关卡和工作规则。
> 状态标记：**[已定]** 用户明确说过；**[建议]** 由评估得出、待用户确认；**[待核实]** 尚未验证，不得当作事实使用。

---

## 1. 目标

在 Linux 的 Wayland 会话下，做一个体验接近 [Blinue/Magpie](https://github.com/Blinue/Magpie) 的工具：
**用热键对一个正在运行的窗口一键放大，并用超分/锐化算法输出到屏幕。**
最终希望做成可日常使用的产品，而不是一次性实验。**[已定]**

## 2. 环境（开发机即目标机）**[已定]**

- 发行版：Fedora
- 桌面：KDE Plasma 6.7.5，Wayland 会话
- Magpie 本身只支持 Windows（依赖 Windows 窗口捕获与 DirectX 11），不能直接移植。

## 3. 需求

### 必须（MUST）**[建议，待用户确认]**

1. 热键触发，对**当前激活窗口**放大到整个屏幕；再按一次退出，恢复原状。
2. 原窗口按其原始（较低）分辨率渲染，输出经过至少一种放大算法（Lanczos / FSR / CAS 任选其一起步）。
3. 键盘、鼠标输入能正确作用到原窗口：点击、拖拽、滚轮的落点与放大后的画面一致。
4. 光标显示在与画面一致的位置。
5. 退出或出错时必须干净恢复；**插件崩溃会带崩 KWin，进而带崩整个桌面**，所以稳定性优先于功能数量。

### 应该（SHOULD）**[建议]**

- Anime4K 或类似的 CNN 放大。
- 按程序保存配置档（算法、参数）。
- 游戏所需的相对鼠标 / 指针锁定。
- 多显示器、HiDPI / 分数缩放。
- 延迟与帧时间的测量手段。

### 初期不做（WON'T）**[建议]**

帧生成、X11 会话、KDE 以外的桌面、图形配置界面、Windows 支持。

## 4. 路线

已评估的方案：

| 方案 | 能否实现“对运行中窗口热键放大” | 结论 |
|---|---|---|
| A. 包装 gamescope（启动器） | 否，只能在启动程序时包裹 | 作为兜底参考 |
| B. 自写嵌套合成器（smithay / wlroots） | 否，只能在启动程序时包裹，但输入问题天然解决 | **兜底方案** |
| C. KWin C++ 效果插件 | 是，唯一能完整实现目标体验的方案 | **当前推荐路线 [建议]** |
| 独立程序 + portal 捕获 + 覆盖层 | 画面可以，输入转发在 Wayland 下基本是死结 | 放弃 |

选择 C 的理由：只有合成器同时掌握窗口内容、光标和输入；用 KWin 内部的窗口画面则不需要额外捕获，也没有 portal 授权弹窗。

**路线 C 的最大风险是输入映射**，见第 5 节关卡。若验证失败，退回方案 B，因此着色器链要做成与 KWin 解耦的独立模块，两种形态共用。

## 5. 验证关卡（Gates）

**每个关卡结束时，agent 必须停下来向用户汇报结论，由用户决定是否继续。不得自行跳过或改走别的路线。**

### Gate 0 — 环境
- 通过标准：能在本机编译并加载一个空的 KWin 效果插件，效果在“桌面效果”设置里可见并可启停。
- 失败：汇报缺失的依赖与报错。

### Gate 1 — 输入能力检查（半小时级，**最先做**）
目的：判断插件能否在 KWin 内部调用“移动真实指针”这类能力。
- 公开的 `EffectsHandler` 接口里没有注入事件或瞬移指针的方法；鼠标拦截会让所有窗口收不到事件。（已核对头文件）
- KWin 内部的 `PointerInputRedirection` 有 `warp(const QPointF&)`，KWin 自己的测试里在用。（来自文档镜像，版本**[待核实]**，以 v6.7.5 源码为准）
- 需要检查两件事：
  ```bash
  # kwin-devel 是否带了内部头文件（多半没有）
  ls /usr/include/kwin | grep -i -E "pointer|input"

  # libkwin 是否导出了相关符号（先用 rpm -ql 找到 libkwin.so.6 的路径）
  nm -D --defined-only /path/to/libkwin.so.6 | c++filt | grep PointerInputRedirection
  ```
- 通过：符号已导出，可以把 KWin 源码的 `src/` 加入 include 路径来调用。
- 失败：向用户汇报，选项是 (a) 给 KWin 提上游补丁（不保证被接受），(b) 退回方案 B。

### Gate 2 — 视觉原型（Spike A）
- 内容：热键触发，当前激活窗口拉伸画满屏幕，原窗口不再正常绘制；先用双线性，再接一个着色器。
- 通过标准：画面正确、被放大的程序不卡帧、HiDPI 下无明显错位。
- 要特别验证：**原窗口不被正常绘制时，KWin 是否还会通知应用渲染下一帧**（**[待核实]**，可能导致游戏卡住）。

### Gate 3 — 输入映射
- 内容：在 Gate 1 的结论上实现输入映射与光标。
- 通过标准：点击、拖拽、滚轮都准确落到原窗口；隐藏真实光标并在映射位置绘制光标；在一个使用相对鼠标的程序里转视角正常。

### Gate 4 — 着色器链与延迟
- 内容：接入 FSR / CAS，再评估 Anime4K。
- 通过标准：给出开启前后的延迟与帧时间对比数据。

## 6. 已核实与待核实的事实

**已核实**（来自 KWin 公开头文件与资料）：
- `EffectsHandler` 提供 `paintWindow` / `drawWindow` / `renderWindow`、`hideCursor` / `showCursor`、`startInteractiveWindowSelection`、`blocksDirectScanout`、`startMouseInterception`。
- `startMouseInterception` 的语义：拦截后没有窗口会收到鼠标事件，只适合自带界面的全屏效果。
- 效果可通过 D-Bus 接口 `org.kde.kwin.Effects` 的 `toggleEffect` 等方法启停。
- libkwin 的 API / ABI 几乎每个版本都有不兼容变化，效果插件需要跟随 Plasma 版本重新适配。

**待核实**（不得当作事实使用）：
- libkwin 是否导出 `PointerInputRedirection` 的相关符号；`kwin-devel` 是否带内部头文件。
- 原窗口不被正常绘制时的帧回调行为。
- `OffscreenEffect` 在 6.7.5 中的可用方式，以及多遍着色器在 KWin 的 OpenGL 上下文里怎么组织。
- KWin 内部是否有现成的窗口输入坐标变换可以复用。
- KWin 的 OpenGL 上下文能否使用 compute shader（决定能否跑重型 CNN）。
- 效果开启后对直通扫描的实际影响（预期会阻止，延迟比 Windows 上的 Magpie 稍高）。

## 7. 开发环境

```bash
sudo dnf install kwin-devel cmake extra-cmake-modules gcc-c++ \
  libepoxy-devel wayland-devel libdrm-devel libxkbcommon-devel \
  qt6-qtbase-devel qt6-qtbase-private-devel qt6-qtdeclarative-devel \
  kf6-kglobalaccel-devel kf6-kconfig-devel kf6-kcoreaddons-devel kf6-ki18n-devel

# 参考源码，版本必须与系统一致
git clone https://invent.kde.org/plasma/kwin.git
cd kwin && git checkout v6.7.5
```

缺什么依赖按 cmake 报错补。参考效果可看 KWin 源码 `src/plugins/` 下的 `zoom`、`magnifier`、`overview`。网上现成的第三方效果模板多为 Plasma 5 时代，只能参考目录骨架。

## 8. 建议的仓库结构

```
effect/        KWin C++ 效果插件（热键、取窗口画面、放大绘制、光标、输入映射）
shaders/       GLSL 着色器链（与 KWin 解耦，供插件和未来的嵌套合成器共用）
packaging/     RPM spec（面向 COPR）
docs/FINDINGS.md   每个关卡的实验结论与证据
AGENTS.md      本文件
```

## 9. 给 agent 的工作规则

1. **以 KWin v6.7.5 源码和系统安装的头文件为准**，不要凭记忆写 KWin API；不确定时先在源码里查，查不到就标 **[待核实]** 并告诉用户。
2. **按关卡顺序推进，每个关卡结束停下汇报。** 不要同时展开多个关卡，也不要在 Gate 1 没结论时就开始写输入映射。
3. 每次实验的结论、命令输出、失败原因写进 `docs/FINDINGS.md`，区分“实测”和“推测”。
4. **插件在 KWin 进程里运行，崩溃会带崩真实桌面。** 尽量在嵌套 / 测试会话里调试（具体启动方式以 KWin 文档和源码为准，**[待核实]**）。在真实会话里测试前，确认崩溃后能从命令行禁用插件：
   ```bash
   kwriteconfig6 --file kwinrc --group Plugins --key <插件ID>Enabled false
   ```
   然后重启 KWin。
5. 着色器链与 KWin 代码保持解耦，不要让着色器逻辑依赖 KWin 的类型。
6. 改动保持小步、可回退；不要为了让某个关卡“看起来通过”而放宽通过标准。

## 10. 许可证注意事项（不是法律意见）

- KWin 源码头标注 GPL-2.0-or-later，效果插件会链接 libkwin，应按 GPL 兼容处理。如果将来考虑闭源或商业分发，需要先评估。
- 如果要移植 Magpie 的着色器或代码，先核对其许可证条款。
- Anime4K、FSR、CAS 等算法的实现各有自己的许可证，引入前逐个确认。

## 11. 待用户确认的问题

1. 第 3 节的 MUST / SHOULD / WON'T 划分是否符合预期？
2. 主要使用场景是什么：Proton 游戏、原生程序 / 模拟器，还是视频？这会影响算法优先级与相对鼠标的重要程度。
3. 是否接受“仅支持 KDE Plasma”？
4. 路线 C（KWin 插件）是否确认作为主路线，Gate 1 失败时是否同意退回方案 B？
5. 项目名称与许可证选择。
