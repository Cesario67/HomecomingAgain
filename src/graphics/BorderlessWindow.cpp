#include "graphics/BorderlessWindow.h"

#include <cmath>

#include "core/Log.h"

namespace hca::graphics {

namespace {

// WS_CAPTION = WS_BORDER | WS_DLGFRAME : on retire explicitement chaque élément du cadre.
constexpr DWORD kFrameStyles = WS_CAPTION | WS_BORDER | WS_DLGFRAME | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX |
                               WS_MAXIMIZEBOX;
constexpr DWORD kFrameExStyles = WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE;

}  // namespace

DWORD BorderlessStyle(DWORD style) noexcept {
    return (style & ~kFrameStyles) | WS_POPUP;
}

DWORD BorderlessExStyle(DWORD exStyle) noexcept {
    return exStyle & ~kFrameExStyles;
}

RECT FitWindowToMonitor(const RECT& monitor, unsigned bufferWidth, unsigned bufferHeight) noexcept {
    const long monitorWidth = monitor.right - monitor.left;
    const long monitorHeight = monitor.bottom - monitor.top;
    if (bufferWidth == 0 || bufferHeight == 0 || monitorWidth <= 0 || monitorHeight <= 0) {
        return monitor;
    }
    // Compare ratios without division: bufferWidth / bufferHeight against monitorWidth / monitorHeight.
    const double bufferCross = static_cast<double>(bufferWidth) * static_cast<double>(monitorHeight);
    const double monitorCross = static_cast<double>(monitorWidth) * static_cast<double>(bufferHeight);
    if (std::fabs(bufferCross - monitorCross) <= 0.01 * monitorCross) {
        return monitor;
    }
    long width = monitorWidth;
    long height = monitorHeight;
    if (bufferCross > monitorCross) {
        // Buffer wider than the monitor: fill the width, bars above and below.
        height = std::lround(static_cast<double>(monitorWidth) * static_cast<double>(bufferHeight) /
                             static_cast<double>(bufferWidth));
    } else {
        // Buffer narrower than the monitor: fill the height, bars on the sides.
        width = std::lround(static_cast<double>(monitorHeight) * static_cast<double>(bufferWidth) /
                            static_cast<double>(bufferHeight));
    }
    const long left = monitor.left + (monitorWidth - width) / 2;
    const long top = monitor.top + (monitorHeight - height) / 2;
    return RECT{left, top, left + width, top + height};
}

bool ApplyBorderless(HWND window, unsigned bufferWidth, unsigned bufferHeight) noexcept {
    if (window == nullptr || !IsWindow(window)) {
        return false;
    }
    try {
        const auto style = static_cast<DWORD>(GetWindowLongW(window, GWL_STYLE));
        const auto exStyle = static_cast<DWORD>(GetWindowLongW(window, GWL_EXSTYLE));
        SetWindowLongW(window, GWL_STYLE, static_cast<LONG>(BorderlessStyle(style)));
        SetWindowLongW(window, GWL_EXSTYLE, static_cast<LONG>(BorderlessExStyle(exStyle)));

        MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        const HMONITOR handle = MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY);
        if (!GetMonitorInfoW(handle, &monitor)) {
            return false;
        }
        const RECT area = FitWindowToMonitor(monitor.rcMonitor, bufferWidth, bufferHeight);
        // SWP_FRAMECHANGED : prend en compte le nouveau style (sinon l'ancien cadre reste dessiné).
        const BOOL moved = SetWindowPos(window, HWND_TOP, area.left, area.top, area.right - area.left,
                                        area.bottom - area.top, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        log::Info("Fenêtre sans bordure : style 0x{:08X} -> 0x{:08X}, position {},{} taille {}x{} ({})", style,
                  BorderlessStyle(style), area.left, area.top, area.right - area.left, area.bottom - area.top,
                  moved ? "ok" : "SetWindowPos a échoué");
        return moved != FALSE;
    } catch (...) {
        return false;
    }
}

}  // namespace hca::graphics
