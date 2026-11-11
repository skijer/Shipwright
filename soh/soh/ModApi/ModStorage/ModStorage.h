#ifndef SOH_MOD_STORAGE_H
#define SOH_MOD_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool ModStorage_Has(const char* mod, const char* key);
uint32_t ModStorage_GetSize(const char* mod, const char* key);
uint32_t ModStorage_Get(const char* mod, const char* key, void* output, uint32_t capacity);
bool ModStorage_Set(const char* mod, const char* key, const void* data, uint32_t size);
bool ModStorage_Remove(const char* mod, const char* key);
uint32_t ModStorage_GetString(const char* mod, const char* key, char* output, uint32_t capacity);
bool ModStorage_SetString(const char* mod, const char* key, const char* value);

#ifdef __cplusplus
}

void ModStorage_Init();
#endif

#endif
