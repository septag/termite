#include "pch.h"
#include "imgui_custom_controls.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "bx/string.h"
#include "bx/math.h"

namespace termite
{
    void imguiBezierEditor(ImGuiBezierEd* bezier, const char* strId, const ImVec2& size, bool lockEnds, bool showText, bool showMirrorY)
    {
        const float hsize = 5.0f;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 curvePos = ImGui::GetCursorScreenPos();
        ImVec2 curveSize = ImGui::GetContentRegionAvail();

        if (size.x > 0)
            curveSize.x = size.x;
        if (size.y > 0)
            curveSize.y = size.y;

        if (curveSize.x < 50.0f)
            curveSize.x = 50.0f;
        if (curveSize.y < 50.0f)
            curveSize.y = 50.0f;

        auto convertToScreen = [curvePos, curveSize](ImVec2 pt)->ImVec2 {
            ImVec2 rpt;
            rpt.x = curvePos.x + pt.x*curveSize.x;
            rpt.y = curvePos.y + (0.5f - pt.y*0.5f)*curveSize.y;
            return rpt;
        };

        auto convertToNorm = [curvePos, curveSize](ImVec2 pt)->ImVec2 {
            ImVec2 rpt;
            rpt.x = (pt.x - curvePos.x)/curveSize.x;
            rpt.y = 1.0f - 2.0f*(pt.y - curvePos.y)/curveSize.y;
            return rpt;
        };

        ImGui::InvisibleButton(strId, curveSize);
        ImGui::PushClipRect(curvePos, ImVec2(curvePos.x + curveSize.x, curvePos.y + curveSize.y));

        // Background
        drawList->AddRectFilled(curvePos, ImVec2(curvePos.x + curveSize.x, curvePos.y + curveSize.y), ImColor(88, 88, 88));

        // Guides
        const ImColor gridColor = ImColor(128, 128, 128);
        ImVec2 basePt = ImVec2(curvePos.x, curvePos.y + curveSize.y*0.5f);
        drawList->AddLine(basePt, ImVec2(basePt.x + curveSize.x, basePt.y), gridColor);
        const float ys[] = {
            0.75f, 0.5f, 0.25f, -0.25f, -0.5f, -0.75f
        };
        for (int i = 0; i < BX_COUNTOF(ys); i++) {
            ImVec2 lineStart = convertToScreen(ImVec2(0, ys[i]));
            drawList->AddLine(lineStart, convertToScreen(ImVec2(1.0f, ys[i])), gridColor);

            if (showText) {
                char guideStr[32];
                bx::snprintf(guideStr, sizeof(guideStr), "%.2f", ys[i]);
                drawList->AddText(lineStart, gridColor, guideStr);
            }
        }

        ImVec2 cps[4];
        for (int i = 0; i < 4; i++)
            cps[i] = convertToScreen(bezier->controlPts[i]);


        // Select control point
        //bool hovered, held;
        //bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, 0);
        if (ImGui::IsItemHovered()) {
            if (ImGui::IsMouseDown(0)) {
                if (bezier->selectedPt == -1) {
                    for (int i = 0; i < 4; i++) {
                        if (ImGui::IsMouseHoveringRect(ImVec2(cps[i].x - hsize, cps[i].y - hsize),
                                                        ImVec2(cps[i].x + hsize, cps[i].y + hsize), false)) {
                            bezier->selectedPt = i;
                            break;
                        }
                    }
                } else if (ImGui::IsMouseDragging(0)) {
                    ImVec2 delta = ImGui::GetMouseDragDelta(0);
                    if (!lockEnds || (bezier->selectedPt != 0 && bezier->selectedPt != 3))
                        cps[bezier->selectedPt].x += delta.x;
                    cps[bezier->selectedPt].y += delta.y;
                    bezier->controlPts[bezier->selectedPt] = convertToNorm(cps[bezier->selectedPt]);
                    bezier->controlPts[bezier->selectedPt].x = bx::fclamp(bezier->controlPts[bezier->selectedPt].x, 0, 1.0f);
                    bezier->controlPts[bezier->selectedPt].y = bx::fclamp(bezier->controlPts[bezier->selectedPt].y, -1.0f, 1.0f);
                    ImGui::ResetMouseDragDelta(0);
                }
            } else if (bezier->selectedPt != -1 && !ImGui::IsMouseDown(0)) {
                bezier->selectedPt = -1;
            }
        }

        // Draw Curves
        if (showMirrorY) {
            ImVec2 cpsMirror[4] = {
                bezier->controlPts[0],
                bezier->controlPts[1],
                bezier->controlPts[2],
                bezier->controlPts[3]
            };
            for (int i = 0; i < 4; i++) {
                cpsMirror[i].y = -cpsMirror[i].y;
                cpsMirror[i] = convertToScreen(cpsMirror[i]);
            }

            drawList->AddBezierCurve(cpsMirror[0], cpsMirror[1], cpsMirror[2], cpsMirror[3], ImColor(67, 143, 0), 2, 25);
        }
        drawList->AddBezierCurve(cps[0], cps[1], cps[2], cps[3], ImColor(255, 222, 0), 2, 25);

        // Draw control points
        for (int i = 0; i < 4; i++) {
            ImColor color;
            if (i != bezier->selectedPt)
                color = ImColor(255, 255, 255);
            else
                color = ImColor(51, 51, 204);
            drawList->AddRectFilled(ImVec2(cps[i].x - hsize, cps[i].y - hsize), ImVec2(cps[i].x + hsize, cps[i].y + hsize), 
                                    color);
        }

        ImGui::PopClipRect();
    }

