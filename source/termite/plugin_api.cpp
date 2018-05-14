#include "pch.h"

#define TEE_CORE_API
#define TEE_GFX_API
#define TEE_IMGUI_API
#define TEE_ECS_API
#define TEE_MATH_API
#define TEE_ASSET_API
#include "plugin_api.h"

#include "gfx_utils.h"
#include "gfx_driver.h"

#include "remotery/Remotery.h"

#include "imgui_custom_controls.h"

using namespace tee;

static void* getImGuiApi(uint32_t version)
{
	static ImGuiApi imguiApi;
	bx::memSet(&imguiApi, 0x00, sizeof(imguiApi));

	imguiApi.begin = static_cast<bool(*)(const char*, bool*, ImGuiWindowFlags)>(ImGui::Begin);
	imguiApi.beginWithSize = static_cast<bool(*)(const char*, bool*, const ImVec2&, float, ImGuiWindowFlags)>(ImGui::Begin);
	imguiApi.end = ImGui::End;
	imguiApi.beginChild = static_cast<bool(*)(const char*, const ImVec2&, bool, ImGuiWindowFlags)>(ImGui::BeginChild);
	imguiApi.beginChildId = static_cast<bool(*)(ImGuiID, const ImVec2&, bool, ImGuiWindowFlags)>(ImGui::BeginChild);
	imguiApi.endChild = ImGui::EndChild;
	imguiApi.getContentRegionMax = ImGui::GetContentRegionMax;
	imguiApi.getContentRegionAvail = ImGui::GetContentRegionAvail;
	imguiApi.getContentRegionAvailWidth = ImGui::GetContentRegionAvailWidth;
	imguiApi.getWindowContentRegionWidth = ImGui::GetWindowContentRegionWidth;
	imguiApi.getWindowContentRegionMin = ImGui::GetWindowContentRegionMin;
	imguiApi.getWindowContentRegionMax = ImGui::GetWindowContentRegionMax;
	imguiApi.getWindowDrawList = ImGui::GetWindowDrawList;
	imguiApi.getWindowFont = ImGui::GetWindowFont;
	imguiApi.getWindowFontSize = ImGui::GetWindowFontSize;
	imguiApi.setWindowFontScale = ImGui::SetWindowFontScale;
	imguiApi.getWindowPos = ImGui::GetWindowPos;
	imguiApi.getWindowFontSize = ImGui::GetWindowFontSize;
	imguiApi.getWindowWidth = ImGui::GetWindowWidth;
	imguiApi.getWindowHeight = ImGui::GetWindowHeight;
	imguiApi.isWindowCollapsed = ImGui::IsWindowCollapsed;
	imguiApi.setNextWindowPos = ImGui::SetNextWindowPos;
	imguiApi.setNextWindowPosCenter = ImGui::SetNextWindowPosCenter;
	imguiApi.setNextWindowSize = ImGui::SetNextWindowSize;
	imguiApi.setNextWindowContentSize = ImGui::SetNextWindowContentSize;
	imguiApi.setNextWindowContentWidth = ImGui::SetNextWindowContentWidth;
	imguiApi.setNextWindowFocus = ImGui::SetNextWindowFocus;
	imguiApi.setWindowCollapsed = ImGui::SetWindowCollapsed;
	imguiApi.setNextWindowFocus = ImGui::SetNextWindowFocus;
	imguiApi.setNextWindowCollapsed = ImGui::SetWindowCollapsed;
	imguiApi.setWindowPos = static_cast<void(*)(const ImVec2&, ImGuiSetCond)>(ImGui::SetWindowPos);
	imguiApi.setWindowPosName = static_cast<void(*)(const char*, const ImVec2&, ImGuiSetCond)>(ImGui::SetWindowPos);
	imguiApi.setWindowSize = static_cast<void(*)(const ImVec2&, ImGuiSetCond)>(ImGui::SetWindowSize);
	imguiApi.setWindowSizeName = static_cast<void(*)(const char*, const ImVec2&, ImGuiSetCond)>(ImGui::SetWindowSize);
	imguiApi.setWindowCollapsed = static_cast<void(*)(bool, ImGuiSetCond)>(ImGui::SetWindowCollapsed);
	imguiApi.setWindowCollapsedName = static_cast<void(*)(const char*, bool, ImGuiSetCond)>(ImGui::SetWindowCollapsed);
	imguiApi.setWindowFocus = static_cast<void(*)()>(ImGui::SetWindowFocus);
	imguiApi.setWindowFocusName = static_cast<void(*)(const char*)>(ImGui::SetWindowFocus);
	imguiApi.getScrollX = ImGui::GetScrollX;
	imguiApi.getScrollY = ImGui::GetScrollY;
	imguiApi.getScrollMaxX = ImGui::GetScrollMaxX;
	imguiApi.getScrollMaxY = ImGui::GetScrollMaxY;
	imguiApi.setScrollX = ImGui::SetScrollX;
	imguiApi.setScrollY = ImGui::SetScrollY;
	imguiApi.setScrollHere = ImGui::SetScrollHere;
	imguiApi.setScrollFromPosY = ImGui::SetScrollFromPosY;
	imguiApi.setKeyboardFocusHere = ImGui::SetKeyboardFocusHere;
	imguiApi.setStateStorage = ImGui::SetStateStorage;
	imguiApi.getStateStorage = ImGui::GetStateStorage;
	imguiApi.pushFont = ImGui::PushFont;
	imguiApi.popFont = ImGui::PopFont;
	imguiApi.pushStyleColor = ImGui::PushStyleColor;
	imguiApi.popStyleColor = ImGui::PopStyleColor;
	imguiApi.pushStyleVar = static_cast<void(*)(ImGuiStyleVar, float)>(ImGui::PushStyleVar);
	imguiApi.pushStyleVarVec2 = static_cast<void(*)(ImGuiStyleVar, const ImVec2&)>(ImGui::PushStyleVar);
	imguiApi.popStyleVar = static_cast<void(*)(int)>(ImGui::PopStyleVar);
	imguiApi.pushItemWidth = ImGui::PushItemWidth;
	imguiApi.popItemWidth = ImGui::PopItemWidth;
	imguiApi.calcItemWidth = ImGui::CalcItemWidth;
	imguiApi.pushTextWrapPos = ImGui::PushTextWrapPos;
	imguiApi.popTextWrapPos = ImGui::PopTextWrapPos;
	imguiApi.pushAllowKeyboardFocus = ImGui::PushAllowKeyboardFocus;
	imguiApi.popAllowKeyboardFocus = ImGui::PopAllowKeyboardFocus;
	imguiApi.pushButtonRepeat = ImGui::PushButtonRepeat;
	imguiApi.popButtonRepeat = ImGui::PopButtonRepeat;
	imguiApi.beginGroup = ImGui::BeginGroup;
	imguiApi.endGroup = ImGui::EndGroup;
	imguiApi.separator = ImGui::Separator;
	imguiApi.sameLine = ImGui::SameLine;
	imguiApi.spacing = ImGui::Spacing;
	imguiApi.dummy = ImGui::Dummy;
	imguiApi.indent = ImGui::Indent;
	imguiApi.unindent = ImGui::Unindent;
	imguiApi.columns = ImGui::Columns;
	imguiApi.nextColumn = ImGui::NextColumn;
	imguiApi.getColumnIndex = ImGui::GetColumnIndex;
	imguiApi.getColumnOffset = ImGui::GetColumnOffset;
	imguiApi.setColumnOffset = ImGui::SetColumnOffset;
	imguiApi.getColumnWidth = ImGui::GetColumnWidth;
	imguiApi.getColumnsCount = ImGui::GetColumnsCount;
	imguiApi.getCursorPos = ImGui::GetCursorPos;
	imguiApi.getCursorPosX = ImGui::GetCursorPosX;
	imguiApi.getCursorPosY = ImGui::GetCursorPosY;
	imguiApi.getCursorStartPos = ImGui::GetCursorStartPos;
	imguiApi.getCursorScreenPos = ImGui::GetCursorScreenPos;
	imguiApi.setCursorScreenPos = ImGui::SetCursorScreenPos;
	imguiApi.alignFirstTextHeightToWidgets = ImGui::AlignFirstTextHeightToWidgets;
	imguiApi.getTextLineHeight = ImGui::GetTextLineHeight;
	imguiApi.getTextLineHeightWithSpacing = ImGui::GetTextLineHeightWithSpacing;
	imguiApi.getItemsLineHeightWithSpacing = ImGui::GetItemsLineHeightWithSpacing;
	imguiApi.pushID = static_cast<void(*)(const char*)>(ImGui::PushID);
	imguiApi.pushIDStr = static_cast<void(*)(const char*, const char*)>(ImGui::PushID);
	imguiApi.pushIDPtr = static_cast<void(*)(const void*)>(ImGui::PushID);
	imguiApi.pushIDInt = static_cast<void(*)(int)>(ImGui::PushID);
	imguiApi.popID = ImGui::PopID;
	imguiApi.getIDStr = static_cast<ImGuiID(*)(const char*)>(ImGui::GetID);
	imguiApi.getIDPtr = static_cast<ImGuiID(*)(const void*)>(ImGui::GetID);
	imguiApi.getIDSubStr = static_cast<ImGuiID(*)(const char*, const char*)>(ImGui::GetID);
	imguiApi.text = ImGui::Text;
	imguiApi.textV = ImGui::TextV;
	imguiApi.textColored = ImGui::TextColored;
	imguiApi.textColoredV = ImGui::TextColoredV;
	imguiApi.textDisabled = ImGui::TextDisabled;
	imguiApi.textDisabledV = ImGui::TextDisabledV;
	imguiApi.textWrapped = ImGui::TextWrapped;
	imguiApi.textWrappedV = ImGui::TextWrappedV;
	imguiApi.textUnformatted = ImGui::TextUnformatted;
	imguiApi.labelText = ImGui::LabelText;
	imguiApi.labelTextV = ImGui::LabelTextV;
	imguiApi.bullet = ImGui::Bullet;
	imguiApi.bulletText = ImGui::BulletText;
	imguiApi.bulletTextV = ImGui::BulletTextV;
	imguiApi.button = ImGui::Button;
	imguiApi.smallButton = ImGui::SmallButton;
	imguiApi.invisibleButton = ImGui::InvisibleButton;
	imguiApi.image = ImGui::Image;
	imguiApi.imageButton = ImGui::ImageButton;
	imguiApi.collapsingHeader = ImGui::CollapsingHeader;
	imguiApi.checkbox = ImGui::Checkbox;
	imguiApi.checkboxFlags = ImGui::CheckboxFlags;
	imguiApi.radioButton = static_cast<bool(*)(const char*, bool)>(ImGui::RadioButton);
	imguiApi.radioButtonInt = static_cast<bool(*)(const char*, int*, int)>(ImGui::RadioButton);
	imguiApi.combo = static_cast<bool(*)(const char*, int*, const char**, int, int)>(ImGui::Combo);
	imguiApi.comboZeroSep = static_cast<bool(*)(const char*, int*, const char*, int)>(ImGui::Combo);
	imguiApi.comboGetter = static_cast<bool(*)(const char*, int*, bool(*)(void*, int, const char**), void*, int, int)>(ImGui::Combo);
	imguiApi.colorButton = ImGui::ColorButton;
	imguiApi.colorEdit3 = ImGui::ColorEdit3;
	imguiApi.colorEdit4 = ImGui::ColorEdit4;
	imguiApi.colorEditMode = ImGui::ColorEditMode;
	imguiApi.plotLines = static_cast<void(*)(const char*, const float*, int, int, const char*, float, float, ImVec2, int)>(ImGui::PlotLines);
	imguiApi.plotLinesGetter = static_cast<void(*)(const char*, float(*)(void*, int), void*, int, int, const char*, float, float, ImVec2)>(ImGui::PlotLines);
	imguiApi.plotHistogram = static_cast<void(*)(const char*, const float*, int, int, const char*, float, float, ImVec2, int)>(ImGui::PlotHistogram);
	imguiApi.plotHistogramGetter = static_cast<void(*)(const char*, float(*)(void*, int), void*, int, int, const char*, float, float, ImVec2)>(ImGui::PlotHistogram);
	imguiApi.progressBar = ImGui::ProgressBar;
	imguiApi.dragFloat = ImGui::DragFloat;
	imguiApi.dragFloat2 = ImGui::DragFloat2;
	imguiApi.dragFloat3 = ImGui::DragFloat3;
	imguiApi.dragFloat4 = ImGui::DragFloat4;
	imguiApi.dragFloatRange2 = ImGui::DragFloatRange2;
	imguiApi.dragInt = ImGui::DragInt;
	imguiApi.dragInt2 = ImGui::DragInt2;
	imguiApi.dragInt3 = ImGui::DragInt3;
	imguiApi.dragInt4 = ImGui::DragInt4;
	imguiApi.dragIntRange2 = ImGui::DragIntRange2;
	imguiApi.inputText = ImGui::InputText;
	imguiApi.inputTextMultiline = ImGui::InputTextMultiline;
	imguiApi.inputFloat = ImGui::InputFloat;
	imguiApi.inputFloat2 = ImGui::InputFloat2;
	imguiApi.inputFloat3 = ImGui::InputFloat3;
	imguiApi.inputFloat4 = ImGui::InputFloat4;
	imguiApi.inputInt = ImGui::InputInt;
	imguiApi.inputInt2 = ImGui::InputInt2;
	imguiApi.inputInt3 = ImGui::InputInt3;
	imguiApi.inputInt4 = ImGui::InputInt4;
	imguiApi.sliderFloat = ImGui::SliderFloat;
	imguiApi.sliderFloat2 = ImGui::SliderFloat2;
	imguiApi.sliderFloat3 = ImGui::SliderFloat3;
	imguiApi.sliderFloat4 = ImGui::SliderFloat4;
	imguiApi.sliderAngle = ImGui::SliderAngle;
	imguiApi.sliderInt = ImGui::SliderInt;
	imguiApi.sliderInt2 = ImGui::SliderInt2;
	imguiApi.sliderInt3 = ImGui::SliderInt3;
	imguiApi.sliderInt4 = ImGui::SliderInt4;
	imguiApi.vSliderFloat = ImGui::VSliderFloat;
	imguiApi.vSliderInt = ImGui::VSliderInt;
	imguiApi.treeNode = static_cast<bool(*)(const char*)>(ImGui::TreeNode);
	imguiApi.treeNodeFmt = static_cast<bool(*)(const char* str_id, const char* fmt, ...)>(ImGui::TreeNode);
	imguiApi.treeNodePtrFmt = static_cast<bool(*)(const void* ptr_id, const char* fmt, ...)>(ImGui::TreeNode);
	imguiApi.treeNodeV = static_cast<bool(*)(const char*, const char*, va_list)>(ImGui::TreeNodeV);
	imguiApi.treeNodeVPtr = static_cast<bool(*)(const void*, const char*, va_list)>(ImGui::TreeNodeV);
	imguiApi.treePush = static_cast<void(*)(const char*)>(ImGui::TreePush);
	imguiApi.treePushPtr = static_cast<void(*)(const void*)>(ImGui::TreePush);
	imguiApi.treePop = ImGui::TreePop;
	imguiApi.setNextTreeNodeOpened = ImGui::SetNextTreeNodeOpened;
	imguiApi.selectable = static_cast<bool(*)(const char*, bool, ImGuiSelectableFlags, const ImVec2&)>(ImGui::Selectable);
	imguiApi.selectableSel = static_cast<bool(*)(const char*, bool*, ImGuiSelectableFlags, const ImVec2&)>(ImGui::Selectable);
	imguiApi.listBox = static_cast<bool(*)(const char*, int*, const char**, int, int)>(ImGui::ListBox);
	imguiApi.listBoxGetter = static_cast<bool(*)(const char*, int*, bool(*)(void*, int, const char**), void*, int, int)>(ImGui::ListBox);
	imguiApi.listBoxHeader = static_cast<bool(*)(const char*, const ImVec2&)>(ImGui::ListBoxHeader);
	imguiApi.listBoxHeader2 = static_cast<bool(*)(const char*, int, int)>(ImGui::ListBoxHeader);
	imguiApi.listBoxFooter = static_cast<void(*)()>(ImGui::ListBoxFooter);
	imguiApi.valueBool = static_cast<void(*)(const char*, bool)>(ImGui::Value);
	imguiApi.valueInt = static_cast<void(*)(const char*, int)>(ImGui::Value);
	imguiApi.valueUint = static_cast<void(*)(const char*, unsigned int)>(ImGui::Value);
	imguiApi.valueFloat = static_cast<void(*)(const char*, float, const char*)>(ImGui::Value);
	imguiApi.valueColor = static_cast<void(*)(const char*, const ImVec4&)>(ImGui::ValueColor);
	imguiApi.valueColorUint = static_cast<void(*)(const char*, unsigned int)>(ImGui::ValueColor);

	imguiApi.setTooltip = ImGui::SetTooltip;
	imguiApi.setTooltipV = ImGui::SetTooltipV;
	imguiApi.beginTooltip = ImGui::BeginTooltip;
	imguiApi.endTooltip = ImGui::EndTooltip;

	imguiApi.beginMainMenuBar = ImGui::BeginMainMenuBar;
	imguiApi.endMainMenuBar = ImGui::EndMainMenuBar;
	imguiApi.beginMenuBar = ImGui::BeginMenuBar;
	imguiApi.endMenuBar = ImGui::EndMenuBar;
	imguiApi.beginMenu = ImGui::BeginMenu;
	imguiApi.endMenu = ImGui::EndMenu;
	imguiApi.menuItem = static_cast<bool(*)(const char*, const char*, bool, bool)>(ImGui::MenuItem);
	imguiApi.menuItemSel = static_cast<bool(*)(const char*, const char*, bool*, bool)>(ImGui::MenuItem);

	imguiApi.openPopup = ImGui::OpenPopup;
	imguiApi.beginPopup = ImGui::BeginPopup;
	imguiApi.beginPopupModal = ImGui::BeginPopupModal;
	imguiApi.beginPopupContextItem = ImGui::BeginPopupContextItem;
	imguiApi.beginPopupContextWindow = ImGui::BeginPopupContextWindow;
	imguiApi.beginPopupContextVoid = ImGui::BeginPopupContextVoid;
	imguiApi.endPopup = ImGui::EndPopup;
	imguiApi.closeCurrentPopup = ImGui::CloseCurrentPopup;
	imguiApi.beginChildFrame = ImGui::BeginChildFrame;
	imguiApi.endChildFrame = ImGui::EndChildFrame;

	imguiApi.isMouseHoveringAnyWindow = ImGui::IsMouseHoveringAnyWindow;
	imguiApi.isMouseHoveringWindow = ImGui::IsMouseHoveringWindow;
    imguiApi.isItemHovered = ImGui::IsItemHovered;
    imguiApi.isWindowFocused = ImGui::IsWindowFocused;
    imguiApi.isRootWindowOrAnyChildFocused = ImGui::IsRootWindowOrAnyChildFocused;
    imguiApi.isRootWindowFocused = ImGui::IsRootWindowFocused;
    imguiApi.isMouseClicked = ImGui::IsMouseClicked;
    imguiApi.isMouseDoubleClicked = ImGui::IsMouseDoubleClicked;
    imguiApi.isAnyItemActive = ImGui::IsAnyItemActive;
    imguiApi.isAnyItemHovered = ImGui::IsAnyItemHovered;

    imguiApi.isOverGuizmo = ImGuizmo::IsOver;
    imguiApi.isUsingGuizmo = ImGuizmo::IsUsing;
    imguiApi.enableGuizmo = ImGuizmo::Enable;
    imguiApi.decomposeMatrixToComponents = ImGuizmo::DecomposeMatrixToComponents;
    imguiApi.recomposeMatrixFromComponents = ImGuizmo::RecomposeMatrixFromComponents;
    imguiApi.manipulateGuizmo = ImGuizmo::Manipulate;
    imguiApi.drawCubeGuizmo = ImGuizmo::DrawCube;

    imguiApi.bezierEditor = imgui::bezierEditor;
    imguiApi.fishLayout = imgui::gridSelect;
    imguiApi.gaunt = imgui::gaunt;

	return &imguiApi;
}

