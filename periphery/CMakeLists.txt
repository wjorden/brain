add_library(periphery SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/vision/camera_v4l2.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vision/camera_gray8.c"
)

target_include_directories(periphery PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(periphery PRIVATE
    thalamus interfaces)
