add_library(cortex SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/vision/visual_cortex.c"
)

target_include_directories(cortex PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/auditory
    ${CMAKE_CURRENT_SOURCE_DIR}/motor
    ${CMAKE_CURRENT_SOURCE_DIR}/prefrontal
    ${CMAKE_CURRENT_SOURCE_DIR}/somatosensory
    ${CMAKE_CURRENT_SOURCE_DIR}/vision
)

target_link_libraries(cortex PRIVATE
    interfaces thalamus
)


