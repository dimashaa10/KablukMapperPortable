/*
 * Kabluk Mapper Portable - fs.h
 *
 * The few filesystem bits the editor needs for its file browser. SDL2 has no
 * directory listing, so this is the only place with a platform ifdef:
 *   _WIN32  -> _findfirst/_findnext
 *   other   -> opendir/readdir (Linux, and newlib on the PSP)
 * No dynamic allocation, the caller provides the entry array.
 */
#ifndef KMP_FS_H
#define KMP_FS_H

#include "config.h"

#define FS_NAME_LEN 96

#define FS_KIND_FILE 0
#define FS_KIND_DIR 1
#define FS_KIND_USE_THIS 2 /* "( USE THIS FOLDER )" pseudo entry */
#define FS_KIND_PARENT 3   /* ".." pseudo entry              */

typedef struct FsEntry {
    char name[FS_NAME_LEN];
    unsigned char kind;
} FsEntry;

/* Fills 'out' with up to 'max' entries of 'path'. Directories are always
 * listed; files only when they match 'ext' (NULL = no files at all).
 * Returns the number of entries written. */
int fs_list_dir(const char *path, const char *ext, FsEntry *out, int max);

void fs_copy(char *dst, int cap, const char *src);
void fs_join(char *out, int cap, const char *base, const char *name);
void fs_parent(char *path, int cap);
const char *fs_basename(const char *path);
int fs_has_suffix(const char *name, const char *ext);

#endif /* KMP_FS_H */
