#include "menu.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "settings.h"
#include "game.h"
#include "schema.h"
#include "interfaces.h"
#include "movement.h"
#include <Windows.h>
#include <cstdint>
#include <string>

// These headers DEFINE globals (main_color, icon[]); include in this TU only.
#include "utils.h"     // ImVec4 main_color
#include "Header.h"    // unsigned char icon[]  (embedded icon font)

ImFont* font = nullptr;

void menu::init_fonts()
{
    ImGuiIO& io = ImGui::GetIO();

    ImFontConfig cfg;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = 1;

    static const ImWchar ranges[] = { 0x0020, 0x00FF, 0x0400, 0x044F, 0 };

    io.Fonts->AddFontFromMemoryTTF((void*)icon, sizeof(icon), 44.0f, &cfg, ranges);
    font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\verdana.ttf", 13.0f, &cfg, ranges);
    if (!font)
        font = io.Fonts->AddFontDefault();
}

// accent-tinted widgets --------------------------------------------------------
static void a_checkbox(const char* label, bool* v)
{
    // The project's custom Checkbox already paints the reference pink fill; the
    // style pushes are harmless and keep behaviour identical if the widget falls
    // back to the stock path.
    ImGui::PushStyleColor(ImGuiCol_CheckMark, main_color);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ImGui::Checkbox(label, v);
    ImGui::PopStyleColor(2);
}

static void a_color(const char* label, float col[4])
{
    // Accent-tint the swatch + picker popup so it reads as part of the pink theme
    // instead of stock ImGui blue, and keep a clean right-aligned swatch.
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.16f, 0.16f, 0.19f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.22f, 0.22f, 0.26f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button,         main_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  main_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   main_color);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,     main_color);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, main_color);
    ImGui::PushStyleColor(ImGuiCol_PopupBg,        ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(main_color.x, main_color.y, main_color.z, 0.5f));

    ImGui::ColorEdit4(label, col,
        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_DisplayRGB);

    ImGui::PopStyleColor(9);
}

// A bind row: [enable] Name            [key]   [Always/Hold/Toggle]
static Bind* g_capturing = nullptr;
static void draw_bind(const char* label, Bind& b)
{
    ImGui::PushID(label);
    ImGui::PushStyleColor(ImGuiCol_CheckMark, main_color);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ImGui::Checkbox(label, &b.enabled);
    ImGui::PopStyleColor(2);

    // Accent-tint the stock key button + mode combo so they match the pink menu
    // (without this they render in ImGui's default blue).
    const ImVec4 accent    = main_color;
    const ImVec4 accentHov = ImVec4(main_color.x * 1.12f, main_color.y * 1.12f, main_color.z * 1.12f, 1.0f);
    const ImVec4 accentDim = ImVec4(0.20f, 0.20f, 0.23f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        accentDim);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  accentHov);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        accentDim);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.26f, 0.26f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(0.30f, 0.30f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Header,         accent);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,  accentHov);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,   accentHov);
    ImGui::PushStyleColor(ImGuiCol_Text,           ImVec4(0.92f, 0.92f, 0.94f, 1.0f));

    const bool capturing = (g_capturing == &b);
    ImGui::SameLine(300.0f);
    ImGui::SetNextItemWidth(70.0f);
    if (ImGui::Button(capturing ? "..." : key_name(b.key), ImVec2(70, 0)))
        g_capturing = capturing ? nullptr : &b;

    ImGui::SameLine(380.0f);
    const char* modes[] = { "Always", "Hold", "Toggle" };
    ImGui::SetNextItemWidth(95.0f);
    ImGui::Combo("##m", &b.mode, modes, 3);

    ImGui::PopStyleColor(10);
    ImGui::PopID();

    if (capturing)
    {
        int k = b.key;
        if (bind_capture(k)) { b.key = k; g_capturing = nullptr; }
    }
}

