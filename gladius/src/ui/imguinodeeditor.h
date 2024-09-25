#pragma once
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#pragma clang diagnostic ignored "-Wunused-variable"
#endif
 
#include "imgui.h"
#include "imgui_internal.h"
#ifdef IMGUI_DISABLE_OBSOLETE_KEYIO
    static_assert(false, "imgui node editor requires legacy key io functions, but ");
#endif

#include <imgui-node-editor/imgui_node_editor.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif
