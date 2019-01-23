#include "imgui_user.h"
#include "../../Common_3/ThirdParty/OpenSource/imgui/imgui_internal.h"

#ifndef M_PI
#define M_PI 3.14159f
#endif

namespace ImGui {

int KnobFloat(
	const char* label, float* value_p, const float& step, const float& minv, const float& maxv, const char* format,
	const float& minimumHitRadius, bool doubleTapForText, bool spawnText)
{
	ImGuiWindow* window = GetCurrentWindow();
	if (window->SkipItems)
		return 0;

	ImGuiContext&     g = *GImGui;
	const ImGuiStyle& style = g.Style;

	const ImGuiID id = window->GetID(label);
	float         size = CalcItemWidth();

	float2 extraPadding = style.ItemSpacing + style.ItemInnerSpacing;
	//extraPadding.y = 0.f;
	float line_height = ImGui::GetTextLineHeight();
	//extraPadding *= 2.f;
	float2 label_size = CalcTextSize(label, NULL, true);
	label_size.y = fmaxf(label_size.y, 0.f) + line_height;
	ImRect total_bb(window->DC.CursorPos, window->DC.CursorPos + float2(size, size + label_size.y / 2.f) + extraPadding);

	float knobRadius = size * 0.5f;

	float2 startPos = window->DC.CursorPos;
	float2 center = total_bb.GetCenter();

	const ImRect inner_bb(center - float2(knobRadius, knobRadius), center + float2(knobRadius, knobRadius));

	//bool hovered, held;
	//Create Invisible button and check if its active/hovered
	float2 mousePos = ImGui::GetIO().MousePos;

	// Tabbing or CTRL-clicking on Drag turns it into an input box
	bool       start_text_input = false;
	bool       hovered = ItemHoverable(inner_bb, id);
	const bool tab_focus_requested = FocusableItemRegister(window, id);

	if (doubleTapForText && (tab_focus_requested || (hovered && (g.IO.MouseClicked[0] || g.IO.MouseDoubleClicked[0])) ||
							 g.NavActivateId == id || (g.NavInputId == id && g.ScalarAsInputTextId != id)))
	{
		SetActiveID(id, window);
		SetFocusID(id, window);
		FocusWindow(window);
		g.ActiveIdAllowNavDirFlags = (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
		if (tab_focus_requested || g.IO.KeyCtrl || g.IO.MouseDoubleClicked[0] || g.NavInputId == id)
		{
			start_text_input = true;
			g.ScalarAsInputTextId = 0;
		}
	}
	bool using_text_input = false;
	if (start_text_input || (g.ActiveId == id && g.ScalarAsInputTextId == id))
	{
		if (spawnText)
		{
			int retValue = (int)InputScalarAsWidgetReplacement(inner_bb, id, label, ImGuiDataType_Float, value_p, format);

			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			// Bottom Label (X, Y, Z, W) Goes Below Knob
			label_size = CalcTextSize(label, NULL, true);
			uint32_t col32text = ImGui::GetColorU32(ImGuiCol_Text);
			float2   textpos = center + float2(-label_size.x / 2.f, inner_bb.GetHeight() / 2.f + label_size.y / 2.f);
			draw_list->AddText(textpos, col32text, label);

			// if max and min are not equal only then we apply the limits
			// to the final value
			if (!(fabs(maxv - minv) <= ((fabs(maxv) < fabs(minv) ? fabs(minv) : fabs(maxv)) * 0.01f)))
			{
				value_p[0] = fmaxf(minv, fminf(value_p[0], maxv));
			}

			return retValue;
		}
		else
			//Return value to indicate if text was requested
			using_text_input = true;
	}

	//Create Invisible button and check if its active/hovered
	ImGui::InvisibleButton(label, total_bb.GetSize());

	bool isActive = ImGui::IsItemActive();
	bool updatedKnob = false;

	float currentAngle = 0.f;
	// Mouse delta movement since last frame
	float2 mouseDelta = ImGui::GetIO().MouseDelta;
	// Previous Mouse position
	float2 mousePrevPos = mousePos - mouseDelta;
	// Vector from center of knob to previous mouse position
	float2 mousePrevToKnob = mousePrevPos - center;
	// Get the length
	float lengthPrev = sqrtf(mousePrevToKnob.x * mousePrevToKnob.x + mousePrevToKnob.y * mousePrevToKnob.y);
	// Get Vector from center of knob to current mouse position
	float2 mouseToKnob = mousePos - center;
	// Get the length
	float lengthCurr = sqrtf(mouseToKnob.x * mouseToKnob.x + mouseToKnob.y * mouseToKnob.y);

	if (lengthCurr > knobRadius && g.ActiveId != id)
		isActive = false;
	const float hitRadius = minimumHitRadius * knobRadius;
	if (isActive && g.ActiveIdPreviousFrame == id)
	{
		// sanity check in case user clicks on center of knob.
		// wait for 3 frames before activating the knob
		const float delay = ((1.f / g.IO.Framerate) * 3.f);
		if (lengthPrev > 0.001f && lengthCurr > hitRadius && g.ActiveIdTimer > delay)
		{
			//Normalize our vectors
			mousePrevToKnob /= lengthPrev;
			mouseToKnob /= lengthCurr;

			// Function returns true if we have modified a value, not just activated it.
			updatedKnob = true;

			// dot product between[x1, y1] and [x2, y2]
			float dot = mouseToKnob.x * mousePrevToKnob.x + mouseToKnob.y * mousePrevToKnob.y;
			// determinant
			float det = mouseToKnob.x * mousePrevToKnob.y - mouseToKnob.y * mousePrevToKnob.x;

			// Get the new angle to add. Needs to be in degrees
			float deltaAngle = atan2f(det, dot) * 180.f / M_PI;

			// Increment current value
			value_p[0] += deltaAngle * step / fmaxf(1.f, lengthCurr / knobRadius);

			// if max and min are not equal only then we apply the limits
			// to the final value
			if (!(fabs(maxv - minv) <= ((fabs(maxv) < fabs(minv) ? fabs(minv) : fabs(maxv)) * 0.01f)))
			{
				value_p[0] = fmaxf(minv, fminf(value_p[0], maxv));
			}

			float lineAngle = atan2f(mouseToKnob.y, mouseToKnob.x) - M_PI / 2.0f;

			// TODO: Add magnitude to increase step. (Multiples of initial length)
			currentAngle = lineAngle;
		}
	}

	// Display current value (Final value after knob increments)
	char textval[32];
	ImFormatString(textval, IM_ARRAYSIZE(textval), format, value_p[0]);

	uint32_t    col32 = ImGui::GetColorU32(isActive ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
	uint32_t    col32line = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	// Enable to draw debug rectangles to view the frames being used
	// [DEBUGGING]
	//draw_list->AddRect(total_bb.Min, total_bb.Max, ImColor(0.0f, 1.0f, 0.0f, 1.0f));
	//draw_list->AddRect(inner_bb.Min, inner_bb.Max, ImColor(0.0f, 0.0f, 1.0f, 1.0f));

	// Draw knob as Filled circle
	// We could add textures to it in the future.
	draw_list->AddCircleFilled(center, knobRadius, col32, 32);

	// Cover the part that can't be dragged
	// i.e: Create hole in center
	if (hitRadius > 0.f)
		draw_list->AddCircleFilled(center, hitRadius, ImGui::GetColorU32(ImGuiCol_WindowBg), 32);

	// Add line from center of knob to edge of circle along mouse direction
	float x2 = -sinf(currentAngle) * knobRadius + center.x;
	float y2 = cosf(currentAngle) * knobRadius + center.y;
	draw_list->AddLine(center, float2(x2, y2), col32line, 1);

	// Current Value of Widget (Above Knob)
	label_size = CalcTextSize(textval, NULL, true);
	float2 textpos = center - float2(label_size.x / 2.f, total_bb.GetHeight() / 2.f);
	RenderText(textpos, textval);

	// Bottom Label (X, Y, Z, W) Goes Below Knob
	label_size = CalcTextSize(label, NULL, true);
	textpos = center + float2(-label_size.x / 2.f, inner_bb.GetHeight() / 2.f);
	RenderText(textpos, label);

	if (using_text_input)
		return 2;
	return (int)updatedKnob;
}

int KnobFloatN(
	const char* label, float* value_p, const int& components, const float& step, const float& minv, const float& maxv, const char* format,
	const float& minimumHitRadius, float windowWidthRatio, bool doubleTapForText, bool spawnText, float framePaddingScale)
{
	ImGuiWindow* window = GetCurrentWindow();
	if (window->SkipItems)
		return false;

	int value_changed = 0;
	// Print label of widget
	TextUnformatted(label, FindRenderedTextEnd(label));
	// Start group. if app calls IsItemActive, then this enables all the elements created to affect the state of widget.
	BeginGroup();
	// Looks better with some padding before titles such as "Translation, Rotation, Scale"
	PushID(label);

	//Controls how much padding for start of knobs
	framePaddingScale = fmaxf(framePaddingScale, 1.0f);

	float extraFramePadding = GetStyle().FramePadding.x * framePaddingScale;
	float itemSpacing = GetStyle().ItemSpacing.x * 3;

	// Scale the content based on how much of the width we would like to fill
	float contentWidth = window->Size.x - GetStyle().FrameBorderSize - GetStyle().ScrollbarSize - itemSpacing - extraFramePadding;

	// Compute final size multipler > 0 to avoid undefined behavior
	windowWidthRatio = fmaxf(windowWidthRatio, 0.05f);

	PushMultiItemsWidths(components, contentWidth * windowWidthRatio);
	size_t type_size = sizeof(float);

	SetCursorPosX(extraFramePadding);
	// 4 Component names, this could probably be given as an extra paramater with some helper functions for defaults.
	const char* componentNames[4]{ "x", "y", "z", "w" };
	for (int i = 0; i < components; i++)
	{
		PushID(i);
		int currValue = KnobFloat(componentNames[i % 4], value_p, step, minv, maxv, format, minimumHitRadius, doubleTapForText, spawnText);
		if (value_changed == 0)
			value_changed = currValue > 1 ? currValue + i : currValue;
		SameLine(0, itemSpacing);
		PopID();
		PopItemWidth();

		// update user provided float value
		value_p = (float*)((char*)value_p + type_size);
	}
	PopID();
	EndGroup();

	return value_changed;
}
}    // namespace ImGui
