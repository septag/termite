#include "pch.h"

#define T_CORE_API
#define T_GFX_API
#define T_IMGUI_API
#define T_COMPONENT_API
#include "plugin_api.h"

#include "gfx_utils.h"
#include "gfx_driver.h"

using namespace termite;

static void* getImGuiApi0()
{
	static ImGuiApi_v0 api;
	memset(&api, 0x00, sizeof(api));

	api.begin = static_cast<bool(*)(const char*, bool*, ImGuiWindowFlags)>(ImGui::Begin);
	api.beginWithSize = static_cast<bool(*)(const char*, bool*, const ImVec2&, float, ImGuiWindowFlags)>(ImGui::Begin);
	api.end = ImGui::End;
	api.beginChild = static_cast<bool(*)(const char*, const ImVec2&, bool, ImGuiWindowFlags)>(ImGui::BeginChild);
	api.beginChildId = static_cast<bool(*)(ImGuiID, const ImVec2&, bool, ImGuiWindowFlags)>(ImGui::BeginChild);
	api.endChild = ImGui::EndChild;
	api.getContentRegionMax = ImGui::GetContentRegionMax;
	api.getContentRegionAvail = ImGui::GetContentRegionAvail;
	api.getContentRegionAvailWidth = ImGui::GetContentRegionAvailWidth;
	api.getWindowContentRegionWidth = ImGui::GetWindowContentRegionWidth;
	api.getWindowContentRegionMin = ImGui::GetWindowContentRegionMin;
	api.getWindowContentRegionMax = ImGui::GetWindowContentRegionMax;
	api.getWindowDrawList = ImGui::GetWindowDrawList;
	api.getWindowFont = ImGui::GetWindowFont;
	api.getWindowFontSize = ImGui::GetWindowFontSize;
	api.setWindowFontScale = ImGui::SetWindowFontScale;
	api.getWindowPos = ImGui::GetWindowPos;
	api.getWindowFontSize = ImGui::GetWindowFontSize;
	api.getWindowWidth = ImGui::GetWindowWidth;
	api.getWindowHeight = ImGui::GetWindowHeight;
	api.isWindowCollapsed = ImGui::IsWindowCollapsed;
	api.setNextWindowPos = ImGui::SetNextWindowPos;
	api.setNextWindowPosCenter = ImGui::SetNextWindowPosCenter;
	api.setNextWindowSize = ImGui::SetNextWindowSize;
	api.setNextWindowContentSize = ImGui::SetNextWindowContentSize;
	api.setNextWindowContentWidth = ImGui::SetNextWindowContentWidth;
	api.setNextWindowFocus = ImGui::SetNextWindowFocus;
	api.setWindowCollapsed = ImGui::SetWindowCollapsed;
	api.setNextWindowFocus = ImGui::SetNextWindowFocus;
	api.setNextWindowCollapsed = ImGui::SetWindowCollapsed;
	api.setWindowPos = static_cast<void(*)(const ImVec2&, ImGuiSetCond)>(ImGui::SetWindowPos);
	api.setWindowPosName = static_cast<void(*)(const char*, const ImVec2&, ImGuiSetCond)>(ImGui::SetWindowPos);
	api.setWindowSize = static_cast<void(*)(const ImVec2&, ImGuiSetCond)>(ImGui::SetWindowSize);
	api.setWindowSizeName = static_cast<void(*)(const char*, const ImVec2&, ImGuiSetCond)>(ImGui::SetWindowSize);
	api.setWindowCollapsed = static_cast<void(*)(bool, ImGuiSetCond)>(ImGui::SetWindowCollapsed);
	api.setWindowCollapsedName = static_cast<void(*)(const char*, bool, ImGuiSetCond)>(ImGui::SetWindowCollapsed);
	api.setWindowFocus = static_cast<void(*)()>(ImGui::SetWindowFocus);
	api.setWindowFocusName = static_cast<void(*)(const char*)>(ImGui::SetWindowFocus);
	api.getScrollX = ImGui::GetScrollX;
	api.getScrollY = ImGui::GetScrollY;
	api.getScrollMaxX = ImGui::GetScrollMaxX;
	api.getScrollMaxY = ImGui::GetScrollMaxY;
	api.setScrollX = ImGui::SetScrollX;
	api.setScrollY = ImGui::SetScrollY;
	api.setScrollHere = ImGui::SetScrollHere;
	api.setScrollFromPosY = ImGui::SetScrollFromPosY;
	api.setKeyboardFocusHere = ImGui::SetKeyboardFocusHere;
	api.setStateStorage = ImGui::SetStateStorage;
	api.getStateStorage = ImGui::GetStateStorage;
	api.pushFont = ImGui::PushFont;
	api.popFont = ImGui::PopFont;
	api.pushStyleColor = ImGui::PushStyleColor;
	api.popStyleColor = ImGui::PopStyleColor;
	api.pushStyleVar = static_cast<void(*)(ImGuiStyleVar, float)>(ImGui::PushStyleVar);
	api.pushStyleVarVec2 = static_cast<void(*)(ImGuiStyleVar, const ImVec2&)>(ImGui::PushStyleVar);
	api.popStyleVar = static_cast<void(*)(int)>(ImGui::PopStyleVar);
	api.getColorU32 = static_cast<ImU32(*)(ImGuiCol, float)>(ImGui::GetColorU32);
	api.getColorU32Vec4 = static_cast<ImU32(*)(const ImVec4&)>(ImGui::GetColorU32);
	api.pushItemWidth = ImGui::PushItemWidth;
	api.popItemWidth = ImGui::PopItemWidth;
	api.calcItemWidth = ImGui::CalcItemWidth;
	api.pushTextWrapPos = ImGui::PushTextWrapPos;
	api.popTextWrapPos = ImGui::PopTextWrapPos;
	api.pushAllowKeyboardFocus = ImGui::PushAllowKeyboardFocus;
	api.popAllowKeyboardFocus = ImGui::PopAllowKeyboardFocus;
	api.pushButtonRepeat = ImGui::PushButtonRepeat;
	api.popButtonRepeat = ImGui::PopButtonRepeat;
	api.beginGroup = ImGui::BeginGroup;
	api.endGroup = ImGui::EndGroup;
	api.separator = ImGui::Separator;
	api.sameLine = ImGui::SameLine;
	api.spacing = ImGui::Spacing;
	api.dummy = ImGui::Dummy;
	api.indent = ImGui::Indent;
	api.unindent = ImGui::Unindent;
	api.columns = ImGui::Columns;
	api.nextColumn = ImGui::NextColumn;
	api.getColumnIndex = ImGui::GetColumnIndex;
	api.getColumnOffset = ImGui::GetColumnOffset;
	api.setColumnOffset = ImGui::SetColumnOffset;
	api.getColumnWidth = ImGui::GetColumnWidth;
	api.getColumnsCount = ImGui::GetColumnsCount;
	api.getCursorPos = ImGui::GetCursorPos;
	api.getCursorPosX = ImGui::GetCursorPosX;
	api.getCursorPosY = ImGui::GetCursorPosY;
	api.getCursorStartPos = ImGui::GetCursorStartPos;
	api.getCursorScreenPos = ImGui::GetCursorScreenPos;
	api.setCursorScreenPos = ImGui::SetCursorScreenPos;
	api.alignFirstTextHeightToWidgets = ImGui::AlignFirstTextHeightToWidgets;
	api.getTextLineHeight = ImGui::GetTextLineHeight;
	api.getTextLineHeightWithSpacing = ImGui::GetTextLineHeightWithSpacing;
	api.getItemsLineHeightWithSpacing = ImGui::GetItemsLineHeightWithSpacing;
	api.pushID = static_cast<void(*)(const char*)>(ImGui::PushID);
	api.pushIDStr = static_cast<void(*)(const char*, const char*)>(ImGui::PushID);
	api.pushIDPtr = static_cast<void(*)(const void*)>(ImGui::PushID);
	api.pushIDInt = static_cast<void(*)(int)>(ImGui::PushID);
	api.popID = ImGui::PopID;
	api.getIDStr = static_cast<ImGuiID(*)(const char*)>(ImGui::GetID);
	api.getIDPtr = static_cast<ImGuiID(*)(const void*)>(ImGui::GetID);
	api.getIDSubStr = static_cast<ImGuiID(*)(const char*, const char*)>(ImGui::GetID);
	api.text = ImGui::Text;
	api.textV = ImGui::TextV;
	api.textColored = ImGui::TextColored;
	api.textColoredV = ImGui::TextColoredV;
	api.textDisabled = ImGui::TextDisabled;
	api.textDisabledV = ImGui::TextDisabledV;
	api.textWrapped = ImGui::TextWrapped;
	api.textWrappedV = ImGui::TextWrappedV;
	api.textUnformatted = ImGui::TextUnformatted;
	api.labelText = ImGui::LabelText;
	api.labelTextV = ImGui::LabelTextV;
	api.bullet = ImGui::Bullet;
	api.bulletText = ImGui::BulletText;
	api.bulletTextV = ImGui::BulletTextV;
	api.button = ImGui::Button;
	api.smallButton = ImGui::SmallButton;
	api.invisibleButton = ImGui::InvisibleButton;
	api.image = ImGui::Image;
	api.imageButton = ImGui::ImageButton;
	api.collapsingHeader = ImGui::CollapsingHeader;
	api.checkbox = ImGui::Checkbox;
	api.checkboxFlags = ImGui::CheckboxFlags;
	api.radioButton = static_cast<bool(*)(const char*, bool)>(ImGui::RadioButton);
	api.radioButtonInt = static_cast<bool(*)(const char*, int*, int)>(ImGui::RadioButton);
	api.combo = static_cast<bool(*)(const char*, int*, const char**, int, int)>(ImGui::Combo);
	api.comboZeroSep = static_cast<bool(*)(const char*, int*, const char*, int)>(ImGui::Combo);
	api.comboGetter = static_cast<bool(*)(const char*, int*, bool(*)(void*, int, const char**), void*, int, int)>(ImGui::Combo);
	api.colorButton = ImGui::ColorButton;
	api.colorEdit3 = ImGui::ColorEdit3;
	api.colorEdit4 = ImGui::ColorEdit4;
	api.colorEditMode = ImGui::ColorEditMode;
	api.plotLines = static_cast<void(*)(const char*, const float*, int, int, const char*, float, float, ImVec2, int)>(ImGui::PlotLines);
	api.plotLinesGetter = static_cast<void(*)(const char*, float(*)(void*, int), void*, int, int, const char*, float, float, ImVec2)>(ImGui::PlotLines);
	api.plotHistogram = static_cast<void(*)(const char*, const float*, int, int, const char*, float, float, ImVec2, int)>(ImGui::PlotHistogram);
	api.plotHistogramGetter = static_cast<void(*)(const char*, float(*)(void*, int), void*, int, int, const char*, float, float, ImVec2)>(ImGui::PlotHistogram);
	api.progressBar = ImGui::ProgressBar;
	api.dragFloat = ImGui::DragFloat;
	api.dragFloat2 = ImGui::DragFloat2;
	api.dragFloat3 = ImGui::DragFloat3;
	api.dragFloat4 = ImGui::DragFloat4;
	api.dragFloatRange2 = ImGui::DragFloatRange2;
	api.dragInt = ImGui::DragInt;
	api.dragInt2 = ImGui::DragInt2;
	api.dragInt3 = ImGui::DragInt3;
	api.dragInt4 = ImGui::DragInt4;
	api.dragIntRange2 = ImGui::DragIntRange2;
	api.inputText = ImGui::InputText;
	api.inputTextMultiline = ImGui::InputTextMultiline;
	api.inputFloat = ImGui::InputFloat;
	api.inputFloat2 = ImGui::InputFloat2;
	api.inputFloat3 = ImGui::InputFloat3;
	api.inputFloat4 = ImGui::InputFloat4;
	api.inputInt = ImGui::InputInt;
	api.inputInt2 = ImGui::InputInt2;
	api.inputInt3 = ImGui::InputInt3;
	api.inputInt4 = ImGui::InputInt4;
	api.sliderFloat = ImGui::SliderFloat;
	api.sliderFloat2 = ImGui::SliderFloat2;
	api.sliderFloat3 = ImGui::SliderFloat3;
	api.sliderFloat4 = ImGui::SliderFloat4;
	api.sliderAngle = ImGui::SliderAngle;
	api.sliderInt = ImGui::SliderInt;
	api.sliderInt2 = ImGui::SliderInt2;
	api.sliderInt3 = ImGui::SliderInt3;
	api.sliderInt4 = ImGui::SliderInt4;
	api.vSliderFloat = ImGui::VSliderFloat;
	api.vSliderInt = ImGui::VSliderInt;
	api.treeNode = static_cast<bool(*)(const char*)>(ImGui::TreeNode);
	api.treeNodeFmt = static_cast<bool(*)(const char* str_id, const char* fmt, ...)>(ImGui::TreeNode);
	api.treeNodePtrFmt = static_cast<bool(*)(const void* ptr_id, const char* fmt, ...)>(ImGui::TreeNode);
	api.treeNodeV = static_cast<bool(*)(const char*, const char*, va_list)>(ImGui::TreeNodeV);
	api.treeNodeVPtr = static_cast<bool(*)(const void*, const char*, va_list)>(ImGui::TreeNodeV);
	api.treePush = static_cast<void(*)(const char*)>(ImGui::TreePush);
	api.treePushPtr = static_cast<void(*)(const void*)>(ImGui::TreePush);
	api.treePop = ImGui::TreePop;
	api.setNextTreeNodeOpened = ImGui::SetNextTreeNodeOpened;
	api.selectable = static_cast<bool(*)(const char*, bool, ImGuiSelectableFlags, const ImVec2&)>(ImGui::Selectable);
	api.selectableSel = static_cast<bool(*)(const char*, bool*, ImGuiSelectableFlags, const ImVec2&)>(ImGui::Selectable);
	api.listBox = static_cast<bool(*)(const char*, int*, const char**, int, int)>(ImGui::ListBox);
	api.listBoxGetter = static_cast<bool(*)(const char*, int*, bool(*)(void*, int, const char**), void*, int, int)>(ImGui::ListBox);
	api.listBoxHeader = static_cast<bool(*)(const char*, const ImVec2&)>(ImGui::ListBoxHeader);
	api.listBoxHeader2 = static_cast<bool(*)(const char*, int, int)>(ImGui::ListBoxHeader);
	api.listBoxFooter = static_cast<void(*)()>(ImGui::ListBoxFooter);
	api.valueBool = static_cast<void(*)(const char*, bool)>(ImGui::Value);
	api.valueInt = static_cast<void(*)(const char*, int)>(ImGui::Value);
	api.valueUint = static_cast<void(*)(const char*, unsigned int)>(ImGui::Value);
	api.valueFloat = static_cast<void(*)(const char*, float, const char*)>(ImGui::Value);
	api.valueColor = static_cast<void(*)(const char*, const ImVec4&)>(ImGui::ValueColor);
	api.valueColorUint = static_cast<void(*)(const char*, unsigned int)>(ImGui::ValueColor);

	api.setTooltip = ImGui::SetTooltip;
	api.setTooltipV = ImGui::SetTooltipV;
	api.beginTooltip = ImGui::BeginTooltip;
	api.endTooltip = ImGui::EndTooltip;

	api.beginMainMenuBar = ImGui::BeginMainMenuBar;
	api.endMainMenuBar = ImGui::EndMainMenuBar;
	api.beginMenuBar = ImGui::BeginMenuBar;
	api.endMenuBar = ImGui::EndMenuBar;
	api.beginMenu = ImGui::BeginMenu;
	api.endMenu = ImGui::EndMenu;
	api.menuItem = static_cast<bool(*)(const char*, const char*, bool, bool)>(ImGui::MenuItem);
	api.menuItemSel = static_cast<bool(*)(const char*, const char*, bool*, bool)>(ImGui::MenuItem);

	api.openPopup = ImGui::OpenPopup;
	api.beginPopup = ImGui::BeginPopup;
	api.beginPopupModal = ImGui::BeginPopupModal;
	api.beginPopupContextItem = ImGui::BeginPopupContextItem;
	api.beginPopupContextWindow = ImGui::BeginPopupContextWindow;
	api.beginPopupContextVoid = ImGui::BeginPopupContextVoid;
	api.endPopup = ImGui::EndPopup;
	api.closeCurrentPopup = ImGui::CloseCurrentPopup;
	api.beginChildFrame = ImGui::BeginChildFrame;
	api.endChildFrame = ImGui::EndChildFrame;

	api.isMouseHoveringAnyWindow = ImGui::IsMouseHoveringAnyWindow;
	api.isMouseHoveringWindow = ImGui::IsMouseHoveringWindow;

	return &api;
}

