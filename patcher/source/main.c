#define RIPCORD_WIN64_SIZE 5509808
#define RIPCORD_SHA256_BASELINE "12D62ABB9AD4DB43C2B9B1398ACAE66857EB6E64205364631A3D3BDA0FF17E2E"
#define RIPCORD_SHA256_PATCHED  "A99C8132F984B608CA1E1702FC3B5BB2D07554636DE822FB05C35855652B39EC"
#define RIPCORD_SHA256_PATCHED_LEGACY "D37B9FC412B684EC0741F53354BF6CC839BD9F4BBECE8754B7E46E96012255B4"

#if BUILD_WIN32
#include <bcrypt.h>

internal b32 ComputeSHA256(const u8* data, u64 size, char out_hex[65]) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    NTSTATUS status;
    u8 hash_bytes[32];
    DWORD hash_len = sizeof(hash_bytes);

    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (status != 0) return 0;

    status = BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0);
    if (status != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return 0;
    }

    status = BCryptHashData(hHash, (PUCHAR) data, (ULONG) size, 0);
    if (status != 0) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return 0;
    }

    status = BCryptFinishHash(hHash, hash_bytes, hash_len, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (status != 0) return 0;

    for (int i = 0; i < 32; ++i) {
        snprintf(out_hex + (i * 2), 3, "%02X", hash_bytes[i]);
    }
    out_hex[64] = '\0';
    return 1;
}

internal b32 ComputeFileSHA256(const char* filepath, char out_hex[65]) {
    HANDLE f = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;

    LARGE_INTEGER sz;
    if (!GetFileSizeEx(f, &sz)) {
        CloseHandle(f);
        return 0;
    }

    HANDLE m = CreateFileMappingA(f, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!m) {
        CloseHandle(f);
        return 0;
    }

    const u8* ptr = (const u8*) MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
    if (!ptr) {
        CloseHandle(m);
        CloseHandle(f);
        return 0;
    }

    b32 ok = ComputeSHA256(ptr, (u64) sz.QuadPart, out_hex);
    UnmapViewOfFile(ptr);
    CloseHandle(m);
    CloseHandle(f);
    return ok;
}
#endif

typedef enum {
    ACTION_PATCH,
    ACTION_REVERT,
    ACTION_VERIFY,
    ACTION_LIST,
    ACTION_HELP,
} Action;

internal b32 CreateFileBackup(const char* filepath, const char* expected_sha256) {
    char backup_path[MAX_PATH];
    snprintf(backup_path, sizeof(backup_path), "%s.bak", filepath);
    
    // Check if backup already exists
    DWORD attr = GetFileAttributesA(backup_path);
    if (attr != INVALID_FILE_ATTRIBUTES) {
        printf("Backup already exists at: %s\n", backup_path);
        return 1;
    }
    
    if (!CopyFileA(filepath, backup_path, TRUE)) {
        fprintf(stderr, "ERROR: Failed to create backup file %s (error %lu)\n", backup_path, GetLastError());
        return 0;
    }

    // Verify backup integrity
    char backup_sha[65] = {0};
    if (!ComputeFileSHA256(backup_path, backup_sha) || strcmp(backup_sha, expected_sha256) != 0) {
        fprintf(stderr, "ERROR: Backup integrity verification failed! Removing corrupted backup.\n");
        DeleteFileA(backup_path);
        return 0;
    }

    printf("Successfully created verified backup: %s (SHA256: %s)\n", backup_path, backup_sha);
    return 1;
}

