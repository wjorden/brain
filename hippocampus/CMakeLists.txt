add_library(hippocampus SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/hippocampus.c"
)

target_include_directories(hippocampus PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)


