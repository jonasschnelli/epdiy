idf_component_register(SRCS "binary_downloader.cpp" "www-image2.cpp" "jpegdec_wrapper.cpp"
                    REQUIRES jpegdec nvs_flash epdiy esp_wifi esp_http_client mbedtls
                    INCLUDE_DIRS ".")
