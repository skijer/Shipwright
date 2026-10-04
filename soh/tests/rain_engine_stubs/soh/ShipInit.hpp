#pragma once
#include <initializer_list>
struct RegisterShipInitFunc {
    static inline int updatePathCount = 0;

    RegisterShipInitFunc(void (*)(), std::initializer_list<const char*> updatePaths = {}) {
        updatePathCount += static_cast<int>(updatePaths.size());
    }
};
