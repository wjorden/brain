add_library(spinal-cord SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/spinal_reflex.c"
)

target_include_directories(spinal-cord PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)


