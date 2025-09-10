add_library(thalamus SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/vision/thalamus_vision.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vision/frame_pool.c"
)

target_include_directories(thalamus PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/audition
    ${CMAKE_CURRENT_SOURCE_DIR}/somatic
    ${CMAKE_CURRENT_SOURCE_DIR}/vision
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(thalamus PRIVATE
    interfaces
)
