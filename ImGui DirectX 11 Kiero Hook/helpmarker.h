#pragma once
#include "ImGui/imgui.h"
//For Helpmarker definition    /// ImGui::SameLine();
//                                           HelpMarker("Closes the Cheat u need to reinject"); 

void HelpMarker(const char* desc, bool useRed = false)
{
	// Save the current color style
	ImVec4 originalColor = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);

	// If useRed is true, change the color to red
	if (useRed)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "(?)");  // Red color
	}
	else
	{
		ImGui::TextDisabled("(?)");  // Default disabled text color
	}

	// Tooltip logic
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}


//to use them use this:

//HelpMarker("This is a tooltip text", false); // Default color
//HelpMarker("This is a tooltip text", true);  // Red color