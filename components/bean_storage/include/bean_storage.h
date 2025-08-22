#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "esp_flash.h"
#include "esp_flash_spi_init.h"
#include "esp_partition.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "soc/spi_pins.h"


#define HOST_ID  SPI2_HOST//SPI3_HOST
#define PIN_MOSI SPI2_IOMUX_PIN_NUM_MOSI
#define PIN_MISO SPI2_IOMUX_PIN_NUM_MISO
#define PIN_CLK  SPI2_IOMUX_PIN_NUM_CLK
#define PIN_CS   SPI2_IOMUX_PIN_NUM_CS
#define PIN_WP   SPI2_IOMUX_PIN_NUM_WP
#define PIN_HD   SPI2_IOMUX_PIN_NUM_HD
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO

esp_err_t storage_init(void);
esp_err_t storage_write_file(char* filename, const char* data);
esp_err_t storage_list_files();
esp_err_t storage_read_file(char* filename);
esp_err_t storage_append_file(char* filename, const char* data);
esp_err_t storage_delete_file(char* filename);

