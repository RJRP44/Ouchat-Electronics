idf_component_register(
        SRCS data_processor.cc calibrator.cc ai.cc
        INCLUDE_DIRS include
        REQUIRES utils vl53l8cx hardware network esp-tflite-micro
)