i32 main(i32 args_count, char** args) {
    Action action = ACTION_PATCH;
    const char* filename = DEFAULT_FILENAME;
    b32 dry_run = 0;
    b32 no_backup = 0;
    b32 force = 0;

    for (i32 i = 1; i < args_count; ++i) {
        if (strcmp(args[i], "-h") == 0 || strcmp(args[i], "--help") == 0) {
            action = ACTION_HELP;
        } else if (strcmp(args[i], "-l") == 0 || strcmp(args[i], "--list") == 0) {
            action = ACTION_LIST;
        } else if (strcmp(args[i], "-v") == 0 || strcmp(args[i], "--verify") == 0) {
            action = ACTION_VERIFY;
        } else if (strcmp(args[i], "-u") == 0 || strcmp(args[i], "--revert") == 0 || strcmp(args[i], "--unpatch") == 0) {
            action = ACTION_REVERT;
        } else if (strcmp(args[i], "-n") == 0 || strcmp(args[i], "--dry-run") == 0) {
            dry_run = 1;
        } else if (strcmp(args[i], "--no-backup") == 0) {
            no_backup = 1;
        } else if (strcmp(args[i], "-f") == 0 || strcmp(args[i], "--force") == 0) {
            force = 1;
        } else if (args[i][0] != '-') {
            filename = args[i];
        }
    }

    if (action == ACTION_HELP) {
        printf("Ripcord Hardened Binary Patcher (Windows x64)\n");
        printf("Usage: %s [options] [target_file (%s)]\n\n", args[0], DEFAULT_FILENAME);
        printf("Actions:\n");
        printf("  (default)          Safely apply verified patches with automatic verified backup\n");
        printf("  -v, --verify       Inspect patch status and verify binary SHA-256 integrity\n");
        printf("  -u, --revert       Revert patches bit-for-bit to official unpatched baseline\n");
        printf("  -l, --list         List all available patches and target offsets\n");
        printf("  -h, --help         Show this help message\n\n");
        printf("Safety Options:\n");
        printf("  -n, --dry-run      Simulate patching/reverting without modifying file\n");
        printf("  --no-backup        Do not create .bak backup before modifying\n");
        printf("  -f, --force        Override non-fatal warnings (refuses corrupt binaries)\n");
        return 0;
    }

    if (action == ACTION_LIST) {
        printf("Available patches for Ripcord 0.4.29 x64 (total: %u):\n", patches_count);
        for (u32 p = 0; p < patches_count; ++p) {
            printf("  [%u] %-28.*s (offset 0x%06X, size %u bytes)\n",
                   p + 1, S8VArg(patches[p]->name), patches[p]->offset, patches[p]->patched.size);
        }
        return 0;
    }

#if BUILD_WIN32
    DWORD desired_access = (action == ACTION_VERIFY || dry_run) ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE);
    DWORD share_mode = FILE_SHARE_READ;
    HANDLE file = CreateFileA(filename, desired_access, share_mode, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "ERROR: Failed to open '%s' (error code %lu)\n", filename, GetLastError());
        return 1;
    }

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file, &file_size)) {
        fprintf(stderr, "ERROR: Failed to get file size for '%s'\n", filename);
        CloseHandle(file);
        return 1;
    }

    if (file_size.QuadPart != RIPCORD_WIN64_SIZE) {
        fprintf(stderr, "ERROR: File size (%lld bytes) does not match expected official Ripcord 0.4.29 x64 (%d bytes)!\n",
                file_size.QuadPart, RIPCORD_WIN64_SIZE);
        fprintf(stderr, "ABORT: Refusing to modify incompatible or unknown binary.\n");
        CloseHandle(file);
        return 1;
    }

    DWORD flProtect = (action == ACTION_VERIFY || dry_run) ? PAGE_READONLY : PAGE_READWRITE;
    HANDLE file_mapping = CreateFileMappingA(file, NULL, flProtect, 0, 0, NULL);
    if (file_mapping == NULL) {
        fprintf(stderr, "ERROR: Failed to create file mapping (error code %lu)\n", GetLastError());
        CloseHandle(file);
        return 1;
    }

    DWORD map_access = (action == ACTION_VERIFY || dry_run) ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS;
    u8* data = (u8*) MapViewOfFile(file_mapping, map_access, 0, 0, 0);
    if (!data) {
        fprintf(stderr, "ERROR: Failed to map view of file (error code %lu)\n", GetLastError());
        CloseHandle(file_mapping);
        CloseHandle(file);
        return 1;
    }

    // Compute cryptographic SHA-256 of target binary
    char current_sha[65] = {0};
    if (!ComputeSHA256(data, (u64) file_size.QuadPart, current_sha)) {
        fprintf(stderr, "ERROR: Failed to compute SHA-256 hash of '%s'\n", filename);
        UnmapViewOfFile(data);
        CloseHandle(file_mapping);
        CloseHandle(file);
        return 1;
    }
