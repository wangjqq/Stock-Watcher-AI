#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "sdkconfig.h"

//#include "my_sd_fatfs.h"
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

//#define TAG "SPI_SDCard_Demo"

static const char *TAG = "SPI_SDCard_Demo";

// 文件系统挂载点
#define MOUNT_POINT     "/sdcard"
// DMA通道
#define SPI_DMA_CHAN    1
//在测试SD和SPI模式时，请记住，一旦在SPI模式下初始化了卡，在不接通卡电源的情况下就无法在SD模式下将其重新初始化。
#define PIN_NUM_MISO    3     //PIN_D0
#define PIN_NUM_MOSI    2     //PIN_CMD
#define PIN_NUM_CLK     1     //PIN_CLK
#define PIN_NUM_CS      8     //PIN_D3

static void get_fatfs_usage(size_t* out_total_bytes, size_t* out_free_bytes);

void SD_Fatfs_Init(void)
{
  esp_err_t ret;                // 结果定义
  sdmmc_card_t* card;             // SD / MMC卡结构
  const char mount_point[] = MOUNT_POINT;   // 挂载点/根目录
  char ReadFileBuff[64];            // 文件操作缓存

  // 文件系统挂载配置
  esp_vfs_fat_sdmmc_mount_config_t mount_config = { 
    .format_if_mount_failed = false,        // 如果挂载失败：true会重新分区和格式化/false不会重新分区和格式化
    .max_files = 5,                 // 打开文件最大数量
    .allocation_unit_size = 16 * 1024       // 硬盘分区簇的大小
  };

  ESP_LOGI(TAG,"Initializing SD Card");
  ESP_LOGI(TAG,"Using SPI Peripheralr");

  // 默认SPI接口配置
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  // SPI总线配置
  spi_bus_config_t bus_cfg = {
    .mosi_io_num = PIN_NUM_MOSI,
    .miso_io_num = PIN_NUM_MISO,
    .sclk_io_num = PIN_NUM_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4 * 1024 * sizeof(uint8_t),  // 最大传输大小
  };
  host.slot = SPI3_HOST;// 使用SPI3
  // SPI总线初始化
  ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
  if (ret != ESP_OK) {
    ESP_LOGI(TAG,"Failed To Initialize Bus");
    return;
  }
  // 这将初始化没有卡检测（CD）和写保护（WP）信号的插槽。
  // 如果主板有这些信号，请修改slot_config.gpio_cd和slot_config.gpio_wp。开发板没有接两个信号
  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = PIN_NUM_CS;
  slot_config.host_id = host.slot;
  // 挂载文件系统
  ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGI(TAG,"Failed to mount filesystem If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
    } else {
      ESP_LOGI(TAG,"Failed to initialize the card %s Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
    }
    return;
  }
  // TF卡已经初始化，打印TF卡属性
  sdmmc_card_print_info(stdout, card);

  // 打印文件系统使用情况信息
  size_t bytes_total, bytes_free;
  get_fatfs_usage(&bytes_total, &bytes_free);
  ESP_LOGI(TAG,"FAT FS Total: %d MB, Free: %d MB", bytes_total / 1024, bytes_free / 1024);

  // 使用POSIX和C标准库函数来处理文件。
  ESP_LOGI(TAG,"Opening File");
  FILE* f = fopen(MOUNT_POINT"/hello.txt", "w");// 创建一个文件
  if (f == NULL) {
    ESP_LOGI(TAG,"Failed to open file for writing");
    return;
  }
  fprintf(f, "ESP32-S3 Read-write test: Hello %s!\n", card->cid.name);
  fclose(f);
  ESP_LOGI(TAG,"File Written");

  // 重命名前检查目标文件是否存在
  struct stat st;
  if (stat(MOUNT_POINT"/foo.txt", &st) == 0) {
    unlink(MOUNT_POINT"/foo.txt");// 删除（如果存在）
  }

  // 重命名文件
  ESP_LOGI(TAG,"Renaming File");
  if (rename(MOUNT_POINT"/hello.txt", MOUNT_POINT"/foo.txt") != 0) {
    ESP_LOGI(TAG,"Rename Failed");
    return;
  }

  // 读取文件
  ESP_LOGI(TAG,"Reading file");
  f = fopen(MOUNT_POINT"/foo.txt", "r");// 读取方式打开文件
  if (f == NULL) {
    ESP_LOGI(TAG,"Failed to open file for reading");
    return;
  }
  fgets(ReadFileBuff, sizeof(ReadFileBuff), f); // 读取一行数据
  fclose(f);                    // 关闭文件 
  char* pos = strchr(ReadFileBuff, '\n');     // 在字符串中查找换行
  if (pos) {
    *pos = '\0';                // 替换为结束符
  }
  ESP_LOGI(TAG,"Read from file: '%s' ",ReadFileBuff);

  // 卸载分区并禁用SDMMC或SPI外设
  esp_vfs_fat_sdcard_unmount(mount_point, card);
  ESP_LOGI(TAG,"Card Unmounted");
  //卸载总线
  spi_bus_free(host.slot);
}
static void get_fatfs_usage(size_t* out_total_bytes, size_t* out_free_bytes)
{
  FATFS *fs;
  size_t free_clusters;
  int res = f_getfree("0:", &free_clusters, &fs);//
  assert(res == FR_OK);
  size_t total_sectors = (fs->n_fatent - 2) * fs->csize;
  size_t free_sectors = free_clusters * fs->csize;

  size_t sd_total = total_sectors/1024;
  size_t sd_total_KB = sd_total*fs->ssize;
  size_t sd_total_MB = (sd_total*fs->ssize)/1024;
  size_t sd_free = free_sectors/1024;
  size_t sd_free_KB = sd_free*fs->ssize;
  size_t sd_free_MB = (sd_free*fs->ssize)/1024;

  //printf("SD Cart sd_total_KB %d KByte\r\n ", sd_total_KB); 
  //printf("SD Cart sd_total_MB %d MByte\r\n ", sd_total_MB); 
  //printf("SD Cart sd_free_KB %d KByte\r\n ", sd_free_KB); 
  //printf("SD Cart sd_free_MB %d MByte\r\n ", sd_free_MB); 

  // 假设总大小小于4GiB，对于SPI Flash应该为true
  if (out_total_bytes != NULL) {
    *out_total_bytes = sd_total_KB;
  }
  if (out_free_bytes != NULL) {
    *out_free_bytes = sd_free_KB;
  }
}
// 主函数
void app_main()
{
  ESP_LOGI(TAG, "[APP] Start!~\r\n");
  ESP_LOGI(TAG, "[APP] IDF Version is %d.%d.%d",ESP_IDF_VERSION_MAJOR,ESP_IDF_VERSION_MINOR,ESP_IDF_VERSION_PATCH);
  ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

  vTaskDelay(1000 / portTICK_PERIOD_MS);
  SD_Fatfs_Init();
}