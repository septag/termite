#pragma once

#include "bx/allocator.h"
#include "imgui/imgui.h"

namespace termite
{
    struct ImGuiBezierEd
    {
        int selectedPt;
        ImVec2 controlPts[4];

        ImGuiBezierEd()
        {
            selectedPt = -1;
            controlPts[0] = ImVec2(0, 0);
            controlPts[1] = ImVec2(0.33f, 0);
            controlPts[2] = ImVec2(0.66f, 0);
            controlPts[3] = ImVec2(1.0f, 0);
        }

        void reset()
        {
            controlPts[0] = ImVec2(0, 0);
            controlPts[1] = ImVec2(0.33f, 0);
            controlPts[2] = ImVec2(0.66f, 0);
            controlPts[3] = ImVec2(1.0f, 0);
        }
    };

    void imguiBezierEditor(ImGuiBezierEd* bezier, const char* strId, const ImVec2& size, 
                           bool lockEnds = true, bool showText = true, bool showMirrorY = false);

    struct ImGuiFishLayout
    {
        enum LayoutState
        {
            None = 0,
            Fish,
            EnemyFish
        };

        ImVec2 padding;
        LayoutState layout[16];
        bool mouseDown[2];

        ImGuiFishLayout()
        {
            padding = ImVec2(0, 0);
            bx::memSet(layout, 0x00, sizeof(layout));
            mouseDown[0] = mouseDown[1] = false;
        }
    };
    void imguiFishLayout(ImGuiFishLayout* layout, const char* strId, const ImVec2& size);

    /// Values: x=start, y=end
    bool imguiGaunt(const char* strId, ImVec2* values, int numValues, int* changeIdx, const ImVec2& size);

    template <uint32_t _Max>
    struct ImGuiGraphData
    {
        float data[_Max];
        float presentData[_Max];
        uint32_t num;

        ImGuiGraphData() : num(0) 
        {
        }

        void add(float value)
        {
            uint32_t index = num % _Max;
            data[index] = value;
            if (num > _Max) {
                memcpy(presentData, data + index, sizeof(float)*(_Max - index));
                if (index > 0)
                    memcpy(presentData + _Max - index, data, sizeof(float)*index);
            } else {
                memcpy(presentData, data, sizeof(float)*index);
            }
            ++num;
        }

        int getCount() const
        {
            return (int)std::min<uint32_t>(num, _Max);
        }

        const float* getValues() const
        {
            return presentData;
        }
    };
}