#endif

    // Run version and integrity checks
    b32 failed_checks = 0;
    for (u32 c_idx = 0; c_idx < checks_count; ++c_idx) {
        const Check* c = checks[c_idx];
        if (c->offset + c->pattern.size > (u64) file_size.QuadPart) {
            fprintf(stderr, "ERROR: Check '%.*s' offset out of file bounds!\n", S8VArg(c->name));
            failed_checks = 1;
            continue;
        }
        if (memcmp(data + c->offset, PatternVArg(c->pattern)) != 0) {
            fprintf(stderr, "ERROR: Integrity check '%.*s' failed at offset 0x%06X!\n",
                    S8VArg(c->name), c->offset);
            failed_checks = 1;
        }
    }

    if (failed_checks) {
        fprintf(stderr, "ABORT: Target file fails Ripcord version/integrity checks. Modification disallowed.\n");
#if BUILD_WIN32
        UnmapViewOfFile(data);
        CloseHandle(file_mapping);
        CloseHandle(file);
#endif
        return 1;
    }

    b32 is_official_baseline = (strcmp(current_sha, RIPCORD_SHA256_BASELINE) == 0);
    b32 is_official_patched  = (strcmp(current_sha, RIPCORD_SHA256_PATCHED) == 0 || strcmp(current_sha, RIPCORD_SHA256_PATCHED_LEGACY) == 0);

    if (action == ACTION_VERIFY) {
        printf("=== Ripcord Patch & Integrity Verification for '%s' ===\n", filename);
        printf("  File Size : %lld bytes (Matches 0.4.29 x64: YES)\n", file_size.QuadPart);
        printf("  SHA-256   : %s\n", current_sha);
        printf("  Status    : %s\n\n",
               is_official_baseline ? "EXACT OFFICIAL UNPATCHED BASELINE" :
               is_official_patched  ? "EXACT OFFICIAL HARDENED PATCHED BINARY" : "MODIFIED / UNKNOWN VERSION");

        u32 count_patched = 0;
        u32 count_original = 0;
        u32 count_mismatch = 0;

        for (u32 p = 0; p < patches_count; ++p) {
            const Patch* patch = patches[p];
            u8* target = data + patch->offset;
            b32 is_orig = (memcmp(target, PatternVArg(patch->original)) == 0);
            b32 is_patch = (memcmp(target, PatternVArg(patch->patched)) == 0);

            if (is_patch) {
                printf("  [PATCHED]    %-28.*s (offset 0x%06X)\n", S8VArg(patch->name), patch->offset);
                count_patched++;
            } else if (is_orig) {
                printf("  [ORIGINAL]   %-28.*s (offset 0x%06X)\n", S8VArg(patch->name), patch->offset);
                count_original++;
            } else {
                printf("  [MISMATCH]   %-28.*s (offset 0x%06X) - Unexpected bytes!\n", S8VArg(patch->name), patch->offset);
                count_mismatch++;
            }
        }

        printf("\nSummary: %u Patched, %u Original, %u Mismatched (Total %u)\n",
               count_patched, count_original, count_mismatch, patches_count);

#if BUILD_WIN32
        UnmapViewOfFile(data);
        CloseHandle(file_mapping);
        CloseHandle(file);
#endif
        return (count_mismatch > 0) ? 2 : 0;
    }

    if (action == ACTION_PATCH) {
        if (is_official_patched) {
            printf("Target file '%s' is ALREADY fully patched (SHA256: %s).\n", filename, current_sha);
            printf("No further modifications needed.\n");
#if BUILD_WIN32
            UnmapViewOfFile(data);
            CloseHandle(file_mapping);
            CloseHandle(file);
#endif
            return 0;
        }

        if (!is_official_baseline && !force) {
            fprintf(stderr, "ABORT: Target binary SHA-256 (%s) does not match official Ripcord 0.4.29 x64 baseline!\n", current_sha);
            fprintf(stderr, "Expected: %s\n", RIPCORD_SHA256_BASELINE);
            fprintf(stderr, "Refusing to patch unknown or modified binary (use -f to force if intentional).\n");
#if BUILD_WIN32
            UnmapViewOfFile(data);
            CloseHandle(file_mapping);
            CloseHandle(file);
#endif
            return 1;
        }

        // Pre-validate all patches
        b32 can_patch = 1;
        for (u32 p = 0; p < patches_count; ++p) {
            const Patch* patch = patches[p];
            u8* target = data + patch->offset;
            b32 is_orig = (memcmp(target, PatternVArg(patch->original)) == 0);
            b32 is_patch = (memcmp(target, PatternVArg(patch->patched)) == 0);
            if (!is_orig && !is_patch) {
                fprintf(stderr, "ERROR: Patch '%.*s' cannot be applied: offset 0x%06X contains unexpected bytes!\n",
                        S8VArg(patch->name), patch->offset);
                can_patch = 0;
            }
        }

        if (!can_patch) {
            fprintf(stderr, "ABORT: Pre-validation failed. No modifications were made.\n");
#if BUILD_WIN32
            UnmapViewOfFile(data);
            CloseHandle(file_mapping);
            CloseHandle(file);
#endif
            return 1;
        }

        if (!dry_run && !no_backup) {
#if BUILD_WIN32
            UnmapViewOfFile(data);
            CloseHandle(file_mapping);
            CloseHandle(file);
            
            if (!CreateFileBackup(filename, current_sha)) {
                fprintf(stderr, "ABORT: Backup creation failed. Aborting modification.\n");
                return 1;
            }

            // Re-open for write
            file = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, share_mode, NULL, OPEN_EXISTING, 0, NULL);
            file_mapping = CreateFileMappingA(file, NULL, PAGE_READWRITE, 0, 0, NULL);
            data = (u8*) MapViewOfFile(file_mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
#endif
        }

        printf("=== Applying Patches to '%s' ===\n", filename);
        u32 applied_count = 0;
        for (u32 p = 0; p < patches_count; ++p) {
            const Patch* patch = patches[p];
            u8* target = data + patch->offset;
            if (memcmp(target, PatternVArg(patch->patched)) == 0) {
                printf("  [ALREADY]  %-28.*s (offset 0x%06X)\n", S8VArg(patch->name), patch->offset);
            } else {
                if (!dry_run) {
                    memcpy(target, PatternVArg(patch->patched));
                }
                printf("  [APPLIED]  %-28.*s (offset 0x%06X)\n", S8VArg(patch->name), patch->offset);
                applied_count++;
            }
        }

        printf("Result: %u patches applied successfully%s.\n", applied_count, dry_run ? " (DRY RUN)" : "");

        if (!dry_run) {
            char post_sha[65] = {0};
            ComputeSHA256(data, (u64) file_size.QuadPart, post_sha);
            printf("Post-patch SHA-256: %s (Verified Patched Signature: %s)\n",
                   post_sha, (strcmp(post_sha, RIPCORD_SHA256_PATCHED) == 0) ? "YES" : "NO");
        }
    } else if (action == ACTION_REVERT) {
        if (is_official_baseline) {
            printf("Target file '%s' is ALREADY the unpatched baseline (SHA256: %s).\n", filename, current_sha);
            printf("No reversion needed.\n");
#if BUILD_WIN32
            UnmapViewOfFile(data);
            CloseHandle(file_mapping);
            CloseHandle(file);
#endif
            return 0;
        }

        if (!dry_run && !no_backup) {
#if BUILD_WIN32
            UnmapViewOfFile(data);
            CloseHandle(file_mapping);
            CloseHandle(file);
            CreateFileBackup(filename, current_sha);
            file = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, share_mode, NULL, OPEN_EXISTING, 0, NULL);
            file_mapping = CreateFileMappingA(file, NULL, PAGE_READWRITE, 0, 0, NULL);
            data = (u8*) MapViewOfFile(file_mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
#endif
        }

        printf("=== Reverting Patches in '%s' ===\n", filename);
        u32 reverted_count = 0;
        for (u32 p = 0; p < patches_count; ++p) {
            const Patch* patch = patches[p];
            u8* target = data + patch->offset;
            if (memcmp(target, PatternVArg(patch->original)) == 0) {
                printf("  [ORIGINAL] %-28.*s (already unpatched)\n", S8VArg(patch->name));
            } else if (memcmp(target, PatternVArg(patch->patched)) == 0) {
                if (!dry_run) {
                    memcpy(target, PatternVArg(patch->original));
                }
                printf("  [REVERTED] %-28.*s (offset 0x%06X)\n", S8VArg(patch->name), patch->offset);
                reverted_count++;
            } else {
                fprintf(stderr, "  [UNKNOWN]  %-28.*s (offset 0x%06X has unknown bytes!)\n", S8VArg(patch->name), patch->offset);
            }
        }
        printf("Result: %u patches reverted successfully%s.\n", reverted_count, dry_run ? " (DRY RUN)" : "");

        if (!dry_run) {
            char post_sha[65] = {0};
            ComputeSHA256(data, (u64) file_size.QuadPart, post_sha);
            printf("Post-revert SHA-256: %s\n", post_sha);
            if (strcmp(post_sha, RIPCORD_SHA256_BASELINE) == 0) {
                printf("SUCCESS: Bit-perfect restoration to official Ripcord 0.4.29 baseline verified!\n");
            } else {
                fprintf(stderr, "WARNING: Post-revert hash does not match baseline (%s vs %s)!\n",
                        post_sha, RIPCORD_SHA256_BASELINE);
            }
        }
    }

#if BUILD_WIN32
    UnmapViewOfFile(data);
    CloseHandle(file_mapping);
    CloseHandle(file);
#endif
    return 0;
}