static void* getAssetApi(uint32_t version) {
    static AssetApi assetApi;
    bx::memSet(&assetApi, 0x00, sizeof(assetApi));

    assetApi.registerType = asset::registerType;
    assetApi.load = asset::load;
    assetApi.loadMem = asset::loadMem;
    assetApi.unload = asset::unload;

    return &assetApi;
}

static void* getEcsApi(uint32_t version)
{
	static EcsApi ecsApi;
	bx::memSet(&ecsApi, 0x00, sizeof(ecsApi));

	switch (version) {
	case 0:
		ecsApi.createEntityManager = ecs::createEntityManager;
		ecsApi.destroyEntityManager = ecs::destroyEntityManager;
		ecsApi.create = ecs::create;
		ecsApi.destroy = ecs::destroy;
		ecsApi.isAlive = ecs::isAlive;
		ecsApi.registerComponent = ecs::registerComponent;
		ecsApi.createComponent = ecs::createComponent;
		ecsApi.findTypeByHash = ecs::findType;
		ecsApi.get = ecs::get;
		ecsApi.getData = ecs::getData;
        ecsApi.createGroup = ecs::createGroup;
        ecsApi.destroyGroup = ecs::destroyGroup;
        ecsApi.updateGroup = ecs::updateGroup;
		return &ecsApi;
	default:
		return nullptr;
	}
}

