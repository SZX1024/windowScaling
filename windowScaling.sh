#!/usr/bin/env bash
# windowScaling - Independent Super Resolution Scaling Overlay for Linux Galgames

# Auto-detect XAUTHORITY if not set
if [ -z "$XAUTHORITY" ]; then
    AUTH_FILE=$(ls -t /run/user/$UID/xauth_* 2>/dev/null | head -n 1)
    if [ -n "$AUTH_FILE" ]; then
        export XAUTHORITY="$AUTH_FILE"
    fi
fi

# Ensure DISPLAY defaults to :0 for XWayland
if [ -z "$DISPLAY" ]; then
    export DISPLAY=":0"
fi

# Use xcb platform by default so overlay and game share the same X11 stacking hierarchy
if [ -z "$QT_QPA_PLATFORM" ]; then
    export QT_QPA_PLATFORM="xcb"
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/bin/windowScaling" "$@"
