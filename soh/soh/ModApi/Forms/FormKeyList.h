#ifndef SOH_MOD_API_FORM_KEY_LIST_H
#define SOH_MOD_API_FORM_KEY_LIST_H

#include <cstdint>
#include <string>
#include <vector>

// Host-owned copy of a definition's allowedForms: the array a mod registers with may be a temporary.
class FormKeyList {
  public:
    FormKeyList() = default;
    // The exposed pointers aim into this object's own strings, so a copy would hand out the original's.
    FormKeyList(const FormKeyList&) = delete;
    FormKeyList& operator=(const FormKeyList&) = delete;

    static bool IsValid(const char* const* keys, uint32_t count) {
        if (count == 0) {
            return true;
        }
        if (keys == nullptr) {
            return false;
        }
        for (uint32_t index = 0; index < count; index++) {
            if (keys[index] == nullptr || keys[index][0] == '\0') {
                return false;
            }
        }
        return true;
    }

    // Assumes IsValid(keys, count).
    void Assign(const char* const* keys, uint32_t count) {
        mKeys.assign(keys, keys + count);
        mPointers.clear();
        for (const std::string& key : mKeys) {
            mPointers.push_back(key.c_str());
        }
    }

    const char* const* Data() const {
        return mPointers.empty() ? nullptr : mPointers.data();
    }

    uint32_t Count() const {
        return static_cast<uint32_t>(mPointers.size());
    }

  private:
    std::vector<std::string> mKeys;
    std::vector<const char*> mPointers;
};

#endif
