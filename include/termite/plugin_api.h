#pragma once

#ifdef termite_SHARED_LIB
#   if BX_COMPILER_MSVC
#       define T_PLUGIN_EXPORT extern "C" __declspec(dllexport) 
#   else
#       define T_PLUGIN_EXPORT extern "C" __attribute__ ((visibility("default")))
#   endif
#else
#   define T_PLUGIN_EXPORT extern "C" 
#endif

#define BX_TRACE_API(_Api, _Fmt, ...) _Api->logPrintf(__FILE__, __LINE__, bx::LogType::Text, _Fmt, ##__VA_ARGS__)
#define BX_VERBOSE_API(_Api, _Fmt, ...) _Api->logPrintf(__FILE__, __LINE__, bx::LogType::Verbose, _Fmt, ##__VA_ARGS__)
#define BX_FATAL_API(_Api, _Fmt, ...) _Api->logPrintf(__FILE__, __LINE__, bx::LogType::Fatal, _Fmt, ##__VA_ARGS__)
#define BX_WARN_API(_Api, _Fmt, ...) _Api->logPrintf(__FILE__, __LINE__, bx::LogType::Warning, _Fmt, ##__VA_ARGS__)
#define BX_BEGINP_API(_Api, _Fmt, ...) _Api->logBeginProgress(__FILE__, __LINE__, _Fmt, ##__VA_ARGS__)
#define BX_END_OK_API(_Api) _Api->logEndProgress(bx::LogProgressResult::Ok)
#define BX_END_FATAL_API(_Api) _Api->logEndProgress(bx::LogProgressResult::Fatal)
#define BX_END_NONFATAL_API(_Api) _Api->logEndProgress(bx::LogProgressResult::NonFatal)

#define T_ERROR_API(_Api, _Fmt, ...) _Api->reportErrorf(__FILE__, __LINE__, _Fmt, ##__VA_ARGS__)

namespace termite
{
    enum class ApiId : uint16_t
    {
        Core = 0,
        Plugin, 
        Gfx,
		ImGui
    };

    // Plugins should also implement this function
    // Should be named "termiteGetPluginApi" (extern "C")
    typedef void* (*GetApiFunc)(uint16_t apiId, uint32_t version);

    enum PluginType : uint16_t
    {
        Unknown = 0,
        GraphicsDriver,
        IoDriver,
        Renderer
    };

    struct PluginDesc
    {
        char name[32];
        char description[64];
        uint32_t version;
        PluginType type;
    };

    struct PluginApi_v0
    {
        void* (*init)(bx::AllocatorI* alloc, GetApiFunc getApi);
        void (*shutdown)();
        PluginDesc* (*getDesc)();
    };

    TERMITE_API void* getEngineApi(uint16_t apiId, uint32_t version);
}   // namespace termite

#ifdef T_CORE_API
#include "core.h"
#include "bxx/logger.h"

namespace termite {
    struct CoreApi_v0
    {
        MemoryBlock* (*createMemoryBlock)(uint32_t size, bx::AllocatorI* alloc);
        MemoryBlock* (*refMemoryBlockPtr)(void* data, uint32_t size);
        MemoryBlock* (*refMemoryBlock)(MemoryBlock* mem);
        MemoryBlock* (*copyMemoryBlock)(const void* data, uint32_t size, bx::AllocatorI* alloc);
        void(*releaseMemoryBlock)(MemoryBlock* mem);
        MemoryBlock* (*readTextFile)(const char* filepath);
        double(*getElapsedTime)();
        void(*reportError)(const char* source, int line, const char* desc);
        void(*reportErrorf)(const char* source, int line, const char* fmt, ...);

        void(*logPrint)(const char* sourceFile, int line, bx::LogType type, const char* text);
        void(*logPrintf)(const char* sourceFile, int line, bx::LogType type, const char* fmt, ...);
        void(*logBeginProgress)(const char* sourceFile, int line, const char* fmt, ...);
        void(*logEndProgress)(bx::LogProgressResult result);

        const Config& (*getConfig)();
        uint32_t(*getEngineVersion)();
        bx::AllocatorI* (*getTempAlloc)();
    };
}
#endif

#ifdef T_GFX_API
#include "vec_math.h"
#include "gfx_defines.h"
namespace termite {
    struct IoDriverApi;

