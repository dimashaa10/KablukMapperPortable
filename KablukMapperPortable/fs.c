/*
 * Kabluk Mapper Portable - fs.c
 *
 * Directory listing + tiny path helpers for the file browser.
 */
#include "fs.h"

#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <stdint.h>
#define FS_SEP '\\'
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#define FS_SEP '/'
#endif

static int fs_is_sep(char c)
{
    return c == '/' || c == '\\';
}

void fs_copy(char *dst, int cap, const char *src)
{
    int i;

    if (dst == NULL || cap <= 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    for (i = 0; i < cap - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

void fs_join(char *out, int cap, const char *base, const char *name)
{
    int len;

    if (base == NULL || base[0] == '\0') {
        fs_copy(out, cap, name);
        return;
    }
    fs_copy(out, cap, base);
    len = (int)strlen(out);
    if (len > 0 && !fs_is_sep(out[len - 1]) && len < cap - 1) {
        out[len] = FS_SEP;
        out[len + 1] = '\0';
        len++;
    }
    if (name != NULL && name[0] != '\0' && len < cap - 1) {
        strncat(out, name, (size_t)(cap - 1 - len));
    }
}

const char *fs_basename(const char *path)
{
    int i;
    int cut = -1;

    if (path == NULL) {
        return "";
    }
    for (i = 0; path[i] != '\0'; i++) {
        if (fs_is_sep(path[i])) {
            cut = i;
        }
    }
    return path + cut + 1;
}

void fs_parent(char *path, int cap)
{
    char tmp[PATH_LEN];
    int len;
    int i;
    int cut = -1;

    fs_copy(tmp, PATH_LEN, path);
    len = (int)strlen(tmp);

    while (len > 1 && fs_is_sep(tmp[len - 1])) {
        len--;
    }
    tmp[len] = '\0';

    if (len == 0) {
        fs_copy(path, cap, ".");
        return;
    }
    if (len == 1 && fs_is_sep(tmp[0])) { /* "/" */
        fs_copy(path, cap, tmp);
        return;
    }
#ifdef _WIN32
    if (len == 3 && tmp[1] == ':' && fs_is_sep(tmp[2])) { /* "C:\" */
        fs_copy(path, cap, tmp);
        return;
    }
    if (len == 2 && tmp[1] == ':') { /* "C:" */
        tmp[2] = FS_SEP;
        tmp[3] = '\0';
        fs_copy(path, cap, tmp);
        return;
    }
#endif
    for (i = len - 1; i >= 0; i--) {
        if (fs_is_sep(tmp[i])) {
            cut = i;
            break;
        }
    }
    if (cut < 0) {
        fs_copy(path, cap, ".");
        return;
    }
    if (cut == 0) { /* "/foo" -> "/" */
        tmp[1] = '\0';
        fs_copy(path, cap, tmp);
        return;
    }
#ifdef _WIN32
    if (cut == 2 && tmp[1] == ':') { /* "C:\foo" -> "C:\" */
        tmp[3] = '\0';
        fs_copy(path, cap, tmp);
        return;
    }
#endif
    tmp[cut] = '\0';
    fs_copy(path, cap, tmp);
}

static char fs_lower(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

static int fs_stricmp(const char *a, const char *b)
{
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0') {
        char ca = fs_lower(a[i]);
        char cb = fs_lower(b[i]);
        if (ca != cb) {
            return (int)ca - (int)cb;
        }
        i++;
    }
    return (int)fs_lower(a[i]) - (int)fs_lower(b[i]);
}

int fs_has_suffix(const char *name, const char *ext)
{
    int nlen;
    int elen;
    int i;

    if (name == NULL || ext == NULL) {
        return 0;
    }
    nlen = (int)strlen(name);
    elen = (int)strlen(ext);
    if (elen == 0 || elen >= nlen) {
        return 0;
    }
    for (i = 0; i < elen; i++) {
        if (fs_lower(name[nlen - elen + i]) != fs_lower(ext[i])) {
            return 0;
        }
    }
    return 1;
}

static void fs_add(FsEntry *out, int *count, int max, const char *name, int kind)
{
    if (*count >= max) {
        return;
    }
    fs_copy(out[*count].name, FS_NAME_LEN, name);
    out[*count].kind = (unsigned char)kind;
    (*count)++;
}

static void fs_sort(FsEntry *entries, int count)
{
    int i;
    int j;
    FsEntry key;

    /* directories first, then alphabetical (insertion sort, tiny arrays) */
    for (i = 1; i < count; i++) {
        key = entries[i];
        j = i - 1;
        while (j >= 0) {
            int a_dir = entries[j].kind == FS_KIND_DIR;
            int b_dir = key.kind == FS_KIND_DIR;
            int swap;

            if (a_dir != b_dir) {
                swap = b_dir;
            } else {
                swap = fs_stricmp(entries[j].name, key.name) > 0;
            }
            if (!swap) {
                break;
            }
            entries[j + 1] = entries[j];
            j--;
        }
        entries[j + 1] = key;
    }
}

int fs_list_dir(const char *path, const char *ext, FsEntry *out, int max)
{
    const char *dir;
    int count = 0;

    if (out == NULL || max <= 0) {
        return 0;
    }
    dir = (path == NULL || path[0] == '\0') ? "." : path;

#ifdef _WIN32
    {
        char pattern[PATH_LEN + 4];
        struct _finddata_t data;
        intptr_t handle;

        fs_join(pattern, (int)sizeof(pattern), dir, "*");
        handle = _findfirst(pattern, &data);
        if (handle == -1) {
            return 0;
        }
        do {
            if (strcmp(data.name, ".") == 0 || strcmp(data.name, "..") == 0) {
                continue;
            }
            if ((data.attrib & _A_SUBDIR) != 0) {
                fs_add(out, &count, max, data.name, FS_KIND_DIR);
            } else if (ext != NULL && fs_has_suffix(data.name, ext)) {
                fs_add(out, &count, max, data.name, FS_KIND_FILE);
            }
            if (count >= max) {
                break;
            }
        } while (_findnext(handle, &data) == 0);
        _findclose(handle);
    }
#else
    {
        DIR *d;
        struct dirent *ent;

        d = opendir(dir);
        if (d == NULL) {
            return 0;
        }
        while ((ent = readdir(d)) != NULL) {
            char full[PATH_LEN];
            struct stat st;
            int is_dir;

            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
                continue;
            }
            if (count >= max) {
                break;
            }
            fs_join(full, (int)sizeof(full), dir, ent->d_name);
            if (stat(full, &st) != 0) {
                continue;
            }
            is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
            if (is_dir) {
                fs_add(out, &count, max, ent->d_name, FS_KIND_DIR);
            } else if (ext != NULL && fs_has_suffix(ent->d_name, ext)) {
                fs_add(out, &count, max, ent->d_name, FS_KIND_FILE);
            }
        }
        closedir(d);
    }
#endif

    fs_sort(out, count);
    return count;
}
