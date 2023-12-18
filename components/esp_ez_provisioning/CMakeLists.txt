set(component_srcs)

idf_component_register(SRCS "esp_ez_provisioning.c" "${component_srcs}"
                       PRIV_REQUIRES driver
                       INCLUDE_DIRS "."
                       REQUIRES
                            json
                            esp_http_server
                            esp_simple_wifi)