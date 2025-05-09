idf_component_register(SRCS "home_seismometer_main.cpp" "sensor/adxl355.cpp" 
                            "sensor/lsm6dso.cpp"
                            "seismometer/iir.cpp"
                            #"seismometer/shindo_iir_filter.cpp"
                            "seismometer/shindo_fft_processor.cpp"
                            "network/storage.cpp"
                            "network/wlan.cpp"
                            "network/network.cpp"
                            "network/improv_wifi_aux.cpp"
                            "network/improv_wifi_handler.cpp"
                            "task_improv.cpp" 
                           
                           
                            
                       INCLUDE_DIRS "."
                       REQUIRES LovyanGFX
                       REQUIRES sdk-cpp
                       REQUIRES esp_wifi
                       REQUIRES esp_http_server
                       REQUIRES lwip
                       REQUIRES fatfs)

# Create a FATFS image from the contents of the 'fatfs_image' directory
# that fits the partition named 'storage'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py -p PORT flash'.
# If read-only mode is set (CONFIG_EXAMPLE_FATFS_MODE_READ_ONLY)
# the generated image will be raw without wear levelling support.
# Otherwise it will support wear levelling and thus enable read-write mounting of the image in the device.


if(true)
    set(image ../fatfs_long_name_image)
    fatfs_create_spiflash_image(storage ${image} FLASH_IN_PROJECT PRESERVE_TIME)
endif()

