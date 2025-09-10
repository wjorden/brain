add_library(cerebellum SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/cerebellum.c"
)

target_include_directories(cerebellum PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)


