#pragma once
//
// menu.h - custom styled cheat menu (ported from menu_imgui example).
//
namespace menu
{
    void init_fonts(); // add fonts to the atlas; call during ImGui init
    void render();     // draw the menu; call each frame while it's open
}