// ----------------------------------------------------------------------------
// Panel content, split per (tab, subtab) exactly like the reference's
// Main Group / Modifiers two-column layout.
// ----------------------------------------------------------------------------
static void visuals_general_left()   // "Main Group" checkbox list
{
    a_checkbox("2D Box",     &settings::visuals::box);
    a_checkbox("Glow",       &settings::visuals::glow);
    a_checkbox("Skeleton",   &settings::visuals::skeleton);
    a_checkbox("Name",       &settings::visuals::name);
    a_checkbox("Health Bar", &settings::visuals::healthbar);
}
static void visuals_general_right()  // "Modifiers" color pickers
{
    a_color("Box Color",      settings::visuals::box_color);
    a_color("Glow Color",     settings::visuals::glow_color);
    a_color("Skeleton Color", settings::visuals::skeleton_color);
    a_color("Name Color",     settings::visuals::name_color);
}
static void visuals_advanced_left()
{
    a_checkbox("HP Gradient", &settings::visuals::hp_gradient);
}
static void visuals_advanced_right()
{
    if (settings::visuals::hp_gradient) {
        a_color("HP Top",    settings::visuals::hp_top);
        a_color("HP Bottom", settings::visuals::hp_bottom);
    } else {
        a_color("HP Color",  settings::visuals::hp_color);
    }
}

static void render_content(int tab);

void menu::render()
{
    static int tab = 0;

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 gs = ImVec2(600, 420);
    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - gs.x) * 0.5f,
                                   (io.DisplaySize.y - gs.y) * 0.5f), ImGuiCond_Once);
    // Reference menu is a fixed-size window (the panels are laid out at absolute
    // pixel positions), so force the size every frame to match it 1:1.
    ImGui::SetNextWindowSize(gs);
    ImGui::Begin("##GUI", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);

    const ImVec2 s = ImVec2(ImGui::GetWindowSize().x - ImGui::GetStyle().WindowPadding.x * 2,
                            ImGui::GetWindowSize().y - ImGui::GetStyle().WindowPadding.y * 2);
    const ImVec2 p = ImVec2(ImGui::GetWindowPos().x + ImGui::GetStyle().WindowPadding.x,
                            ImGui::GetWindowPos().y + ImGui::GetStyle().WindowPadding.y);
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // --- window shell -------------------------------------------------------
    draw->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + s.x, p.y + s.y), ImColor(8, 8, 8), 4);                               // bg
    draw->AddRect(ImVec2(p.x + 1, p.y + 1), ImVec2(p.x + s.x - 1, p.y + s.y - 1), ImColor(27, 27, 27, 255), 4.5f);          // outline
    draw->AddRect(ImVec2(p.x + 10, p.y + 35.3f), ImVec2(p.x + s.x - 480, p.y + s.y - 36.85f), ImColor(26, 26, 26, 255));    // subtab column box
    draw->AddLine(ImVec2(p.x, p.y + s.y - 27), ImVec2(p.x + s.x, p.y + s.y - 27), ImColor(27, 27, 27, 255));                // footer separator
    draw->AddLine(ImVec2(p.x, p.y + 25), ImVec2(p.x + s.x, p.y + 25), ImColor(27, 27, 27, 255));                            // tab separator

    // pink fade on the tab separator
    const int fade = 60; const float center = s.x / 2.0f;
    for (int i = 0; i < fade; i++) {
        float a = 1.0f - (i * (1.0f / fade));
        ImColor c(main_color.x, main_color.y, main_color.z, a);
        draw->AddLine(ImVec2(p.x + s.x - i * (center / fade), p.y + 25),
                      ImVec2(p.x + s.x - (i + 1) * (center / fade), p.y + 25), c);
    }

    // --- title + footer -----------------------------------------------------
    ImGui::PushFont(font);
    draw->AddText(ImVec2(p.x + 9.5f, p.y + 7), ImColor(main_color), "gandronima");

    const float fy = p.y + s.y - 20;
    draw->AddText(ImVec2(p.x + 9.5f, fy), ImColor(255, 255, 255, 100), "Build:");
    draw->AddText(ImVec2(p.x + 41, fy), ImColor(main_color), "Live");
    const float live_w = ImGui::CalcTextSize("Live ").x;
    draw->AddText(ImVec2(p.x + 41 + live_w, fy), ImColor(255, 255, 255, 100), "14190");
    const float sexy_w = ImGui::CalcTextSize("sexy").x;
    draw->AddText(ImVec2(p.x + s.x - sexy_w - 6, fy), ImColor(main_color), "sexy");
    ImGui::PopFont();

    // --- top tab row --------------------------------------------------------
    ImGui::PushFont(font);
    ImGui::SetCursorPosX(112); ImGui::SetCursorPosY(10);
    ImGui::BeginGroup();
    if (ImGui::tab("Visuals",  tab == 0)) { tab = 0; } ImGui::SameLine();
    if (ImGui::tab("Movement", tab == 1)) { tab = 1; } ImGui::SameLine();
    if (ImGui::tab("Misc",     tab == 2)) { tab = 2; }
    ImGui::EndGroup();
    ImGui::PopFont();

    ImGui::PushFont(font);
    render_content(tab);
    ImGui::PopFont();

    ImGui::End();
}

