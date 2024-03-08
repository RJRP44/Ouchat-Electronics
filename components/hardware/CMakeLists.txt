idf_component_register(
        SRCS sensor.cc leds.cc
        INCLUDE_DIRS include
        REQUIRES vl53l8cx utils led_strip
)