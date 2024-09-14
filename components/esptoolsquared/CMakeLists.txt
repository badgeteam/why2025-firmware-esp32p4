
if(CONFIG_ET2_SUPPORT_ESP32C2)
    set(flashstubs ${flashstubs} chips/stub_esp32c2.c)
endif()
if(CONFIG_ET2_SUPPORT_ESP32C3)
    set(flashstubs ${flashstubs} chips/stub_esp32c3.c)
endif()
if(CONFIG_ET2_SUPPORT_ESP32C6)
    set(flashstubs ${flashstubs} chips/stub_esp32c6.c)
endif()
if(CONFIG_ET2_SUPPORT_ESP32P4)
    set(flashstubs ${flashstubs} chips/stub_esp32p4.c)
endif()
if(CONFIG_ET2_SUPPORT_ESP32S2)
    set(flashstubs ${flashstubs} chips/stub_esp32s2.c)
endif()
if(CONFIG_ET2_SUPPORT_ESP32S3)
    set(flashstubs ${flashstubs} chips/stub_esp32s3.c)
endif()

idf_component_register(
    SRCS
        src/esptoolsquared.c
        chips/chips.c
        ${flashstubs}
    INCLUDE_DIRS
        include
    PRIV_INCLUDE_DIRS
        chips
    REQUIRES
        driver
    PRIV_REQUIRES
        bootloader_support
)