static void draw_subtabs(int& subtab)
{
    ImGui::SetCursorPosY(54); ImGui::SetCursorPosX(30);
    if (ImGui::subtab("General",  subtab == 0)) subtab = 0;
    ImGui::SetCursorPosY(79); ImGui::SetCursorPosX(30);
    if (ImGui::subtab("Advanced", subtab == 1)) subtab = 1;
}

static void render_content(int tab)
{
    static int subtab = 0;

    if (tab == 0) // ---------------------------------------------------- Visuals
    {
        draw_subtabs(subtab);

        ImGui::SetCursorPosY(38); ImGui::SetCursorPosX(122);
        ImGui::MenuChild("Main Group", ImVec2(226, 337), false);
        if (subtab == 0) visuals_general_left();
        else             visuals_advanced_left();
        ImGui::EndChild();

        ImGui::SetCursorPosY(38); ImGui::SetCursorPosX(358);
        ImGui::MenuChild("Modifiers", ImVec2(224, 337), false);
        if (subtab == 0) visuals_general_right();
        else             visuals_advanced_right();
        ImGui::EndChild();
    }
    else if (tab == 1) // --------------------------------------------- Movement
    {
        draw_subtabs(subtab);

        // bind rows are wide, so Movement uses a single wide "Main Group" panel.
        ImGui::SetCursorPosY(38); ImGui::SetCursorPosX(122);
        ImGui::MenuChild("Main Group", ImVec2(460, 337), false);
        if (subtab == 0)
        {
            draw_bind("Bhop",          settings::movement::bhop);
            draw_bind("Auto Strafe",   settings::movement::autostrafe);
            draw_bind("Edge Bug",      settings::movement::edgebug);
            draw_bind("Smart EdgeBug", settings::movement::smart_edgebug);
            draw_bind("Jump Bug",      settings::movement::jumpbug);
            draw_bind("Pixel Surf",    settings::movement::pixelsurf);
            draw_bind("Texture Bug",   settings::movement::texturebug);
        }
        else
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.65f, 1.0f),
                "Tick a feature on, click its key box and press a key,");
            ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.65f, 1.0f),
                "then choose Always / Hold / Toggle.");
            ImGui::Spacing();
            if (movement::is_hooked())
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "move hook: ACTIVE");
            else
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "move hook: inactive");
            ImGui::SameLine();
            ImGui::Text("| fired: %lu", movement::hook_calls());
        }
        ImGui::EndChild();
    }
    else // ---------------------------------------------------------------- Misc
    {
        ImGui::SetCursorPosY(38); ImGui::SetCursorPosX(122);
        ImGui::MenuChild("Main Group", ImVec2(226, 337), false);
        ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "Menu key: INSERT");
        ImGui::EndChild();

        ImGui::SetCursorPosY(38); ImGui::SetCursorPosX(358);
        ImGui::MenuChild("Modifiers", ImVec2(224, 337), false);
        ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "Build 14190");
        ImGui::EndChild();
    }
}