static void* getCameraApi(uint32_t version)
{
    static MathApi mathApi;
    bx::memSet(&mathApi, 0x00, sizeof(mathApi));

    switch (version) {
    case 0:
        mathApi.camInit = camInit;
        mathApi.camLookAt = camLookAt;
        mathApi.camCalcFrustumCorners = camCalcFrustumCorners;
        mathApi.camCalcFrustumPlanes = camCalcFrustumPlanes;
        mathApi.camPitch = camPitch;
        mathApi.camYaw = camYaw;
        mathApi.camPitchYaw = camPitchYaw;
        mathApi.camRoll = camRoll;
        mathApi.camForward = camForward;
        mathApi.camStrafe = camStrafe;
        mathApi.camViewMtx = camViewMtx;
        mathApi.camProjMtx = camProjMtx;
        mathApi.cam2dInit = cam2dInit;
        mathApi.cam2dPan = cam2dPan;
        mathApi.cam2dZoom = cam2dZoom;
        mathApi.cam2dViewMtx = cam2dViewMtx;
        mathApi.cam2dProjMtx = cam2dProjMtx;
        mathApi.cam2dGetRect = cam2dGetRect;
        return &mathApi;
    default:
        return nullptr;
    }
}

static void* getCoreApi(uint32_t version)
{
    static CoreApi coreApi;
    bx::memSet(&coreApi, 0x00, sizeof(coreApi));

    coreApi.copyMemoryBlock = copyMemoryBlock;
    coreApi.createMemoryBlock = createMemoryBlock;
    coreApi.readTextFile = readTextFile;
    coreApi.refMemoryBlock = refMemoryBlock;
    coreApi.refMemoryBlockPtr = refMemoryBlockPtr;
    coreApi.releaseMemoryBlock = releaseMemoryBlock;
    coreApi.getElapsedTime = getElapsedTime;
    coreApi.reportError = err::report;
    coreApi.reportErrorf = err::reportf;
    coreApi.logBeginProgress = debug::beginProgress;
    coreApi.logEndProgress = debug::endProgress;
    coreApi.logPrint = debug::print;
    coreApi.logPrintf = debug::printf;
    coreApi.getConfig = getConfig;
    coreApi.getEngineVersion = getEngineVersion;
    coreApi.getTempAlloc = getTempAlloc;
    coreApi.getGfxDriver = getGfxDriver;
    coreApi.getAsyncIoDriver = getAsyncIoDriver;
    coreApi.getBlockingIoDriver = getBlockingIoDriver;
    coreApi.getPhys2dDriver = getPhys2dDriver;
#if RMT_ENABLED
    coreApi.beginCPUSample = _rmt_BeginCPUSample;
    coreApi.endCPUSample = _rmt_EndCPUSample;
#endif
    coreApi.dispatchBigJobs = dispatchBigJobs;
    coreApi.dispatchSmallJobs = dispatchSmallJobs;
    coreApi.waitAndDeleteJob = waitAndDeleteJob;
    coreApi.isJobDone = isJobDone;
    coreApi.deleteJob = deleteJob;

    return &coreApi;
}