    void imguiFishLayout(ImGuiFishLayout* layout, const char* strId, const ImVec2& size)
    {
        const float cellLS = 1.0f;

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Padding controls
        ImGui::SliderFloat2("Padding", &layout->padding.x, 0, cellLS*2.0f, "%.2f");

        ImVec2 ctrlPos = ImGui::GetCursorScreenPos();            // ImDrawList API uses screen coordinates!
        ImVec2 ctrlSize = ImGui::GetContentRegionAvail();        // Resize canvas to what's available
        if (size.x > 0)
            ctrlSize.x = bx::fmin(size.x, ctrlSize.x);
        if (size.y > 0)
            ctrlSize.y = bx::fmin(size.y, ctrlSize.y);
        if (ctrlSize.x < 50.0f)
            ctrlSize.x = 50.0f;
        if (ctrlSize.y < 50.0f)
            ctrlSize.y = 50.0f;

        float logicalWidth = layout->padding.x*3.0f + cellLS*4.0f;
        float logicalHeight = layout->padding.y*3.0f + cellLS*4.0f;
        float cellWidth = (cellLS/logicalWidth)*ctrlSize.x;
        float cellHeight = (cellLS/logicalHeight)*ctrlSize.y;
        ImVec2 padding = ImVec2((layout->padding.x / logicalWidth)*ctrlSize.x,
                                (layout->padding.y / logicalHeight)*ctrlSize.y);

        ImGui::InvisibleButton(strId, ctrlSize);
        ImGui::PushClipRect(ctrlPos, ImVec2(ctrlPos.x + ctrlSize.x, ctrlPos.y + ctrlSize.y));
            
        // Background
        drawList->AddRectFilled(ctrlPos, ImVec2(ctrlPos.x + ctrlSize.x, ctrlPos.y + ctrlSize.y), ImColor(88, 88, 88));

        if (ImGui::IsItemHovered()) {
            bool mouseDown[2];
            mouseDown[0] = ImGui::IsMouseDown(0);
            mouseDown[1] = ImGui::IsMouseDown(1);

            for (int i = 0; i < 16; i++) {
                float ix = float(i%4);
                float iy = float(i/4);

                float x = ctrlPos.x + ix*(padding.x + cellWidth);
                float y = ctrlPos.y + iy*(padding.y + cellHeight);

                ImVec2 ra = ImVec2(x, y);
                ImVec2 rb = ImVec2(x + cellWidth, y + cellHeight);

                if (ImGui::IsMouseHoveringRect(ra, rb)) {
                    if (!layout->mouseDown[0] && mouseDown[0]) {
                        layout->layout[i] = layout->layout[i] != ImGuiFishLayout::None ? 
                            ImGuiFishLayout::None : ImGuiFishLayout::Fish;
                    } else if (!layout->mouseDown[1] && mouseDown[1]) {
                        if (layout->layout[i] != ImGuiFishLayout::None) {
                            layout->layout[i] = ImGuiFishLayout::None;
                        } else {
                            for (int k = 0; k < 16; k++) {
                                if (layout->layout[k] == ImGuiFishLayout::EnemyFish)
                                    layout->layout[k] = ImGuiFishLayout::None;
                            }
                            layout->layout[i] = ImGuiFishLayout::EnemyFish;
                        }
                    }
                }
            }

            layout->mouseDown[0] = mouseDown[0];
            layout->mouseDown[1] = mouseDown[1];
        }

        // Cells
        for (int i = 0; i < 16; i++) {
            if (layout->layout[i] != ImGuiFishLayout::Fish)
                continue;

            float ix = float(i%4);
            float iy = float(i/4);

            float x = ctrlPos.x + ix*(padding.x + cellWidth);
            float y = ctrlPos.y + iy*(padding.y + cellHeight);

            ImVec2 ra = ImVec2(x, y);
            ImVec2 rb = ImVec2(x + cellWidth - 2.0f, y + cellHeight - 2.0f);

            drawList->AddRectFilled(ra, rb, ImColor(0, 110, 50));
        }

        for (int i = 0; i < 16; i++) {
            if (layout->layout[i] != ImGuiFishLayout::EnemyFish)
                continue;
            float ix = float(i%4);
            float iy = float(i/4);

            float x = ctrlPos.x + ix*(padding.x + cellWidth);
            float y = ctrlPos.y + iy*(padding.y + cellHeight);
            ImVec2 ra = ImVec2(x, y);
            ImVec2 rb = ImVec2(x + cellWidth - 2.0f, y + cellHeight - 2.0f);

            ra.x -= cellWidth*0.5f;        rb.x += cellWidth*0.5f;
            drawList->AddRectFilled(ra, rb, ImColor(156, 25, 0));
        }

        for (int i = 0; i < 16; i++) {
            float ix = float(i%4);
            float iy = float(i/4);

            float x = ctrlPos.x + ix*(padding.x + cellWidth);
            float y = ctrlPos.y + iy*(padding.y + cellHeight);
            ImVec2 ra = ImVec2(x + 1.0f, y + 1.0f);
            ImVec2 rb = ImVec2(x + cellWidth - 2.0f, y + cellHeight - 2.0f);

            drawList->AddRect(ra, rb, ImColor(140, 140, 140));
        }

        ImGui::PopClipRect();

    }

