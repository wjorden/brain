cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(WITH_VIZ ON)

project(brain LANGUAGES C CXX)
# all 3rd party libs with cmake files
add_subdirectory(vendored/SDL)
add_subdirectory(vendored/SDL_mixer)
add_subdirectory(vendored/SDL_ttf)
# custom libs
add_subdirectory(amygdala)
add_subdirectory(basal_ganglia)
add_subdirectory(brainstem)
add_subdirectory(cerebellum)
add_subdirectory(cortex)
add_subdirectory(hippocampus)
add_subdirectory(hypothalamus)
add_subdirectory(interfaces)
add_subdirectory(output)
add_subdirectory(periphery)
add_subdirectory(spinal_cord)
add_subdirectory(thalamus)

# demos & tests
add_subdirectory(tests) # still needs cmake file

add_executable(brain main.c)

target_include_directories(brain PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"   # proj dir
    "${CMAKE_CURRENT_SOURCE_DIR}/vendored/SDL/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendored/SDL_mixer/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendored/SDL_ttf/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendored/imgui"
)

target_link_libraries(brain PRIVATE
    SDL3::SDL3 SDL3_mixer::SDL3_mixer SDL3_ttf::SDL3_ttf
    amygdala basal-ganglia brainstem
    cerebellum cortex hippocampus interfaces
    out periphery spinal-cord thalamus)

target_compile_options(brain PRIVATE -Wall -Wextra -Wpedantic)

include(GNUInstallDirs)
install(TARGETS brain
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
