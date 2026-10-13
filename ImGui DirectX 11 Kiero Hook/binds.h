#pragma once
//
// binds.h - per-feature activation bind: a key + a mode (always / hold / toggle).
//
#include <Windows.h>

enum BindMode { BIND_ALWAYS = 0, BIND_HOLD = 1, BIND_TOGGLE = 2 };

struct Bind
{
    bool enabled = false;   // feature master on/off
    int  key     = 0;       // virtual key (0 = none)
    int  mode    = BIND_ALWAYS;
    bool toggled = false;   // runtime state for TOGGLE
    bool prev    = false;   // edge tracking
};

// Is the feature active THIS frame? Updates toggle state on key edge.
inline bool bind_active(Bind& b)
{
    if (!b.enabled)
        return false;
    if (b.mode == BIND_ALWAYS)
        return true;

    const bool down = b.key != 0 && (GetAsyncKeyState(b.key) & 0x8000) != 0;
    if (b.mode == BIND_HOLD)
        return down;

    // toggle: flip on each fresh press
    if (down && !b.prev)
        b.toggled = !b.toggled;
    b.prev = down;
    return b.toggled;
}

// Short readable name for a virtual key (for the menu button label).
inline const char* key_name(int vk)
{
    static char buf[32];
    if (vk == 0) return "[none]";
    switch (vk)
    {
    case VK_LBUTTON: return "Mouse1";
    case VK_RBUTTON: return "Mouse2";
    case VK_MBUTTON: return "Mouse3";
    case VK_XBUTTON1: return "Mouse4";
    case VK_XBUTTON2: return "Mouse5";
    case VK_SPACE:   return "Space";
    case VK_SHIFT:   return "Shift";
    case VK_CONTROL: return "Ctrl";
    case VK_MENU:    return "Alt";
    case VK_LMENU:   return "LAlt";
    case VK_TAB:     return "Tab";
    case VK_CAPITAL: return "Caps";
    }
    // printable
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z'))
    {
        buf[0] = (char)vk; buf[1] = 0; return buf;
    }
    if (vk >= VK_F1 && vk <= VK_F12) { wsprintfA(buf, "F%d", vk - VK_F1 + 1); return buf; }
    wsprintfA(buf, "0x%02X", vk);
    return buf;
}

// Captures the next pressed key into b.key when "listening". Call while a
// capture is pending; returns true once a key was captured.
inline bool bind_capture(int& out_key)
{
    for (int vk = 1; vk < 256; vk++)
    {
        if (vk == VK_LBUTTON) continue; // left click = confirm, don't capture
        if (GetAsyncKeyState(vk) & 0x8000)
        {
            if (vk == VK_ESCAPE) { out_key = 0; return true; } // escape clears
            out_key = vk;
            return true;
        }
    }
    return false;
}
