#if BUILD_WIN32
#define PATCHES_INCLUDE_PATH "win32/patches.inc"
#elif BUILD_LINUX
#define PATCHES_INCLUDE_PATH "linux/patches.inc"
#endif

#define Array(...) { __VA_ARGS__ }

#define PATCH(patch_name, patch_offset, orig_bytes, patch_bytes) \
    global const u8 patch_name##_orig[]  = orig_bytes;           \
    global const u8 patch_name##_patch[] = patch_bytes;          \
    global const Patch patch_name = {                            \
        .name     = S8LitComp(#patch_name),                      \
        .offset   = patch_offset,                                \
        .original = PatternComp(patch_name##_orig),              \
        .patched  = PatternComp(patch_name##_patch),             \
    };
#include PATCHES_INCLUDE_PATH
#undef PATCH

#define PATCH(patchname, ...) &patchname,
global const Patch* patches[] = {
#include PATCHES_INCLUDE_PATH
};
#undef PATCH

global const u32 patches_count = ArraySize(patches);

#undef Array
#undef PATCHES_INCLUDE_PATH
