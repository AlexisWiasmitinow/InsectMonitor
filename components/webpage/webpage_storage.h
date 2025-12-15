#ifndef _WEBPAGE_STORAGE_H_
#define _WEBPAGE_STORAGE_H_

#include <esp_err.h>
#include <stdbool.h>

typedef FILE webpage_file;

esp_err_t webpage_storage_init(void);
size_t webpage_storage_read(webpage_file *file, char *buf, size_t chunk_size);
webpage_file* webpage_storage_open_file(char *uri, char *type, bool local);
void webpage_storage_close_file(webpage_file *file);

#endif //_WEBPAGE_STORAGE_H_