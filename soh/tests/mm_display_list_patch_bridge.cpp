// Optional ctypes entry point for read-only testing of archived command streams.
// Compile together with mm_display_list_patch.cpp as a shared library.
#include "../mods/transformation_masks/assets/mm_display_list_patch.h"

extern "C" bool Test_PatchMmDisplayList(MmDisplayListCommand* commands, size_t count,
                                        MmDisplayListResolveResource resolver, void* context,
                                        MmDisplayListPatchStats* stats) {
    return MmDisplayList_PatchCommands(commands, count, resolver, context, stats);
}
