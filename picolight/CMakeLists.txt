add_executable(picolight)

# generate the header file into the source tree as it is included in the RP2040 datasheet
pico_generate_pio_header(picolight ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

target_sources(picolight PRIVATE picolight.cpp)  
target_sources(picolight PRIVATE oled.cpp)    
#target_sources(picolight PRIVATE font_gacha10.cpp)
target_sources(picolight PRIVATE bitmaps.cpp)   
target_sources(picolight PRIVATE PicoNeoPixel.cpp) 
target_sources(picolight PRIVATE Ala.cpp) 
target_sources(picolight PRIVATE AlaLedRgb.cpp) 

target_link_libraries(picolight PRIVATE pico_stdlib hardware_pio hardware_i2c)
pico_add_extra_outputs(picolight)

pico_enable_stdio_usb(picolight 1) 
pico_enable_stdio_uart(picolight 0) 


# add url via pico_set_program_url
example_auto_set_url(picolight)


# Additionally generate python and hex pioasm outputs for inclusion in the RP2040 datasheet
add_custom_target(pio_ws2812_datasheet DEPENDS ${CMAKE_CURRENT_LIST_DIR}/generated/ws2812.py)
add_custom_command(OUTPUT ${CMAKE_CURRENT_LIST_DIR}/generated/ws2812.py
        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio
        COMMAND Pioasm -o python ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio ${CMAKE_CURRENT_LIST_DIR}/generated/ws2812.py
        )
add_dependencies(picolight pio_ws2812_datasheet)

