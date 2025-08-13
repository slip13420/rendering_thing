set(SOURCES
    main/main.cpp
    ui/ui_manager.cpp
    ui/ui_input.cpp
    core/scene_manager.cpp
    core/primitives.cpp
    core/render_engine.cpp
    core/path_tracer.cpp
    core/image_output.cpp
)

set(HEADERS
    ui/ui_manager.h
    ui/ui_input.h
    core/scene_manager.h
    core/primitives.h
    core/render_engine.h
    core/path_tracer.h
    core/image_output.h
    core/common.h
)

add_executable(path_tracer_renderer ${SOURCES} ${HEADERS})

# Compiler-specific warning and optimization flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(path_tracer_renderer PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
        $<$<CONFIG:RelWithDebInfo>:-O2 -g>
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(path_tracer_renderer PRIVATE
        /W4
        $<$<CONFIG:Debug>:/Od /Zi>
        $<$<CONFIG:Release>:/O2 /DNDEBUG>
        $<$<CONFIG:RelWithDebInfo>:/O2 /Zi>
    )
endif()

target_include_directories(path_tracer_renderer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/main
)

target_link_libraries(path_tracer_renderer PRIVATE
    Threads::Threads
)

if(WINDOWING_LIBS)
    target_link_libraries(path_tracer_renderer PRIVATE ${WINDOWING_LIBS})
endif()

if(WIN32)
    set_target_properties(path_tracer_renderer PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()