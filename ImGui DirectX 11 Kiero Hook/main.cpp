#include "includes.h"
#include "globals.h"
#include "menukey.h"
#include "menustyle.h"
#include "menu.h"
#include "settings.h"
#include "movement.h"
#include "esp.h"
#include "game.h"
#include "glowhook.h"
#include "cs2_offsets.h"
#include "interfaces.h"
#include "debuglog.h"
#include <iostream>
//#include "imgui/imgui_demo.cpp"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Present oPresent;
HWND window = NULL;
WNDPROC oWndProc;
ID3D11Device* pDevice = NULL;
ID3D11DeviceContext* pContext = NULL;
ID3D11RenderTargetView* mainRenderTargetView;







void InitImGui()
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.IniFilename = nullptr;            // don't write imgui.ini into the game folder
	ImGui::StyleColorsDark();            // the custom menu draws its own look on top
	menu::init_fonts();
	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX11_Init(pDevice, pContext);
}




LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	// Menu CLOSED: stay completely out of the way. Do not even touch ImGui -
	// the game gets every message untouched, so input works exactly as normal.
	if (!globals::show)
		return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);

	// Menu OPEN: feed ImGui, then swallow input messages so the game behind the
	// menu doesn't react (camera turning, firing, etc.). Non-input messages
	// (paint/size/...) still fall through to the game.
	ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

	switch (uMsg)
	{
	case WM_MOUSEMOVE: case WM_NCMOUSEMOVE:
	case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
	case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
	case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
	case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP:
	case WM_CHAR: case WM_SETCURSOR: case WM_INPUT:
		return true;
	}

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}






























bool init = false;
HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (!init)
	{
		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)& pDevice)))
		{
			pDevice->GetImmediateContext(&pContext);
			DXGI_SWAP_CHAIN_DESC sd;
			pSwapChain->GetDesc(&sd);
			window = sd.OutputWindow;
			ID3D11Texture2D* pBackBuffer;
			pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)& pBackBuffer);
			pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
			pBackBuffer->Release();
			oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
			InitImGui();
			init = true;
		}

		else
			return oPresent(pSwapChain, SyncInterval, Flags);
	}

	// Resolve schema offsets + global signatures. schema::init is cheap and the
	// heavy sig-scan is guarded to run once, so just try every frame until ready.
	static bool gameReady = false;
	if (!gameReady)
		gameReady = game::init();

	// Install the movement (create-move) hook automatically once client.dll is
	// ready. Throttled + capped so a missing/retrying signature can't turn into
	// a full pattern-scan of client.dll every frame.
	static bool movementReady = false;
	static int  movementTries = 0;
	static int  frameCounter = 0;
	if (!movementReady && movementTries < 12 && (++frameCounter % 90) == 0)
	{
		movementReady = movement::initialize();
		++movementTries;
	}

	// Install the flicker-free glow hook (CGlowProperty::IsGlowing / GetGlowColor)
	// once client.dll is up. Throttled + capped like the movement hook.
	static bool glowReady = false;
	static int  glowTries = 0;
	if (!glowReady && glowTries < 12 && (frameCounter % 90) == 10)
	{
		glowReady = glowhook::init();
		++glowTries;
	}






//	if (GetAsyncKeyState(VK_INSERT) & 1) {
//		globals::show = !globals::show;
	//}


	// Toggle the menu on a clean INSERT key edge (once per physical press).
	static bool insertWasDown = false;
	const bool insertDown = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
	if (insertDown && !insertWasDown)
		globals::show = !globals::show;
	insertWasDown = insertDown;

	// Show a cursor only while the menu is open.
	ImGui::GetIO().MouseDrawCursor = globals::show;

	// While the menu is open, free the OS cursor so it can click the menu.
	// Game mouse input (camera/firing) is blocked via the MouseInputEnabled
	// hook in movement.cpp, which is the reliable way for CS2's raw input.
	if (globals::show)
		ClipCursor(nullptr);

	// (The ImGui frame, visuals and present now ALL run every frame below,
	// whether or not the menu is open, so ESP/glow/tracers always draw.)








	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	// Make ImGui's coordinate space match the ACTUAL backbuffer. On high-DPI or
	// non-native resolutions the window client rect (logical pixels) differs from
	// the backbuffer (physical pixels); without this the menu renders scaled into
	// a corner and mouse clicks land in the wrong spot (only the title bar seems
	// to react). Must run AFTER ImGui_ImplWin32_NewFrame, which sets these too.
	{
		ImGuiIO& io = ImGui::GetIO();
		DXGI_SWAP_CHAIN_DESC sd;
		pSwapChain->GetDesc(&sd);
		const float bbW = (float)sd.BufferDesc.Width;
		const float bbH = (float)sd.BufferDesc.Height;
		if (bbW > 0.0f && bbH > 0.0f)
		{
			io.DisplaySize = ImVec2(bbW, bbH);

			POINT p{};
			GetCursorPos(&p);
			ScreenToClient(window, &p);
			RECT cr{};
			GetClientRect(window, &cr);
			const float cw = (float)(cr.right - cr.left);
			const float ch = (float)(cr.bottom - cr.top);
			if (cw > 0.0f && ch > 0.0f)
				io.MousePos = ImVec2(p.x * (bbW / cw), p.y * (bbH / ch));
		}
	}

	ImGui::NewFrame();

	if (globals::show)
		menu::render();

#if 0  // --- old inline menu, replaced by the styled menu::render() above ---
	ImGui::SetNextWindowSize(ImVec2(650, 700));     //standart windowsize is 950, 950
	ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_Once);
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);  // never start collapsed (no arrow needed)
	ImGui::Begin("Movement Internal  |  build " __DATE__ " " __TIME__, NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
	{
		ImGui::SameLine();

		if (ImGui::BeginTabBar("##Tabs"))
		{
			if (ImGui::BeginTabItem("Visuals"))
		
			{
				ImGui::Text("Visuals");


				ImGui::Checkbox("Snaplines", &showTracers);
				ImGui::SameLine();
				ImGui::ColorEdit4("Snaplines Color", (float*)&tracersColor);
				ImGui::Checkbox("Enable Glow", &glow);
				ImGui::SameLine();
				ImGui::ColorEdit4("Glow Color", (float*)&glowColor);

				ImGui::Text("Adjust Snaplines Dimensions");
				static const char* tracerPositionOptions[] = {
					"Bottom Center", "Screen Center", "Top Left",
					"Top Right", "Top Center", "Bottom Left", "Bottom Right"
				};
				static int selectedTracerPosition = static_cast<int>(tracerStartPosition);

				if (ImGui::Combo("Tracer Start Position", &selectedTracerPosition, tracerPositionOptions, IM_ARRAYSIZE(tracerPositionOptions)))
				{
					tracerStartPosition = static_cast<TracerStartPosition>(selectedTracerPosition);
				}

				ImGui::SliderFloat("Tracer Thickness", &SnaplinesThickness, 1.0f, 5.0f, "%.1f");






				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Aimbot"))
			{


				ImGui::Text("Aimbot");

				

				ImGui::EndTabItem();
			

			}
			if (ImGui::BeginTabItem("Rage"))
			{
				ImGui::Text("Rage");






				ImGui::EndTabItem();

			}

			if (ImGui::BeginTabItem("Player"))
			{
				ImGui::Text("Player");





				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Movement"))
			{
				ImGui::Text("Movement");
				ImGui::Separator();
				ImGui::TextDisabled("Use the main menu (INSERT) -> Movement tab.");
				ImGui::TextDisabled("Bhop / AutoStrafe / EdgeBug / SmartEdgeBug /");
				ImGui::TextDisabled("JumpBug / PixelSurf / TextureBug - each with a");
				ImGui::TextDisabled("key bind + Always/Hold/Toggle mode.");
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Misc"))
			{
				ImGui::Text("Misc");

				// Checkbox zum Aktivieren/Deaktivieren
				ImGui::Checkbox("Enable FOV Changer", &showFOVChanger);

				// Slider zur Anpassung des FOV-Werts (wenn aktiviert)
				if (showFOVChanger) {
					ImGui::SliderInt("FOV Value", &currentFOV, 70, 400); // Slider von 70 bis 140


				}

				ImGui::SameLine(); 
			 	HelpMarker("Fov Changer is a bit laggy !!", true);  

				ImGui::Checkbox("Enable Anti-Flash", &antiFlashEnabled); 
				ImGui::SameLine(); 
				 HelpMarker(" only enable this in a round or it result in a crash !!!!!antiflash may cause a crash !!", true); 

				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Settings"))
			{
				ImGui::Text("Settings");


	




				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		float availableHeight = ImGui::GetContentRegionAvail().y;

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + availableHeight - ImGui::GetTextLineHeight());

		ImVec4 textColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
		ImGui::TextColored(textColor, "Menu toggle keybind: INSERT");

		
	//} 
	ImGui::End();
	}
#endif  // --- end old inline menu ---

	// visuals (glow + 2D box) draw every frame, menu open or not
	esp::render();

	ImGui::Render();

	pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	return oPresent(pSwapChain, SyncInterval, Flags);
}




DWORD WINAPI MainThread(LPVOID lpReserved)
{
	dbg::init();   // debug console (Debug build) + crash log
	bool init_hook = false;
	do
	{
		if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success)
		{
			kiero::bind(8, (void**)& oPresent, hkPresent);
			init_hook = true;
		}
	} while (!init_hook);
	return TRUE;
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hMod);
		CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
		break;
	case DLL_PROCESS_DETACH:
		movement::shutdown();
		glowhook::shutdown();
		kiero::shutdown();
		break;
	}
	return TRUE;
}