idf_component_register(
        SRCS sensor.c leds.c
        INCLUDE_DIRS include
        REQUIRES vl53l8cx utils led_strip
)