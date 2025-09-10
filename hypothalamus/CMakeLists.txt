add_library(hypothalamus SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/hypothalamus.c"
)

target_include_directories(hypothalamus PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
