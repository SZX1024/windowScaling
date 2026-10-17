# FINDINGS — 实验结论与证据记录

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

### 结论与后续步骤
- **Gate 1 核心可行性结论**: 符号导出假设成立！KWin 6.7.5 的 `libkwin.so.6` 导出了关键的指针重定向与 warp 接口。
- **头文件情况**: 系统未安装 `kwin-devel`（缺少公开开发包），但由于符号已导出，可以通过克隆 KWin v6.7.5 源码并将 `src/` 包含进编译路径，直接调用这些内部接口。