static void* getGfxApi(uint32_t version)
{
    static GfxApi gfxApi;
    gfxApi.calcGaussKernel = gfx::calcGaussKernel;
    gfxApi.drawFullscreenQuad = gfx::drawFullscreenQuad;
    gfxApi.loadShaderProgram = gfx::loadProgram;
    gfxApi.addAttrib = gfx::addAttrib;
    gfxApi.beginDecl = gfx::beginDecl;
    gfxApi.endDecl = gfx::endDecl;
    gfxApi.decodeAttrib = gfx::decodeAttrib;
    gfxApi.getDeclSize = gfx::getDeclSize;
    gfxApi.hasAttrib = gfx::hasAttrib;
    gfxApi.skipAttrib = gfx::skipAttrib;

    return &gfxApi;
}

void* tee::getEngineApi(uint16_t apiId, uint32_t version)
{
    switch (apiId) {
    case ApiId::Core:
        return getCoreApi(version);

    case ApiId::Gfx:
        return getGfxApi(version);

    case ApiId::ImGui:
        return getImGuiApi(version);

    case ApiId::Camera:
        return getCameraApi(version);

    case ApiId::Component:
        return getEcsApi(version);

    case ApiId::Asset:
        return getAssetApi(version);

    default:
        assert(0);
        return nullptr;
    }
}