    struct GfxApi_v0
    {
        void(*calcGaussKernel)(vec4_t* kernel, int kernelSize, float stdDevSqr, float intensity,
							   int direction /*=0 horizontal, =1 vertical*/, int width, int height);
        ProgramHandle(*loadShaderProgram)(GfxDriverApi* gfxDriver, IoDriverApi* ioDriver, const char* vsFilepath,
										  const char* fsFilepath);
        void(*drawFullscreenQuad)(uint8_t viewId, ProgramHandle prog);

        VertexDecl* (*vdeclBegin)(VertexDecl* vdecl, RendererType _type);
        void(*vdeclEnd)(VertexDecl* vdecl);
        VertexDecl* (*vdeclAdd)(VertexDecl* vdecl, VertexAttrib _attrib, uint8_t _num, VertexAttribType _type, bool _normalized, bool _asInt);
        VertexDecl* (*vdeclSkip)(VertexDecl* vdecl, uint8_t _numBytes);
        void(*vdeclDecode)(VertexDecl* vdecl, VertexAttrib _attrib, uint8_t* _num, VertexAttribType* _type, bool* _normalized, bool* _asInt);
        bool(*vdeclHas)(VertexDecl* vdecl, VertexAttrib _attrib);
        uint32_t(*vdeclGetSize)(VertexDecl* vdecl, uint32_t _num);
    };
}
#endif

#ifdef T_IMGUI_API
#include "imgui/imgui.h"
namespace termite
{
	struct ImGuiApi_v0
	{
		// Window
		bool (*begin)(const char* name, bool* p_opened/* = NULL*/, ImGuiWindowFlags flags/* = 0*/);      // see .cpp for details. return false when window is collapsed, so you can early out in your code. 'bool* p_opened' creates a widget on the upper-right to close the window (which sets your bool to false).
		bool (*beginWithSize)(const char* name, bool* p_opened, const ImVec2& size_on_first_use, float bg_alpha/* = -1.0f*/, ImGuiWindowFlags flags/* = 0*/); // this is the older/longer API. the extra parameters aren't very relevant. call SetNextWindowSize() instead if you want to set a window size. For regular windows, 'size_on_first_use' only applies to the first time EVER the window is created and probably not what you want! maybe obsolete this API eventually.
		void (*end)();
		bool (*beginChild)(const char* str_id, const ImVec2& size/* = ImVec2(0, 0)*/, bool border/* = false*/, ImGuiWindowFlags extra_flags/* = 0*/);  // begin a scrolling region. size==0.0f: use remaining window size, size<0.0f: use remaining window size minus abs(size). size>0.0f: fixed size. each axis can use a different mode, e.g. ImVec2(0,400).
		bool (*beginChildId)(ImGuiID id, const ImVec2& size/* = ImVec2(0, 0)*/, bool border/* = false*/, ImGuiWindowFlags extra_flags/* = 0*/); // "
		void (*endChild)();
		ImVec2 (*getContentRegionMax)();          // current content boundaries (typically window boundaries including scrolling, or current column boundaries), in windows coordinates
		ImVec2 (*getContentRegionAvail)();        // == GetContentRegionMax() - GetCursorPos()
		float (*getContentRegionAvailWidth)();            //
		ImVec2 (*getWindowContentRegionMin)();    // content boundaries min (roughly (0,0)-Scroll), in window coordinates
		ImVec2 (*getWindowContentRegionMax)();    // content boundaries max (roughly (0,0)+Size-Scroll) where Size can be override with SetNextWindowContentSize(), in window coordinates
		float (*getWindowContentRegionWidth)();           // 
		ImDrawList* (*getWindowDrawList)();            // get rendering command-list if you want to append your own draw primitives
		ImFont* (*getWindowFont)();
		float (*getWindowFontSize)();            // size (also height in pixels) of current font with current scale applied
		void (*setWindowFontScale)(float scale);         // per-window font scale. Adjust IO.FontGlobalScale if you want to scale all windows
		ImVec2 (*getWindowPos)();        // get current window position in screen space (useful if you want to do your own drawing via the DrawList api)
		ImVec2 (*getWindowSize)();       // get current window size
		float (*getWindowWidth)();
		float (*getWindowHeight)();
		bool (*isWindowCollapsed)();

