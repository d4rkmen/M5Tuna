file(GLOB_RECURSE APP_SRCS
    ./app/*.c
    ./app/*.cpp
)

file(GLOB_RECURSE HAL_SRCS
    ./hal/*.c
    ./hal/*.cpp
)

idf_component_register(SRCS "main.cpp" "pitch_detector_task.cpp" ${APP_SRCS} ${HAL_SRCS}
                    INCLUDE_DIRS "." "./hal"
                    REQUIRES LovyanGFX esp_adc driver
                    WHOLE_ARCHIVE)
                    
target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_SOURCE_DIR}/extra_components/q/q_lib/include ${CMAKE_SOURCE_DIR}/extra_components/q/infra/include)
