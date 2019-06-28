deps_config := \
	/home/valkirya/ESP32/esp-idf/components/app_trace/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/aws_iot/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/bt/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/driver/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/esp32/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/esp_adc_cal/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/esp_event/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/esp_http_client/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/esp_http_server/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/ethernet/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/fatfs/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/freemodbus/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/freertos/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/heap/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/libsodium/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/log/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/lwip/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/mbedtls/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/mdns/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/mqtt/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/nvs_flash/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/openssl/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/pthread/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/spi_flash/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/spiffs/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/tcpip_adapter/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/unity/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/vfs/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/wear_levelling/Kconfig \
	/home/valkirya/ESP32/esp-idf/components/app_update/Kconfig.projbuild \
	/home/valkirya/ESP32/esp-idf/components/bootloader/Kconfig.projbuild \
	/home/valkirya/ESP32/esp-idf/components/esptool_py/Kconfig.projbuild \
	/home/valkirya/ESP32/sntp/main/Kconfig.projbuild \
	/home/valkirya/ESP32/esp-idf/components/partition_table/Kconfig.projbuild \
	/home/valkirya/ESP32/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(IDF_TARGET)" "esp32"
include/config/auto.conf: FORCE
endif
ifneq "$(IDF_CMAKE)" "n"
include/config/auto.conf: FORCE
endif

$(deps_config): ;
