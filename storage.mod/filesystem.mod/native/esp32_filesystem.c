#include "blitzmax/embedded_runtime.h"
#include "blitzmax_esp32_board.h"

#include "driver/sdmmc_host.h"
#include "esp_littlefs.h"
#include "esp_partition.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <utime.h>

#define BMX_FS_LITTLEFS 0
#define BMX_FS_SD 1
#define BMX_LITTLEFS_PATH "/littlefs"
#define BMX_SD_PATH "/sd"

typedef struct BMXESP32File {
    FILE *file;
} BMXESP32File;

typedef struct BMXESP32Directory {
    DIR *directory;
} BMXESP32Directory;

static int32_t bmx_fs_mounted[2];
static int32_t bmx_fs_error[2];
static sdmmc_card_t *bmx_sd_card;

static int32_t bmx_fs_set_error(int32_t volume, int32_t error) {
    if (volume >= 0 && volume < 2) bmx_fs_error[volume] = error;
    return error;
}

static const char *bmx_fs_base(int32_t volume) {
    return volume == BMX_FS_LITTLEFS ? BMX_LITTLEFS_PATH :
        volume == BMX_FS_SD ? BMX_SD_PATH : NULL;
}

static char *bmx_fs_path(int32_t volume, const BMXEmbeddedString *path) {
    const char *base = bmx_fs_base(volume);
    if (!base || !bmx_fs_mounted[volume]) return NULL;
    char *relative = (char *)bmx_embedded_string_to_utf8_string(path);
    if (!relative) return NULL;
    size_t base_size = strlen(base);
    size_t relative_size = strlen(relative);
    int slash = relative_size == 0 || relative[0] != '/';
    char *result = (char *)bbMemAlloc(base_size + slash + relative_size + 1u);
    if (result) {
        memcpy(result, base, base_size);
        if (slash) result[base_size++] = '/';
        memcpy(result + base_size, relative, relative_size + 1u);
    }
    bbMemFree(relative);
    return result;
}

static int bmx_littlefs_blank(void) {
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_LITTLEFS, "storage");
    if (!partition) return 0;
    uint8_t data[256];
    for (size_t offset = 0; offset < partition->size; offset += sizeof(data)) {
        size_t size = partition->size - offset;
        if (size > sizeof(data)) size = sizeof(data);
        if (esp_partition_read(partition, offset, data, size) != ESP_OK) return 0;
        for (size_t index = 0; index < size; ++index) if (data[index] != 0xffu) return 0;
    }
    return 1;
}

int32_t bmx_esp32_littlefs_mount(int32_t format_blank) {
    if (bmx_fs_mounted[BMX_FS_LITTLEFS]) return bmx_fs_set_error(BMX_FS_LITTLEFS, 0);
    esp_vfs_littlefs_conf_t config = {
        .base_path = BMX_LITTLEFS_PATH,
        .partition_label = "storage",
        .format_if_mount_failed = format_blank && bmx_littlefs_blank(),
        .dont_mount = false
    };
    esp_err_t result = esp_vfs_littlefs_register(&config);
    if (result == ESP_OK) bmx_fs_mounted[BMX_FS_LITTLEFS] = 1;
    return bmx_fs_set_error(BMX_FS_LITTLEFS, result == ESP_OK ? 0 : -(int32_t)result);
}

int32_t bmx_esp32_littlefs_unmount(void) {
    if (!bmx_fs_mounted[BMX_FS_LITTLEFS]) return bmx_fs_set_error(BMX_FS_LITTLEFS, 0);
    esp_err_t result = esp_vfs_littlefs_unregister("storage");
    if (result == ESP_OK) bmx_fs_mounted[BMX_FS_LITTLEFS] = 0;
    return bmx_fs_set_error(BMX_FS_LITTLEFS, result == ESP_OK ? 0 : -(int32_t)result);
}

int32_t bmx_esp32_littlefs_format(void) {
    if (bmx_fs_mounted[BMX_FS_LITTLEFS]) {
        int32_t result = bmx_esp32_littlefs_unmount();
        if (result) return result;
    }
    esp_err_t result = esp_littlefs_format("storage");
    if (result == ESP_OK) return bmx_esp32_littlefs_mount(0);
    return bmx_fs_set_error(BMX_FS_LITTLEFS, -(int32_t)result);
}

int32_t bmx_esp32_littlefs_is_mounted(void) { return bmx_fs_mounted[BMX_FS_LITTLEFS]; }
int32_t bmx_esp32_littlefs_last_error(void) { return bmx_fs_error[BMX_FS_LITTLEFS]; }

int64_t bmx_esp32_littlefs_capacity(void) {
    size_t total = 0, used = 0;
    esp_err_t result = esp_littlefs_info("storage", &total, &used);
    bmx_fs_set_error(BMX_FS_LITTLEFS, result == ESP_OK ? 0 : -(int32_t)result);
    return result == ESP_OK ? (int64_t)total : -1;
}

