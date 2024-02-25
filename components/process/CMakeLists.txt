idf_component_register(
        SRCS data_processor.c calibrator.c
        INCLUDE_DIRS include
        REQUIRES utils vl53l8cx sensor network
)