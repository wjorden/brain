add_library(brainstem SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/brainstem.c"
)

target_include_directories(brainstem PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(brainstem PRIVATE
    interfaces)
