// 本示例使用SPI外设与SD卡通信。

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static const char *TAG = "sdcard_example";

#define MOUNT_POINT "/sdcard"


void app_main(void) {
    esp_err_t ret;

    // 挂载文件系统的选项。
    // 如果format_if_mount_failed设置为true，则在挂载失败时会对SD卡进行分区和格式化。
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files = 5,
            .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    // 使用上述定义的设置初始化SD卡并挂载FAT文件系统。
    // 注意：esp_vfs_fat_sdmmc/sdspi_mount是一个一体化的便利函数。
    // 在开发生产应用程序时，请检查其源代码并实现错误恢复。
    ESP_LOGI(TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
            .mosi_io_num = CONFIG_SD_MOSI,
            .miso_io_num = CONFIG_SD_MISO,
            .sclk_io_num = CONFIG_SD_CLK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    // 这将初始化没有卡检测（CD）和写保护（WP）信号的插槽。
    // 如果您的开发板有这些信号，请修改slot_config.gpio_cd和slot_config.gpio_wp。
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = CONFIG_SD_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. 。"
                          //如果要格式化卡，请设置EXAMPLE_FORMAT_IF_MOUNT_FAILED菜单配置选项。
                          "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          //请确保SD卡线上有上拉电阻。
                          "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // 初始化卡后，打印其属性
    sdmmc_card_print_info(stdout, card);

    // 使用POSIX和C标准库函数来操作文件。
    // 首先创建一个文件
    const char *file_hello = MOUNT_POINT"/hello.txt";

    ESP_LOGI(TAG, "Opening file %s", file_hello);
    FILE *f = fopen(file_hello, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, "Hello %s!\n", card->cid.name);
    fclose(f);
    ESP_LOGI(TAG, "File written");

    const char *file_foo = MOUNT_POINT"/foo.txt";

    // 在重命名之前检查目标文件是否存在
    struct stat st;
    if (stat(file_foo, &st) == 0) {
        // 如果存在，就删除它
        unlink(file_foo);
    }

    // 重命名文件
    ESP_LOGI(TAG, "Renaming file %s to %s", file_hello, file_foo);
    if (rename(file_hello, file_foo) != 0) {
        ESP_LOGE(TAG, "Rename failed");
        return;
    }

    // 打开重命名后的文件只读
    ESP_LOGI(TAG, "Reading file %s", file_foo);
    f = fopen(file_foo, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }

    // 从文件中读取一行
    char line[64];
    fgets(line, sizeof(line), f);
    fclose(f);

    // 去除换行符
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    // 操作完成，卸载分区并禁用SPI外设
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");

    // 在移除所有设备后释放总线
    spi_bus_free(host.slot);
}
