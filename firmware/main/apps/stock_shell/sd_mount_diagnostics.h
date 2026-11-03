#pragma once
#include "sd_boot_trace.hpp"
#include <esp_vfs_fat.h>
#include <driver/sdspi_host.h>
#include <sdmmc_cmd.h>
namespace stock_tools {
// Identical original mount invocation, no retries/extra card commands. Requires
// final linker --wrap=sdmmc_card_init and --wrap=f_mount; SDK stays unchanged.
esp_err_t DiagnosedSdMount(const char* path,const sdmmc_host_t* host,const sdspi_device_config_t* slot,const esp_vfs_fat_sdmmc_mount_config_t* config,sdmmc_card_t** card);
}