    bool imguiGaunt(const char* strId, ImVec2* values, int numValues, int* changeIdx, const ImVec2& size)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ImVec2 ctrlPos = ImGui::GetCursorScreenPos();            // ImDrawList API uses screen coordinates!
        ImVec2 ctrlSize = ImGui::GetContentRegionAvail();        // Resize canvas to what's available
        if (size.x > 0)
            ctrlSize.x = bx::fmin(size.x, ctrlSize.x);
        if (size.y > 0)
            ctrlSize.y = bx::fmin(size.y, ctrlSize.y);
        if (ctrlSize.x < 50.0f)
            ctrlSize.x = 50.0f;
        if (ctrlSize.y < 50.0f)
            ctrlSize.y = 50.0f;

        const float fixedItemHeight = ImGui::GetTextLineHeight();
        const float changeItemHeight = fixedItemHeight*1.5f;
        float totalHeight = bx::fmin(ctrlSize.y, changeItemHeight + fixedItemHeight*float(numValues-1));
        float totalWidth = ctrlSize.x;

        ImGui::InvisibleButton(strId, ImVec2(totalWidth, totalHeight));
        ImGui::PushClipRect(ctrlPos, ImVec2(ctrlPos.x + totalWidth, ctrlPos.y + totalHeight));

        // Background
        drawList->AddRectFilled(ctrlPos, ImVec2(ctrlPos.x + totalWidth, ctrlPos.y + totalHeight), ImColor(88, 88, 88));