int64_t bmx_esp32_littlefs_used(void) {
    size_t total = 0, used = 0;
    esp_err_t result = esp_littlefs_info("storage", &total, &used);
    bmx_fs_set_error(BMX_FS_LITTLEFS, result == ESP_OK ? 0 : -(int32_t)result);
    return result == ESP_OK ? (int64_t)used : -1;
}

int32_t bmx_esp32_sdcard_mount(int32_t format_if_failed) {
    if (bmx_fs_mounted[BMX_FS_SD]) return bmx_fs_set_error(BMX_FS_SD, 0);
#if defined(BMX_ESP32_BOARD_SDMMC_CLOCK) && defined(BMX_ESP32_BOARD_SDMMC_COMMAND) && defined(BMX_ESP32_BOARD_SDMMC_DATA0)
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = BMX_ESP32_BOARD_SDMMC_CONTROLLER;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1;
    slot.clk = BMX_ESP32_BOARD_SDMMC_CLOCK;
    slot.cmd = BMX_ESP32_BOARD_SDMMC_COMMAND;
    slot.d0 = BMX_ESP32_BOARD_SDMMC_DATA0;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    esp_vfs_fat_sdmmc_mount_config_t config = {
        .format_if_mount_failed = format_if_failed != 0,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024
    };
    esp_err_t result = esp_vfs_fat_sdmmc_mount(BMX_SD_PATH, &host, &slot, &config, &bmx_sd_card);
    if (result == ESP_OK) bmx_fs_mounted[BMX_FS_SD] = 1;
    return bmx_fs_set_error(BMX_FS_SD, result == ESP_OK ? 0 : -(int32_t)result);
#else
    (void)format_if_failed;
    return bmx_fs_set_error(BMX_FS_SD, -ESP_ERR_NOT_SUPPORTED);
#endif
}

int32_t bmx_esp32_sdcard_unmount(void) {
    if (!bmx_fs_mounted[BMX_FS_SD]) return bmx_fs_set_error(BMX_FS_SD, 0);
    esp_err_t result = esp_vfs_fat_sdcard_unmount(BMX_SD_PATH, bmx_sd_card);
    if (result == ESP_OK) { bmx_fs_mounted[BMX_FS_SD] = 0; bmx_sd_card = NULL; }
    return bmx_fs_set_error(BMX_FS_SD, result == ESP_OK ? 0 : -(int32_t)result);
}

int32_t bmx_esp32_sdcard_format(void) {
    if (!bmx_fs_mounted[BMX_FS_SD] || !bmx_sd_card) return bmx_fs_set_error(BMX_FS_SD, -ESP_ERR_INVALID_STATE);
    esp_vfs_fat_mount_config_t config = {.format_if_mount_failed = true, .max_files = 8, .allocation_unit_size = 16 * 1024};
    esp_err_t result = esp_vfs_fat_sdcard_format_cfg(BMX_SD_PATH, bmx_sd_card, &config);
    return bmx_fs_set_error(BMX_FS_SD, result == ESP_OK ? 0 : -(int32_t)result);
}

int32_t bmx_esp32_sdcard_is_mounted(void) { return bmx_fs_mounted[BMX_FS_SD]; }
int32_t bmx_esp32_sdcard_last_error(void) { return bmx_fs_error[BMX_FS_SD]; }
int64_t bmx_esp32_sdcard_capacity(void) { return bmx_sd_card ? (int64_t)bmx_sd_card->csd.capacity * bmx_sd_card->csd.sector_size : -1; }
int64_t bmx_esp32_sdcard_free(void) {
    if (!bmx_fs_mounted[BMX_FS_SD]) return -1;
    uint64_t total = 0, free = 0;
    esp_err_t result = esp_vfs_fat_info(BMX_SD_PATH, &total, &free);
    bmx_fs_set_error(BMX_FS_SD, result == ESP_OK ? 0 : -(int32_t)result);
    return result == ESP_OK ? (int64_t)free : -1;
}

void *bmx_esp32_filesystem_open(int32_t volume, const BMXEmbeddedString *path, int32_t readable, int32_t write_mode) {
    char *native_path = bmx_fs_path(volume, path);
    if (!native_path) { bmx_fs_set_error(volume, -ENOENT); return NULL; }
    const char *mode = readable ? (write_mode == 1 ? "w+b" : write_mode == 2 ? "a+b" : "rb") : (write_mode == 2 ? "ab" : "wb");
    BMXESP32File *handle = (BMXESP32File *)bbMemAlloc(sizeof(*handle));
    if (handle) handle->file = fopen(native_path, mode);
    bbMemFree(native_path);
    if (!handle || !handle->file) { if (handle) bbMemFree(handle); bmx_fs_set_error(volume, -errno); return NULL; }
    bmx_fs_set_error(volume, 0);
    return handle;
}

