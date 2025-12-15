
#include "sd_storage.h"
#include <string.h>
#include <esp_vfs_fat.h>
#include <esp_vfs.h>
#include <esp_log.h>

#include <dirent.h>

#define CSV_PATH                PICS_PATH"/pics.csv"
#define CSV_READ_BUF_SIZE       10 * 1024
#define CSV_MAX_COLUMNS         4

static char *csv_read_buf = NULL;
static volatile int max_photos = 10;

const char *TAG = "CSV_HANDLE";

static const char* get_field(char* line, int target_field_num)
{
    const char* tok;
    for (tok = strtok(line, ","); tok && *tok; tok = strtok(NULL, ",\n"))
    {
        if (!--target_field_num)
            return tok;
    }
    return NULL;
}

static int get_entries_size(const char *csv_path)
{
    int lines = 0;
    if (!csv_read_buf) {
        ESP_LOGE(TAG, "Csv read buffer is NULL");
        return -1;
    }
    if (!csv_path) {
        ESP_LOGE(TAG, "Csv path is NULL");
        return -1;
    }

    FILE *csv_file = fopen(csv_path, "r");
    if (csv_file) {
        int read = fread(csv_read_buf, 1, CSV_READ_BUF_SIZE, csv_file);
        if (read > 0) {
            for (int i = 0; i < read; i++) {
                if (csv_read_buf[i] == '\n') {
                    lines++;
                }
            }
        }
    }
    fclose(csv_file);
    memset(csv_read_buf, 0, CSV_READ_BUF_SIZE);

    return lines;
}

int sd_storage_csv_remove_entry(const char *value)
{
    if (!csv_read_buf) {
        ESP_LOGE(TAG, "Csv read buffer is NULL");
        return -1;
    }

    FILE *file = fopen(CSV_PATH, "r");
    if (!file) return 0;

    FILE *temp = fopen(PICS_PATH"/temp.csv", "w");
    if (!temp) {
        fclose(file);
        return 0;
    }

    int read = fread(csv_read_buf, 1, CSV_READ_BUF_SIZE, file);
    if (read > 0) {
        for (int start = 0, end = 0, i = 0; i < read; i++) {
            if (csv_read_buf[i] == '\n') {
                end = i;
                char line[100] = {0};
                char line_dup[100] = {0};
                if (((end - start + 1) < sizeof(line)) && (start < end)) {

                    memcpy(line, &csv_read_buf[start], end - start + 1);
                    memcpy(line_dup, line, end - start + 1);

                    const char *field_value = get_field(line, 1);

                    if (strcmp(field_value, value)) {
                        fwrite(line_dup, 1, end - start + 1, temp);
                    }
                } else {
                    break;
                }

                start = end+1;
            }
        }
    }

    fclose(file);
    fclose(temp);
    remove(CSV_PATH);
    rename(PICS_PATH"/temp.csv", CSV_PATH);

    memset(csv_read_buf, 0, CSV_READ_BUF_SIZE);
    return 1;
}

long long sd_storage_csv_get_oldest_photo(char *oldest_photo_name)
{
    char oldest_name_temp[100] = {0};
    long long oldest_photo_ts = LONG_LONG_MAX;
   
    if (!csv_read_buf) {
        ESP_LOGE(TAG, "Csv read buffer is NULL");
        return -1;
    }

    FILE *csv_file = fopen(CSV_PATH, "r");
    if (csv_file) {
        int read = fread(csv_read_buf, 1, CSV_READ_BUF_SIZE, csv_file);
        if (read > 0) {

            for (int start = 0, end = 0, i = 0; i < read; i++) {
                if (csv_read_buf[i] == '\n') {
                    end = i;
                    
                    char line[100] = {0};
                    if (((end - start) < sizeof(line)) && (start < end)) {
                        memcpy(line, &csv_read_buf[start], end - start);

                        const char *field_value = get_field(line, 3);
                        long long temp = atoll(field_value);
                        if (temp < oldest_photo_ts) {
                            oldest_photo_ts = temp;
                            const char *temp = get_field(line, 1);

                            strcpy(oldest_name_temp, temp);
                        }

                    } else {
                        break;
                    }
                    start = end+1;
                }
            }

            if(oldest_photo_name != NULL) {
                strcpy(oldest_photo_name, oldest_name_temp);
            }
        }
    }
    fclose(csv_file);
    memset(csv_read_buf, 0, CSV_READ_BUF_SIZE);

    return oldest_photo_ts;
}

long long sd_storage_csv_get_newest_photo(char *newest_photo_name)
{
    char newest_name_temp[100] = {0};
    long long newest_photo_ts = 0;
    if (!csv_read_buf) {
        ESP_LOGE(TAG, "Csv read buffer is NULL");
        return -1;
    }

    FILE *csv_file = fopen(CSV_PATH, "r");
    if (csv_file) {
        int read = fread(csv_read_buf, 1, CSV_READ_BUF_SIZE, csv_file);
        if (read > 0) {

            for (int start = 0, end = 0, i = 0; i < read; i++) {
                if (csv_read_buf[i] == '\n') {
                    end = i;
                    
                    char line[100] = {0};
                    if (((end - start) < sizeof(line)) && (start < end)) {
                        memcpy(line, &csv_read_buf[start], end - start);

                        const char *field_value = get_field(line, 3);
                        long long temp = atoll(field_value);
                        if (temp > newest_photo_ts) {
                            newest_photo_ts = temp;
                            const char *temp = get_field(line, 1);

                            strcpy(newest_name_temp, temp);
                        }

                    } else {
                        break;
                    }
                    start = end+1;
                }
            }

            if(newest_photo_name != NULL) {
                strcpy(newest_photo_name, newest_name_temp);
            }
        }
    }
    fclose(csv_file);
    memset(csv_read_buf, 0, CSV_READ_BUF_SIZE);

    return newest_photo_ts;
}

esp_err_t sd_storage_set_max_photos(int _max_photos)
{
    max_photos = _max_photos;
    return ESP_OK;
}

esp_err_t sd_storage_csv_update(char *pic_path, size_t size, long long timestamp)
{
    esp_err_t ret = ESP_FAIL;
    char row[100] = {0};
    if (pic_path) {

        if (get_entries_size(CSV_PATH) >= max_photos) {
            char old_fn[50] = {0};
            long long ts = sd_storage_csv_get_oldest_photo(old_fn);
            ESP_LOGI(TAG, "Oldest photo (ts: %lli) (%s)", ts, old_fn);

            sprintf(row, PICS_PATH"/%s", old_fn);
            remove(row);
            sd_storage_csv_remove_entry(old_fn);
            memset(row, 0, sizeof(row));
        }

        FILE *csv_file = fopen(CSV_PATH, "a");
        sprintf(row, "%s,%d,%lli\n", pic_path, size, timestamp);

        if (csv_file) {
            fwrite(row, 1, strlen(row), csv_file);
        }
        fclose(csv_file);
    }
    return ret;
}

esp_err_t sd_storage_csv_init(void)
{
    esp_err_t ret = ESP_ERR_NO_MEM;

    csv_read_buf = heap_caps_malloc(CSV_READ_BUF_SIZE, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (csv_read_buf) {
        ret = ESP_OK;
        memset(csv_read_buf, 0, CSV_READ_BUF_SIZE);
    }
    return ret;
}