static void* getComponentApi(uint32_t version)
{
	static ComponentApi_v0 api;
	memset(&api, 0x00, sizeof(api));

	switch (version) {
	case 0:
		api.createEntityManager = createEntityManager;
		api.destroyEntityManager = destroyEntityManager;
		api.createEntity = createEntity;
		api.destroyEntity = destroyEntity;
		api.isEntityAlive = isEntityAlive;
		api.registerComponentType = registerComponentType;
		api.createComponent = createComponent;
		api.findComponentTypeById = findComponentTypeById;
		api.getComponent = getComponent;
		api.getComponentData = getComponentData;
		return &api;
	default:
		return nullptr;
	}
}

void* termite::getEngineApi(uint16_t apiId, uint32_t version)
{
    ApiId id = ApiId(apiId);
    if (id == ApiId::Core && version == 0) {
        static CoreApi_v0 core0;
        core0.copyMemoryBlock = copyMemoryBlock;
        core0.createMemoryBlock = createMemoryBlock;
        core0.readTextFile = readTextFile;
        core0.refMemoryBlock = refMemoryBlock;
        core0.refMemoryBlockPtr = refMemoryBlockPtr;
        core0.releaseMemoryBlock = releaseMemoryBlock;
        core0.getElapsedTime = getElapsedTime;
        core0.reportError = reportError;
        core0.reportErrorf = reportErrorf;
        core0.logBeginProgress = bx::logBeginProgress;
        core0.logEndProgress = bx::logEndProgress;
        core0.logPrint = bx::logPrint;
        core0.logPrintf = bx::logPrintf;
        core0.getConfig = getConfig;
        core0.getEngineVersion = getEngineVersion;
        core0.getTempAlloc = getTempAlloc;

        return &core0;
    } else if (id == ApiId::Gfx && version == 0) {
        static GfxApi_v0 gfx0;
        gfx0.calcGaussKernel = calcGaussKernel;
        gfx0.drawFullscreenQuad = drawFullscreenQuad;
        gfx0.loadShaderProgram = loadShaderProgram;
        gfx0.vdeclAdd = vdeclAdd;
        gfx0.vdeclBegin = vdeclBegin;
        gfx0.vdeclEnd = vdeclEnd;
        gfx0.vdeclDecode = vdeclDecode;
        gfx0.vdeclGetSize = vdeclGetSize;
        gfx0.vdeclHas = vdeclHas;
        gfx0.vdeclSkip = vdeclSkip;

        return &gfx0;
	} else if (id == ApiId::ImGui && version == 0) {
		return getImGuiApi0();
	}

    return nullptr;
}

