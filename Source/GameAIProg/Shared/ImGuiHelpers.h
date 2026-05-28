#pragma once

#include <functional>
#include "imgui.h"

namespace ImGuiHelpers
{
	inline bool ImGuiSliderFloatWithSetter(const char* Label, float const CurrentValue, float const MinVal, float const MaxVal, 
	                                       std::function<void(float)> const & Setter, const char* Format = "%.1f")
	{
		float LocalValue = CurrentValue;
		bool bHasChanged = ImGui::SliderFloat(Label, &LocalValue, MinVal, MaxVal, Format);

		if (bHasChanged && LocalValue != CurrentValue)
		{
			Setter(LocalValue);
		}

		return bHasChanged;
	}
}