		void (*setNextWindowPos)(const ImVec2& pos, ImGuiSetCond cond/* = 0*/);         // set next window position. call before Begin()
		void (*setNextWindowPosCenter)(ImGuiSetCond cond/* = 0*/);    // set next window position to be centered on screen. call before Begin()
		void (*setNextWindowSize)(const ImVec2& size, ImGuiSetCond cond/* = 0*/);       // set next window size. set axis to 0.0f to force an auto-fit on this axis. call before Begin()
		void (*setNextWindowContentSize)(const ImVec2& size);     // set next window content size (enforce the range of scrollbars). set axis to 0.0f to leave it automatic. call before Begin()
		void (*setNextWindowContentWidth)(float width);           // set next window content width (enforce the range of horizontal scrollbar). call before Begin() 
		void (*setNextWindowCollapsed)(bool collapsed, ImGuiSetCond cond/* = 0*/);      // set next window collapsed state. call before Begin()
		void (*setNextWindowFocus)();           // set next window to be focused / front-most. call before Begin()
		void (*setWindowPos)(const ImVec2& pos, ImGuiSetCond cond/* = 0*/);    // set current window position - call within Begin()/End(). may incur tearing
		void (*setWindowSize)(const ImVec2& size, ImGuiSetCond cond/* = 0*/);  // set current window size. set to ImVec2(0,0) to force an auto-fit. may incur tearing
		void (*setWindowCollapsed)(bool collapsed, ImGuiSetCond cond/* = 0*/); // set current window collapsed state
		void (*setWindowFocus)();      // set current window to be focused / front-most
		void (*setWindowPosName)(const char* name, const ImVec2& pos, ImGuiSetCond cond/* = 0*/);      // set named window position.
		void (*setWindowSizeName)(const char* name, const ImVec2& size, ImGuiSetCond cond/* = 0*/);    // set named window size. set axis to 0.0f to force an auto-fit on this axis.
		void (*setWindowCollapsedName)(const char* name, bool collapsed, ImGuiSetCond cond/* = 0*/);   // set named window collapsed state
		void (*setWindowFocusName)(const char* name);          // set named window to be focused / front-most. use NULL to remove focus.

		float (*getScrollX)();          // get scrolling amount [0..GetScrollMaxX()]
		float (*getScrollY)();          // get scrolling amount [0..GetScrollMaxY()]
		float (*getScrollMaxX)();       // get maximum scrolling amount ~~ ContentSize.X - WindowSize.X
		float (*getScrollMaxY)();       // get maximum scrolling amount ~~ ContentSize.Y - WindowSize.Y
		void (*setScrollX)(float scroll_x);     // set scrolling amount [0..GetScrollMaxX()]
		void (*setScrollY)(float scroll_y);     // set scrolling amount [0..GetScrollMaxY()]
		void (*setScrollHere)(float center_y_ratio/* = 0.5f*/);       // adjust scrolling amount to make current cursor position visible. center_y_ratio=0.0: top, 0.5: center, 1.0: bottom.
		void (*setScrollFromPosY)(float pos_y, float center_y_ratio/* = 0.5f*/);        // adjust scrolling amount to make given position valid. use GetCursorPos() or GetCursorStartPos()+offset to get valid positions.
		void (*setKeyboardFocusHere)(int offset/* = 0*/);    // focus keyboard on the next widget. Use positive 'offset' to access sub components of a multiple component widget
		void (*setStateStorage)(ImGuiStorage* tree);     // replace tree state storage with our own (if you want to manipulate it yourself, typically clear subsection of it)
		ImGuiStorage* (*getStateStorage)();

		// Parameters stacks (shared)
		void (*pushFont)(ImFont* font);         // use NULL as a shortcut to push default font
		void (*popFont)();
		void (*pushStyleColor)(ImGuiCol idx, const ImVec4& col);
		void (*popStyleColor)(int count/* = 1*/);
		void (*pushStyleVar)(ImGuiStyleVar idx, float val);
		void (*pushStyleVarVec2)(ImGuiStyleVar idx, const ImVec2& val);
		void (*popStyleVar)(int count/* = 1*/);
		ImU32 (*getColorU32)(ImGuiCol idx, float alpha_mul/* = 1.0f*/);         // retrieve given style color with style alpha applied and optional extra alpha multiplier
		ImU32 (*getColorU32Vec4)(const ImVec4& col);          // retrieve given color with style alpha applied

