%module imgui

%{

#include "imgui.h"
#include "rlImGui.h"

%}

%ignore ImGui::ShowDemoWindow;

%include "imgui.h"
%include "rlImGui.h"

%inline %{
bool ShowDemoWindow(bool show) {
    bool showValue = show;
    ImGui::ShowDemoWindow(&showValue);
    return showValue;
}
%}
