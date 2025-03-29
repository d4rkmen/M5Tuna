file(GLOB_RECURSE APP_SRCS
    ./app/*.c
    ./app/*.cpp
)

file(GLOB_RECURSE HAL_SRCS
    ./hal/*.c
    ./hal/*.cpp
)

file(GLOB_RECURSE SETTINGS_SRCS
    ./settings/*.c
    ./settings/*.cpp
)

idf_component_register(SRCS "main.cpp" "pitch_detector_task.cpp" ${APP_SRCS} ${HAL_SRCS} ${SETTINGS_SRCS}
                    INCLUDE_DIRS "." "./hal"
                    REQUIRES M5Unified M5GFX
                    WHOLE_ARCHIVE)
                    
cmake_policy(SET CMP0079 NEW)

target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_SOURCE_DIR}/extra_components/q/q_lib/include ${CMAKE_SOURCE_DIR}/extra_components/q/infra/include)

#nvs_create_partition_image(nvs ../nvs.csv FLASH_IN_PROJECT)