																									// Parameters stacks (current window)
		void (*pushItemWidth)(float item_width);         // width of items for the common item+label case, pixels. 0.0f = default to ~2/3 of windows width, >0.0f: width in pixels, <0.0f align xx pixels to the right of window (so -1.0f always align width to the right side)
		void (*popItemWidth)();
		float (*calcItemWidth)();       // width of item given pushed settings and current cursor position
		void (*pushTextWrapPos)(float wrap_pos_x/* = 0.0f*/);         // word-wrapping for Text*() commands. < 0.0f: no wrapping; 0.0f: wrap to end of window (or column); > 0.0f: wrap at 'wrap_pos_x' position in window local space
		void (*popTextWrapPos)();
		void (*pushAllowKeyboardFocus)(bool v);          // allow focusing using TAB/Shift-TAB, enabled by default but you can disable it for certain widgets
		void (*popAllowKeyboardFocus)();
		void (*pushButtonRepeat)(bool repeat);           // in 'repeat' mode, Button*() functions return repeated true in a typematic manner (uses io.KeyRepeatDelay/io.KeyRepeatRate for now). Note that you can call IsItemActive() after any Button() to tell if the button is held in the current frame.
		void (*popButtonRepeat)();

		// Cursor / Layout
		void (*beginGroup)();          // lock horizontal starting position. once closing a group it is seen as a single item (so you can use IsItemHovered() on a group, SameLine() between groups, etc.
		void (*endGroup)();
		void (*separator)();           // horizontal line
		void (*sameLine)(float local_pos_x/* = 0.0f*/, float spacing_w/* = -1.0f*/);        // call between widgets or groups to layout them horizontally
		void (*spacing)();             // add spacing
		void (*dummy)(const ImVec2& size);      // add a dummy item of given size
		void (*indent)();              // move content position toward the right by style.IndentSpacing pixels
		void (*unindent)();            // move content position back to the left (cancel Indent)
		void (*columns)(int count/* = 1*/, const char* id/* = NULL*/, bool border/* = true*/);  // setup number of columns. use an identifier to distinguish multiple column sets. close with Columns(1).
		void (*nextColumn)();          // next column
		int (*getColumnIndex)();      // get current column index
		float (*getColumnOffset)(int column_index/* = -1*/);           // get position of column line (in pixels, from the left side of the contents region). pass -1 to use current column, otherwise 0..GetcolumnsCount() inclusive. column 0 is usually 0.0f and not resizable unless you call this
		void (*setColumnOffset)(int column_index, float offset_x);         // set position of column line (in pixels, from the left side of the contents region). pass -1 to use current column
		float (*getColumnWidth)(int column_index/* = -1*/);   // column width (== GetColumnOffset(GetColumnIndex()+1) - GetColumnOffset(GetColumnOffset())
		int (*getColumnsCount)();     // number of columns (what was passed to Columns())
		ImVec2 (*getCursorPos)();        // cursor position is relative to window position
		float (*getCursorPosX)();       // "
		float (*getCursorPosY)();       // "
		void (*setCursorPos)(const ImVec2& local_pos);   // "
		void (*setCursorPosX)(float x);         // "
		void (*setCursorPosY)(float y);         // "
		ImVec2 (*getCursorStartPos)();            // initial cursor position
		ImVec2 (*getCursorScreenPos)();           // cursor position in absolute screen coordinates [0..io.DisplaySize]
		void (*setCursorScreenPos)(const ImVec2& pos);   // cursor position in absolute screen coordinates [0..io.DisplaySize]
		void (*alignFirstTextHeightToWidgets)();         // call once if the first item on the line is a Text() item and you want to vertically lower it to match subsequent (bigger) widgets
		float (*getTextLineHeight)();            // height of font == GetWindowFontSize()
		float (*getTextLineHeightWithSpacing)();          // distance (in pixels) between 2 consecutive lines of text == GetWindowFontSize() + GetStyle().ItemSpacing.y
		float (*getItemsLineHeightWithSpacing)();         // distance (in pixels) between 2 consecutive lines of standard height widgets == GetWindowFontSize() + GetStyle().FramePadding.y*2 + GetStyle().ItemSpacing.y

