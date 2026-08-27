#include "sd_card.h"
#include "EXIO/TCA9554PWR.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "diskio_impl.h"
#include "diskio_sdmmc.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "SD_Card";

static sdmmc_card_t *s_card = NULL;
static FATFS *s_fs = NULL;
static bool s_mounted = false;

static esp_err_t exio_do_transaction(int slot, sdmmc_command_t *cmdinfo)
{
    Set_EXIO(TCA9554_EXIO4, 0);
    esp_err_t ret = sdspi_host_do_transaction(slot, cmdinfo);
    Set_EXIO(TCA9554_EXIO4, 1);
    return ret;
}

bool sd_card_init(void)
{
    esp_err_t ret;

    /* The SPI2 host is SHARED with the LCD: LCD_Init() already owns the bus on
       pins 1 (MOSI) / 2 (SCLK) / 42 (MISO) and initializes it with DMA. Never
       free or re-initialize it here, or the LCD would break. We just attach the
       SD card as a second device on that bus. The SD card CS is not a GPIO; it
       is driven through the EXIO (TCA9554) expander in exio_do_transaction(). */
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = -1;
    slot_config.host_id = SPI2_HOST;
    sdspi_dev_handle_t sd_handle;
    ret = sdspi_host_init_device(&slot_config, &sd_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "sdspi host device init failed: %d", ret);
        return false;
    }

    sdmmc_host_t s_host = SDSPI_HOST_DEFAULT();
    s_host.slot = (int)sd_handle;
    s_host.do_transaction = exio_do_transaction;

    s_card = (sdmmc_card_t *)malloc(sizeof(sdmmc_card_t));
    if (!s_card) {
        ESP_LOGE(TAG, "malloc card failed");
        sdspi_host_remove_device(sd_handle);
        return false;
    }

    ret = sdmmc_card_init(&s_host, s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "sdmmc_card_init failed: %d", ret);
        free(s_card);
        s_card = NULL;
        sdspi_host_remove_device(sd_handle);
        return false;
    }

    esp_vfs_fat_conf_t conf = {
        .base_path = "/sdcard",
        .fat_drive = "0:",
        .max_files = 5,
    };

    ret = esp_vfs_fat_register(&conf, &s_fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "VFS fat register failed: %d", ret);
        free(s_card);
        s_card = NULL;
        sdspi_host_remove_device(sd_handle);
        return false;
    }

    ff_diskio_register_sdmmc(0, s_card);

    FRESULT fr = f_mount(s_fs, "0:", 1);
    if (fr == FR_NO_FILESYSTEM) {
        ESP_LOGW(TAG, "No FAT filesystem found on card, formatting as FAT32...");
        MKFS_PARM mkfs_opt = { .fmt = FM_FAT32, .au_size = 0 };
        fr = f_mkfs("0:", &mkfs_opt, NULL, 0);
        if (fr != FR_OK) {
            ESP_LOGE(TAG, "f_mkfs failed: %d", fr);
            esp_vfs_fat_unregister_path("/sdcard");
            free(s_card);
            s_card = NULL;
            sdspi_host_remove_device(sd_handle);
            return false;
        }
        fr = f_mount(s_fs, "0:", 1);
    }
    if (fr != FR_OK) {
        ESP_LOGE(TAG, "f_mount failed: %d", fr);
        esp_vfs_fat_unregister_path("/sdcard");
        free(s_card);
        s_card = NULL;
        sdspi_host_remove_device(sd_handle);
        return false;
    }

    s_mounted = true;
    ESP_LOGI(TAG, "SD card mounted, size: %llu MB",
             (unsigned long long)(s_card->csd.capacity * s_card->csd.sector_size) / (1024 * 1024));
    return true;
}

bool sd_card_mounted(void)
{
    return s_mounted;
}
