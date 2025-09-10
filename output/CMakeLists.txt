find_package(OpenGL REQUIRED)
add_library(out STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../output/imgui_sdl3.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../vendored/imgui/imgui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../vendored/imgui/imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../vendored/imgui/imgui_widgets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../vendored/imgui/imgui_tables.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../vendored/imgui/backends/imgui_impl_opengl3.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../vendored/imgui/backends/imgui_impl_sdl3.cpp"
)
target_include_directories(out PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../output
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendored/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendored/backends
)
target_link_libraries(out PUBLIC SDL3::SDL3 OpenGL::GL)

set_target_properties(out PROPERTIES LINKER_LANGUAGE CXX)
