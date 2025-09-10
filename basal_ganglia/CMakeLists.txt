file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
file(GLOB INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/*.h)


add_library(basal-ganglia SHARED
    ${SOURCES} ${INCLUDES}
)

target_include_directories(basal-ganglia PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)