		void (*pushID)(const char* str_id);     // push identifier into the ID stack. IDs are hash of the *entire* stack!
		void (*pushIDStr)(const char* str_id_begin, const char* str_id_end);
		void (*pushIDPtr)(const void* ptr_id);
		void (*pushIDInt)(int int_id);
		void (*popID)();
		ImGuiID (*getIDStr)(const char* str_id);      // calculate unique ID (hash of whole ID stack + given parameter). useful if you want to query into ImGuiStorage yourself. otherwise rarely needed
		ImGuiID (*getIDSubStr)(const char* str_id_begin, const char* str_id_end);
		ImGuiID (*getIDPtr)(const void* ptr_id);

		// Widgets
		void (*text)(const char* fmt, ...);
		void (*textV)(const char* fmt, va_list args);
		void (*textColored)(const ImVec4& col, const char* fmt, ...);
		void (*textColoredV)(const ImVec4& col, const char* fmt, va_list args);
		void (*textDisabled)(const char* fmt, ...);
		void (*textDisabledV)(const char* fmt, va_list args);
		void (*textWrapped)(const char* fmt, ...);
		void (*textWrappedV)(const char* fmt, va_list args);
		void (*textUnformatted)(const char* text, const char* text_end /*= NULL*/);         // doesn't require null terminated string if 'text_end' is specified. no copy done to any bounded stack buffer, recommended for long chunks of text
		void (*labelText)(const char* label, const char* fmt, ...);
		void (*labelTextV)(const char* label, const char* fmt, va_list args);
		void (*bullet)();
		void (*bulletText)(const char* fmt, ...);
		void (*bulletTextV)(const char* fmt, va_list args);
		bool (*button)(const char* label, const ImVec2& size/* = ImVec2(0, 0)*/);
		bool (*smallButton)(const char* label);
		bool (*invisibleButton)(const char* str_id, const ImVec2& size);
		void (*image)(ImTextureID user_texture_id, const ImVec2& size, const ImVec2& uv0/* = ImVec2(0, 0)*/, const ImVec2& uv1/* = ImVec2(1, 1)*/, const ImVec4& tint_col/* = ImVec4(1, 1, 1, 1)*/, const ImVec4& border_col/* = ImVec4(0, 0, 0, 0)*/);
		bool (*imageButton)(ImTextureID user_texture_id, const ImVec2& size, const ImVec2& uv0/* = ImVec2(0, 0)*/, const ImVec2& uv1/* = ImVec2(1, 1)*/, int frame_padding/* = -1*/, const ImVec4& bg_col/* = ImVec4(0, 0, 0, 0)*/, const ImVec4& tint_col/* = ImVec4(1, 1, 1, 1)*/);    // <0 frame_padding uses default frame padding settings. 0 for no padding
		bool (*collapsingHeader)(const char* label, const char* str_id/* = NULL*/, bool display_frame/* = true*/, bool default_open/* = false*/);
		bool (*checkbox)(const char* label, bool* v);
		bool (*checkboxFlags)(const char* label, unsigned int* flags, unsigned int flags_value);
		bool (*radioButton)(const char* label, bool active);
		bool (*radioButtonInt)(const char* label, int* v, int v_button);
		bool (*combo)(const char* label, int* current_item, const char** items, int items_count, int height_in_items/* = -1*/);
		bool (*comboZeroSep)(const char* label, int* current_item, const char* items_separated_by_zeros, int height_in_items/* = -1*/);      // separate items with \0, end item-list with \0\0
		bool (*comboGetter)(const char* label, int* current_item, bool(*items_getter)(void* data, int idx, const char** out_text), void* data, int items_count, int height_in_items/* = -1*/);
		bool (*colorButton)(const ImVec4& col, bool small_height/* = false*/, bool outline_border/* = true*/);
		bool (*colorEdit3)(const char* label, float col[3]);
		bool (*colorEdit4)(const char* label, float col[4], bool show_alpha/* = true*/);
		void (*colorEditMode)(ImGuiColorEditMode mode);      // FIXME-OBSOLETE: This is inconsistent with most of the API and should be obsoleted.
		void (*plotLines)(const char* label, const float* values, int values_count, int values_offset/* = 0*/, const char* overlay_text/* = NULL*/, float scale_min/* = FLT_MAX*/, float scale_max/* = FLT_MAX*/, ImVec2 graph_size/* = ImVec2(0, 0)*/, int stride/* = sizeof(float)*/);
		void (*plotLinesGetter)(const char* label, float(*values_getter)(void* data, int idx), void* data, int values_count, int values_offset/* = 0*/, const char* overlay_text/* = NULL*/, float scale_min/* = FLT_MAX*/, float scale_max/* = FLT_MAX*/, ImVec2 graph_size/* = ImVec2(0, 0)*/);
		void (*plotHistogram)(const char* label, const float* values, int values_count, int values_offset/* = 0*/, const char* overlay_text/* = NULL*/, float scale_min/* = FLT_MAX*/, float scale_max/* = FLT_MAX*/, ImVec2 graph_size/* = ImVec2(0, 0)*/, int stride/* = sizeof(float)*/);
		void (*plotHistogramGetter)(const char* label, float(*values_getter)(void* data, int idx), void* data, int values_count, int values_offset/* = 0*/, const char* overlay_text/* = NULL*/, float scale_min/* = FLT_MAX*/, float scale_max/* = FLT_MAX*/, ImVec2 graph_size/* = ImVec2(0, 0)*/);
		void (*progressBar)(float fraction, const ImVec2& size_arg/* = ImVec2(-1, 0)*/, const char* overlay/* = NULL*/);

		// Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can go off-bounds)
		bool (*dragFloat)(const char* label, float* v, float v_speed/* = 1.0f*/, float v_min/* = 0.0f*/, float v_max/* = 0.0f*/, const char* display_format/* = "%.3f"*/, float power/* = 1.0f*/);     // If v_min >= v_max we have no bound
		bool (*dragFloat2)(const char* label, float v[2], float v_speed/* = 1.0f*/, float v_min/* = 0.0f*/, float v_max/* = 0.0f*/, const char* display_format/* = "%.3f"*/, float power/* = 1.0f*/);
		bool (*dragFloat3)(const char* label, float v[3], float v_speed/* = 1.0f*/, float v_min/* = 0.0f*/, float v_max/* = 0.0f*/, const char* display_format/* = "%.3f"*/, float power/* = 1.0f*/);
		bool (*dragFloat4)(const char* label, float v[4], float v_speed/* = 1.0f*/, float v_min/* = 0.0f*/, float v_max/* = 0.0f*/, const char* display_format/* = "%.3f"*/, float power/* = 1.0f*/);
		bool (*dragFloatRange2)(const char* label, float* v_current_min, float* v_current_max, float v_speed/* = 1.0f*/, float v_min/* = 0.0f*/, float v_max/* = 0.0f*/, const char* display_format/* = "%.3f"*/, const char* display_format_max/* = NULL*/, float power/* = 1.0f*/);
		bool (*dragInt)(const char* label, int* v, float v_speed/* = 1.0f*/, int v_min/* = 0*/, int v_max/* = 0*/, const char* display_format/* = "%.0f"*/);            // If v_min >= v_max we have no bound
		bool (*dragInt2)(const char* label, int v[2], float v_spee/*d = 1.0f*/, int v_min/* = 0*/, int v_max/* = 0*/, const char* display_format/* = "%.0f"*/);
		bool (*dragInt3)(const char* label, int v[3], float v_speed/* = 1.0f*/, int v_min/* = 0*/, int v_max/* = 0*/, const char* display_format/* = "%.0f"*/);
		bool (*dragInt4)(const char* label, int v[4], float v_speed/* = 1.0f*/, int v_min/* = 0*/, int v_max/* = 0*/, const char* display_format/* = "%.0f"*/);
		bool (*dragIntRange2)(const char* label, int* v_current_min, int* v_current_max, float v_speed/* = 1.0f*/, int v_min/* = 0*/, int v_max/* = 0*/, const char* display_format/* = "%.0f"*/, const char* display_format_max/* = NULL*/);