        // calculate maximum
        float maxValue = 0;
        for (int i = 0; i < numValues; i++) {
            maxValue = bx::fmax(maxValue, values[i].y);
        }

        // Draw Items
        static const ImColor changeItemColors[] = {
            ImColor(0, 155, 35), ImColor(0, 128, 30)
        };
        static const ImColor fixedItemColors[] = {
            ImColor(0, 82, 152), ImColor(0, 67, 123)
        };

        float yOffset = 0;
        ImRect* rects = (ImRect*)alloca(sizeof(ImRect)*numValues);
        ImRect changeRect;
        int selIdx = *changeIdx;
        for (int i = 0; i < numValues; i++) {
            float lastOffset = yOffset;
            const ImColor* colors;
            if (i != selIdx) {
                yOffset += fixedItemHeight;
                colors = fixedItemColors;
            } else {
                yOffset += changeItemHeight;
                colors = changeItemColors;
            }

            ImVec2 a = ImVec2(totalWidth*values[i].x/maxValue + ctrlPos.x, lastOffset + ctrlPos.y);
            ImVec2 b = ImVec2(totalWidth*values[i].y/maxValue + ctrlPos.x, yOffset + ctrlPos.y);
            if (i == selIdx)
                changeRect = ImRect(a, b);
            rects[i] = ImRect(a, b);
            drawList->AddRectFilledMultiColor(a, b, colors[1], colors[1], colors[0], colors[0]);
            drawList->AddRect(a, b, ImColor(128, 128, 128));

            char text[32];
            bx::snprintf(text, sizeof(text), "%d", i + 1);
            ImVec2 tpos = ImVec2(a.x + (b.x - a.x)*0.5f, a.y + 1.0f);
            drawList->AddText(tpos, ImColor(255, 255, 255), text);
        }

        // Interaction
        bool selectedNew = false;
        if (ImGui::IsItemHovered()) {
            if (ImGui::IsMouseDown(0) && ImGui::IsMouseHoveringRect(changeRect.Min, changeRect.Max)) {
                if (ImGui::IsMouseDragging(0)) {
                    ImVec2 delta = ImGui::GetMouseDragDelta(0);
                    if (changeRect.Min.x + delta.x >= ctrlPos.x) {
                        changeRect.Min.x += delta.x;
                        changeRect.Max.x += delta.x;

                        values[selIdx].x = (changeRect.Min.x - ctrlPos.x)*maxValue/totalWidth;
                    }
                    ImGui::ResetMouseDragDelta(0);
                }
            } else if (ImGui::IsMouseClicked(0)&& 
                       ImGui::IsMouseHoveringRect(ctrlPos, ImVec2(ctrlPos.x + totalWidth, ctrlPos.y + totalHeight)))
            {
                for (int i = 0; i < numValues && !selectedNew; i++) {
                    if (ImGui::IsMouseHoveringRect(rects[i].Min, rects[i].Max) && i != selIdx) {
                        *changeIdx = i;
                        selectedNew = true;
                    }
                }
            }
        }

        ImGui::PopClipRect();
        return selectedNew;
    }

}
