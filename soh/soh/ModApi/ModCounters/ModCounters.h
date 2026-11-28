#ifndef SOH_MOD_API_MOD_COUNTERS_H
#define SOH_MOD_API_MOD_COUNTERS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// A named number a mod keeps in the save, from 0 to `max`, shown in the save editor's Mod Flags card.
bool ModCounters_Register(const char* key, int32_t max);
int32_t ModCounters_Get(const char* key);
void ModCounters_Set(const char* key, int32_t value);

#ifdef __cplusplus
}

#include <string>
#include <vector>

struct ModCounter {
    std::string key;
    int32_t max;
    int32_t value;
};

const std::vector<ModCounter>& ModCounters_List();
void ModCounters_Init();
#endif

#endif
