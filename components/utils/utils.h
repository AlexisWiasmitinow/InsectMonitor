#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>
#include <stddef.h>

int64_t get_timestamp(char *timestamp, size_t len);
int convert_range(int value, int old_min, int old_max, int new_min, int new_max);
void stats_task(void *arg);
double round_double(double value, int decimals);

#endif // __UTILS_H__