		// Widgets: Input with Keyboard
		bool (*inputText)(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags/* = 0*/, ImGuiTextEditCallback callback/* = NULL*/, void* user_data/* = NULL*/);
		bool (*inputTextMultiline)(const char* label, char* buf, size_t buf_size, const ImVec2& size/* = ImVec2(0, 0)*/, ImGuiInputTextFlags flags/* = 0*/, ImGuiTextEditCallback callback/* = NULL*/, void* user_data/* = NULL*/);
		bool (*inputFloat)(const char* label, float* v, float step/* = 0.0f*/, float step_fast/* = 0.0f*/, int decimal_precision/* = -1*/, ImGuiInputTextFlags extra_flags/* = 0*/);
		bool (*inputFloat2)(const char* label, float v[2], int decimal_precision/* = -1*/, ImGuiInputTextFlags extra_flags/* = 0*/);
		bool (*inputFloat3)(const char* label, float v[3], int decimal_precision/* = -1*/, ImGuiInputTextFlags extra_flags/* = 0*/);
		bool (*inputFloat4)(const char* label, float v[4], int decimal_precision/* = -1*/, ImGuiInputTextFlags extra_flags/* = 0*/);
		bool (*inputInt)(const char* label, int* v, int step/* = 1*/, int step_fast/* = 100*/, ImGuiInputTextFlags extra_flags/* = 0*/);
		bool (*inputInt2)(const char* label, int v[2], ImGuiInputTextFlags extra_flags/* = 0*/);
		bool (*inputInt3)(const char* label, int v[3], ImGuiInputTextFlags extra_flags/* = 0*/);
		bool (*inputInt4)(const char* label, int v[4], ImGuiInputTextFlags extra_flags/* = 0*/);

		// Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can go off-bounds)
		bool (*sliderFloat)(const char* label, float* v, float v_min, float v_max, const char* display_format/* = "%.3f"*/, float power/* = 1.0f*/);     // adjust display_format to decorate the value with a prefix or a suffix. Use power!=1.0 for logarithmic sliders
		bool (*sliderFloat2)(const char* label, float v[2], float v_min, float v_max, const char* display_format/* = "%.3f"*/, float power/* = 1.0f*/);
		bool (*sliderFloat3)(const char* label, float v[3], float v_min, float v_max, const char* display_format/* = "%.3f"*/, float power/* = 1.0f*/);
		bool (*sliderFloat4)(const char* label, float v[4], float v_min, float v_max, const char* display_format/* = "%.3f"*/, float power/* = 1.0f*/);
		bool (*sliderAngle)(const char* label, float* v_rad, float v_degrees_min/* = -360.0f*/, float v_degrees_max/* = +360.0f*/);
		bool (*sliderInt)(const char* label, int* v, int v_min, int v_max, const char* display_format/* = "%.0f"*/);
		bool (*sliderInt2)(const char* label, int v[2], int v_min, int v_max, const char* display_format/* = "%.0f"*/);
		bool (*sliderInt3)(const char* label, int v[3], int v_min, int v_max, const char* display_format/* = "%.0f"*/);
		bool (*sliderInt4)(const char* label, int v[4], int v_min, int v_max, const char* display_format/* = "%.0f"*/);
		bool (*vSliderFloat)(const char* label, const ImVec2& size, float* v, float v_min, float v_max, const char* display_format/* = "%.3f"*/, float power/* = 1.0f*/);
		bool (*vSliderInt)(const char* label, const ImVec2& size, int* v, int v_min, int v_max, const char* display_format/* = "%.0f"*/);

		// Widgets: Trees
		bool (*treeNode)(const char* str_label_id);          // if returning 'true' the node is open and the user is responsible for calling TreePop()
		bool (*treeNodeFmt)(const char* str_id, const char* fmt, ...);
		bool (*treeNodePtrFmt)(const void* ptr_id, const char* fmt, ...);
		bool (*treeNodeV)(const char* str_id, const char* fmt, va_list args);  // "
		bool (*treeNodeVPtr)(const void* ptr_id, const char* fmt, va_list args);  // "
		void (*treePush)(const char* str_id/* = NULL*/);         // already called by TreeNode(), but you can call Push/Pop yourself for layouting purpose
		void (*treePushPtr)(const void* ptr_id/* = NULL*/);         // "
		void (*treePop)();
		void (*setNextTreeNodeOpened)(bool opened, ImGuiSetCond cond/* = 0*/);     // set next tree node to be opened.
																										// Widgets: Selectable / Lists
		bool (*selectable)(const char* label, bool selected/* = false*/, ImGuiSelectableFlags flags/* = 0*/, const ImVec2& size/* = ImVec2(0, 0)*/);  // size.x==0.0: use remaining width, size.x>0.0: specify width. size.y==0.0: use label height, size.y>0.0: specify height
		bool (*selectableSel)(const char* label, bool* p_selected, ImGuiSelectableFlags flags/* = 0*/, const ImVec2& size/* = ImVec2(0, 0)*/);
		bool (*listBox)(const char* label, int* current_item, const char** items, int items_count, int height_in_items/* = -1*/);
		bool (*listBoxGetter)(const char* label, int* current_item, bool(*items_getter)(void* data, int idx, const char** out_text), void* data, int items_count, int height_in_items/* = -1*/);
		bool (*listBoxHeader)(const char* label, const ImVec2& size/* = ImVec2(0, 0)*/); // use if you want to reimplement ListBox() will custom data or interactions. make sure to call ListBoxFooter() afterwards.
		bool (*listBoxHeader2)(const char* label, int items_count, int height_in_items/* = -1*/); // "
		void (*listBoxFooter)();       // terminate the scrolling region
																									// Widgets: Value() Helpers. Output single value in "name: value" format (tip: freely declare more in your code to handle your types. you can add functions to the ImGui namespace)
		void (*valueBool)(const char* prefix, bool b);
		void (*valueInt)(const char* prefix, int v);
		void (*valueUint)(const char* prefix, unsigned int v);
		void (*valueFloat)(const char* prefix, float v, const char* float_format/* = NULL*/);
		void (*valueColor)(const char* prefix, const ImVec4& v);
		void (*valueColorUint)(const char* prefix, unsigned int v);