int32_t bmx_esp32_filesystem_close(void *opaque) {
    if (!opaque) return 0;
    BMXESP32File *handle = (BMXESP32File *)opaque;
    int result = fclose(handle->file);
    bbMemFree(handle);
    return result;
}
int64_t bmx_esp32_filesystem_read(void *opaque, void *buffer, int64_t count) {
    if (!opaque || !buffer || count < 0 || (uint64_t)count > SIZE_MAX) return -1;
    BMXESP32File *handle = (BMXESP32File *)opaque;
    size_t result = fread(buffer, 1, (size_t)count, handle->file);
    return result || !ferror(handle->file) ? (int64_t)result : -1;
}
int64_t bmx_esp32_filesystem_write(void *opaque, void *buffer, int64_t count) {
    if (!opaque || !buffer || count < 0 || (uint64_t)count > SIZE_MAX) return -1;
    BMXESP32File *handle = (BMXESP32File *)opaque;
    size_t result = fwrite(buffer, 1, (size_t)count, handle->file);
    return result == (size_t)count ? (int64_t)result : -1;
}
int64_t bmx_esp32_filesystem_position(void *opaque) { return opaque ? ftell(((BMXESP32File *)opaque)->file) : -1; }
int64_t bmx_esp32_filesystem_size(void *opaque) {
    if (!opaque) return -1;
    FILE *file = ((BMXESP32File *)opaque)->file;
    long position = ftell(file); if (position < 0 || fseek(file, 0, SEEK_END)) return -1;
    long size = ftell(file); (void)fseek(file, position, SEEK_SET); return size;
}
int64_t bmx_esp32_filesystem_seek(void *opaque, int64_t offset, int32_t whence) {
    if (!opaque || offset < LONG_MIN || offset > LONG_MAX || fseek(((BMXESP32File *)opaque)->file, (long)offset, whence)) return -1;
    return ftell(((BMXESP32File *)opaque)->file);
}
int32_t bmx_esp32_filesystem_resize(void *opaque, int64_t size) { return opaque && size >= 0 ? ftruncate(fileno(((BMXESP32File *)opaque)->file), size) : -1; }
int32_t bmx_esp32_filesystem_flush(void *opaque) { return opaque ? fflush(((BMXESP32File *)opaque)->file) : -1; }

int32_t bmx_esp32_filesystem_stat(int32_t volume, const BMXEmbeddedString *path, int32_t *type, int64_t *size, int64_t *modified, int64_t *created, int64_t *accessed, int32_t *read_only) {
    char *native_path = bmx_fs_path(volume, path); if (!native_path) return -1;
    struct stat info; int result = stat(native_path, &info); bbMemFree(native_path);
    if (result) return -1;
    if (type) *type = S_ISDIR(info.st_mode) ? 2 : 1;
    if (size) *size = info.st_size;
    if (modified) *modified = info.st_mtime;
    if (created) *created = info.st_ctime;
    if (accessed) *accessed = info.st_atime;
    if (read_only) *read_only = (info.st_mode & S_IWUSR) == 0;
    return 0;
}
int32_t bmx_esp32_filesystem_set_time(int32_t volume, const BMXEmbeddedString *path, int64_t time, int32_t time_type) {
    char *native_path = bmx_fs_path(volume, path); if (!native_path) return -1;
    struct stat info; struct utimbuf times; int result = stat(native_path, &info);
    if (!result) { times.actime = time_type == 2 ? time : info.st_atime; times.modtime = time_type == 0 ? time : info.st_mtime; result = utime(native_path, &times); }
    bbMemFree(native_path); return result;
}
int32_t bmx_esp32_filesystem_mkdir(int32_t volume, const BMXEmbeddedString *path) { char *p=bmx_fs_path(volume,path); if(!p)return -1; int r=mkdir(p,0777); bbMemFree(p); return r; }
int32_t bmx_esp32_filesystem_remove(int32_t volume, const BMXEmbeddedString *path) { char *p=bmx_fs_path(volume,path); if(!p)return -1; int r=remove(p); bbMemFree(p); return r; }
int32_t bmx_esp32_filesystem_rename(int32_t volume, const BMXEmbeddedString *old_path, const BMXEmbeddedString *new_path) {
    char *a=bmx_fs_path(volume,old_path), *b=bmx_fs_path(volume,new_path); if(!a||!b){if(a)bbMemFree(a);if(b)bbMemFree(b);return -1;} int r=rename(a,b);bbMemFree(a);bbMemFree(b);return r;
}
void *bmx_esp32_filesystem_directory_open(int32_t volume, const BMXEmbeddedString *path) {
    char *p=bmx_fs_path(volume,path); if(!p)return NULL; BMXESP32Directory *h=(BMXESP32Directory*)bbMemAlloc(sizeof(*h)); if(h)h->directory=opendir(p);bbMemFree(p);if(!h||!h->directory){if(h)bbMemFree(h);return NULL;}return h;
}
const BMXEmbeddedString *bmx_esp32_filesystem_directory_next(void *opaque) {
    if (!opaque) return &bmx_embedded_empty_string;
    struct dirent *entry = readdir(((BMXESP32Directory *)opaque)->directory);
    return entry ? bmx_embedded_string_from_utf8_string((const uint8_t *)entry->d_name) : &bmx_embedded_empty_string;
}
void bmx_esp32_filesystem_directory_close(void *opaque) { if(opaque){BMXESP32Directory*h=(BMXESP32Directory*)opaque;closedir(h->directory);bbMemFree(h);} }
