set(SOURCES
    main/main.cpp
    ui/ui_manager.cpp
    ui/ui_input.cpp
    core/scene_manager.cpp
    core/primitives.cpp
    core/camera.cpp
    render/render_engine.cpp
    render/path_tracer.cpp
    render/image_output.cpp
)

# Add GPU compute sources if GPU acceleration is enabled
if(USE_GPU)
    list(APPEND SOURCES render/gpu_compute.cpp render/gpu_memory.cpp render/gpu_rng.cpp)
endif()

add_executable(path_tracer_renderer ${SOURCES})

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
    ${CMAKE_CURRENT_SOURCE_DIR}/render
    ${CMAKE_CURRENT_SOURCE_DIR}/main
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(path_tracer_renderer PRIVATE
    Threads::Threads
)

if(WINDOWING_LIBS)
    target_link_libraries(path_tracer_renderer PRIVATE ${WINDOWING_LIBS})
endif()

# Link GPU libraries if GPU acceleration is enabled
if(USE_GPU AND GPU_LIBS)
    target_link_libraries(path_tracer_renderer PRIVATE ${GPU_LIBS})
    if(GPU_INCLUDE_DIRS)
        target_include_directories(path_tracer_renderer PRIVATE ${GPU_INCLUDE_DIRS})
    endif()
endif()

if(WIN32)
    set_target_properties(path_tracer_renderer PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()