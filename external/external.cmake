set(EXTERNAL_ROOT ${REAL_ENGINE_ROOT}/external)

set(EXTERNAL_FILES
    ${EXTERNAL_ROOT}/external.cmake
    ${EXTERNAL_ROOT}/EASTL/source/allocator_eastl.cpp
    ${EXTERNAL_ROOT}/EASTL/source/assert.cpp
    ${EXTERNAL_ROOT}/EASTL/source/atomic.cpp
    ${EXTERNAL_ROOT}/EASTL/source/fixed_pool.cpp
    ${EXTERNAL_ROOT}/EASTL/source/hashtable.cpp
    ${EXTERNAL_ROOT}/EASTL/source/intrusive_list.cpp
    ${EXTERNAL_ROOT}/EASTL/source/numeric_limits.cpp
    ${EXTERNAL_ROOT}/EASTL/source/red_black_tree.cpp
    ${EXTERNAL_ROOT}/EASTL/source/string.cpp
    ${EXTERNAL_ROOT}/EASTL/source/thread_support.cpp
    ${EXTERNAL_ROOT}/EASTL/EASTL.natvis
    ${EXTERNAL_ROOT}/enkiTS/LockLessMultiReadPipe.h
    ${EXTERNAL_ROOT}/enkiTS/TaskScheduler.cpp
    ${EXTERNAL_ROOT}/enkiTS/TaskScheduler.h
    ${EXTERNAL_ROOT}/enkiTS/TaskScheduler_c.cpp
    ${EXTERNAL_ROOT}/enkiTS/TaskScheduler_c.h
    ${EXTERNAL_ROOT}/fmt/src/format.cc
    ${EXTERNAL_ROOT}/im3d/im3d.cpp
    ${EXTERNAL_ROOT}/im3d/im3d.h
    ${EXTERNAL_ROOT}/im3d/im3d_config.h
    ${EXTERNAL_ROOT}/im3d/im3d_math.h
    ${EXTERNAL_ROOT}/ImFileDialog/ImFileDialog.cpp
    ${EXTERNAL_ROOT}/ImFileDialog/ImFileDialog.h
    ${EXTERNAL_ROOT}/imgui/imgui.cpp
    ${EXTERNAL_ROOT}/imgui/imgui.h
    ${EXTERNAL_ROOT}/imgui/imgui_demo.cpp
    ${EXTERNAL_ROOT}/imgui/imgui_draw.cpp
    ${EXTERNAL_ROOT}/imgui/imgui_internal.h
    ${EXTERNAL_ROOT}/imgui/imgui_tables.cpp
    ${EXTERNAL_ROOT}/imgui/imgui_widgets.cpp
    ${EXTERNAL_ROOT}/imgui/imstb_rectpack.h
    ${EXTERNAL_ROOT}/imgui/imstb_textedit.h
    ${EXTERNAL_ROOT}/imgui/imstb_truetype.h
    ${EXTERNAL_ROOT}/ImGuizmo/ImGuizmo.cpp
    ${EXTERNAL_ROOT}/ImGuizmo/ImGuizmo.h
    ${EXTERNAL_ROOT}/lodepng/lodepng.cpp
    ${EXTERNAL_ROOT}/lodepng/lodepng.h
    ${EXTERNAL_ROOT}/meshoptimizer/allocator.cpp
    ${EXTERNAL_ROOT}/meshoptimizer/clusterizer.cpp
    ${EXTERNAL_ROOT}/meshoptimizer/indexcodec.cpp
    ${EXTERNAL_ROOT}/meshoptimizer/indexgenerator.cpp
    ${EXTERNAL_ROOT}/meshoptimizer/meshoptimizer.h
    ${EXTERNAL_ROOT}/meshoptimizer/overdrawanalyzer.cpp
    ${EXTERNAL_ROOT}/meshoptimizer/overdrawoptimizer.cpp
    ${EXTERNAL_ROOT}/meshoptimizer/simplifier.cpp
    ${EXTERNAL_ROOT}/meshoptimizer/spatialorder.cpp
    ${EXTERNAL_ROOT}/meshoptimizer/stripifier.cpp
    ${EXTERNAL_ROOT}/meshoptimizer/vcacheanalyzer.cpp
    ${EXTERNAL_ROOT}/meshoptimizer/vcacheoptimizer.cpp
    ${EXTERNAL_ROOT}/meshoptimizer/vertexcodec.cpp
    ${EXTERNAL_ROOT}/meshoptimizer/vertexfilter.cpp
    ${EXTERNAL_ROOT}/meshoptimizer/vfetchanalyzer.cpp
    ${EXTERNAL_ROOT}/meshoptimizer/vfetchoptimizer.cpp
    ${EXTERNAL_ROOT}/rpmalloc/rpmalloc.c
    ${EXTERNAL_ROOT}/rpmalloc/rpmalloc.h
    ${EXTERNAL_ROOT}/rpmalloc/rpnew.h
    ${EXTERNAL_ROOT}/simpleini/ConvertUTF.c
    ${EXTERNAL_ROOT}/simpleini/ConvertUTF.h
    ${EXTERNAL_ROOT}/simpleini/SimpleIni.h
    ${EXTERNAL_ROOT}/tracy/public/TracyClient.cpp
     ${EXTERNAL_ROOT}/tracy/public/tracy/Tracy.hpp
    ${EXTERNAL_ROOT}/tinyxml2/tinyxml2.cpp
    ${EXTERNAL_ROOT}/tinyxml2/tinyxml2.h
    ${EXTERNAL_ROOT}/xxHash/xxhash.c
    ${EXTERNAL_ROOT}/xxHash/xxhash.h
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND EXTERNAL_FILES
        ${EXTERNAL_ROOT}/d3d12ma/D3D12MemAlloc.cpp
        ${EXTERNAL_ROOT}/d3d12ma/D3D12MemAlloc.h
        ${EXTERNAL_ROOT}/d3d12ma/D3D12MemAlloc.natvis
        ${EXTERNAL_ROOT}/imgui/imgui_impl_win32.cpp
        ${EXTERNAL_ROOT}/imgui/imgui_impl_win32.h
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND EXTERNAL_FILES
        ${EXTERNAL_ROOT}/imgui/imgui_impl_osx.mm
        ${EXTERNAL_ROOT}/imgui/imgui_impl_osx.h
    )
endif()

source_group(TREE ${EXTERNAL_ROOT} PREFIX external FILES ${EXTERNAL_FILES})