idf_component_register( SRC_DIRS .
                        INCLUDE_DIRS ../include
                        PRIV_REQUIRES nvs_flash esp_http_server esp_timer wpa_supplicant esp_wifi esp-tls esp_https_ota fatfs
                        REQUIRES json driver app_update
                        WHOLE_ARCHIVE
                        )
idf_build_get_property(project_dir PROJECT_DIR)
               