file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*/*.c)
file(GLOB INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/*/*.h)

add_library(interfaces SHARED
    ${SOURCES} ${INCLUDES}
)

target_include_directories(interfaces PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/bus
    ${CMAKE_CURRENT_SOURCE_DIR}/io
)