		// Tooltip
		void (*setTooltip)(const char* fmt, ...);
		void (*setTooltipV)(const char* fmt, va_list args);
		void (*beginTooltip)();        // use to create full-featured tooltip windows that aren't just text
		void (*endTooltip)();

		// Menus
		bool(*beginMainMenuBar)();
		void(*endMainMenuBar)();
		bool(*beginMenuBar)();
		void(*endMenuBar)();
		bool(*beginMenu)(const char* label, bool enabled/* = true*/);
		void(*endMenu)();
		bool(*menuItem)(const char* label, const char* shortcut/* = nullptr*/, bool selected/* = false*/, bool enabled/* = true*/);
		bool(*menuItemSel)(const char* label, const char* shortcut/* = nullptr*/, bool* pselected, bool enabled/* = true*/);

		// Popup
		void(*openPopup)(const char* str_id);
		bool(*beginPopup)(const char* str_id);
		bool(*beginPopupModal)(const char* name, bool* popened/* = nullptr*/, ImGuiWindowFlags extraFlags/* = 0*/);
		bool(*beginPopupContextItem)(const char* str_id, int mouse_button/* = 1*/);            
		bool(*beginPopupContextWindow)(bool also_over_items/* = true*/, const char* str_id/* = NULL*/, int mouse_button/* = 1*/);
		bool(*beginPopupContextVoid)(const char* str_id/* = NULL*/, int mouse_button/* = 1*/);        
		void(*endPopup)();
		void(*closeCurrentPopup)();

		bool (*beginChildFrame)(ImGuiID id, const ImVec2& size, ImGuiWindowFlags extra_flags/* = 0*/);	// helper to create a child window / scrolling region that looks like a normal widget frame
		void (*endChildFrame)();

		bool (*isMouseHoveringWindow)();
		bool (*isMouseHoveringAnyWindow)();
	};
}
#endif

#ifdef T_COMPONENT_API
#include "component_system.h"

namespace termite
{
	struct ComponentApi_v0
	{
		EntityManager* (*createEntityManager)(bx::AllocatorI* alloc, int bufferSize/* = 0*/);
		void (*destroyEntityManager)(EntityManager* emgr);

		Entity (*createEntity)(EntityManager* emgr);
		void (*destroyEntity)(EntityManager* emgr, Entity ent);
		bool (*isEntityAlive)(EntityManager* emgr, Entity ent);

		ComponentTypeHandle (*registerComponentType)(const char* name, uint32_t id,
													 const ComponentCallbacks* callbacks, ComponentFlag flags,
													 uint32_t dataSize, uint16_t poolSize, uint16_t growSize);
		ComponentHandle (*createComponent)(Entity ent, ComponentTypeHandle handle);
		void (*destroyComponent)(Entity ent, ComponentHandle handle);

		ComponentTypeHandle (*findComponentTypeByName)(const char* name);
		ComponentTypeHandle (*findComponentTypeById)(uint32_t id);
		ComponentHandle (*getComponent)(ComponentTypeHandle handle, Entity ent);
		void* (*getComponentData)(ComponentHandle handle);

		void (*garbageCollectComponents)(EntityManager* emgr);
	};
} // namespace termite
#endif