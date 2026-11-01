/*
 * Kabluk Mapper Portable - fgd.c
 *
 * A tolerant parser for Worldcraft .fgd files. It has to cope with what the
 * real Quake.fgd throws at it: multi line class headers, model({{ ... }})
 * blocks, multi line descriptions and nested [ ] lists for choices/flags.
 *
 * Parsing happens in two steps:
 *   1. fgd_load() strips // comments and indexes every @Class directive
 *      (name, kind, description, body offsets, base classes).
 *   2. fgd_resolve() parses the body of one class on demand, after its base
 *      classes, and merges everything into fgd->props.
 */
#include "fgd.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Small text helpers                                                  */
/* ------------------------------------------------------------------ */
static int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int is_ident(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
}

static char lower_c(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static int strieq(const char *a, const char *b)
{
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (lower_c(a[i]) != lower_c(b[i])) {
            return 0;
        }
        i++;
    }
    return a[i] == b[i];
}

static void copy_str(char *dst, int cap, const char *src)
{
    int i;
    for (i = 0; i < cap - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static int skip_ws(const char *t, int i, int end)
{
    while (i < end && is_space(t[i])) {
        i++;
    }
    return i;
}

static int read_ident(const char *t, int i, int end, char *out, int cap)
{
    int n = 0;

    while (i < end && is_ident(t[i])) {
        if (n < cap - 1) {
            out[n++] = t[i];
        }
        i++;
    }
    out[n] = '\0';
    return i;
}

/* t[i] is a quote; returns the index just past the closing quote */
static int skip_string(const char *t, int i, int end)
{
    i++;
    while (i < end) {
        if (t[i] == '\\' && i + 1 < end) {
            i += 2;
            continue;
        }
        if (t[i] == '"') {
            return i + 1;
        }
        i++;
    }
    return end;
}

static int read_string(const char *t, int i, int end, char *out, int cap)
{
    int n = 0;

    i = skip_ws(t, i, end);
    if (i >= end || t[i] != '"') {
        out[0] = '\0';
        return i;
    }
    i++;
    while (i < end) {
        char c = t[i];

        if (c == '\\' && i + 1 < end) {
            i++;
            if (n < cap - 1) {
                out[n++] = t[i];
            }
            i++;
            continue;
        }
        if (c == '"') {
            i++;
            break;
        }
        if (c == '\n' || c == '\r') {
            /* descriptions may span lines: collapse them into one line */
            if (n > 0 && out[n - 1] != ' ') {
                if (n < cap - 1) {
                    out[n++] = ' ';
                }
            }
            i = skip_ws(t, i, end);
            continue;
        }
        if (n < cap - 1) {
            out[n++] = c;
        }
        i++;
    }
    out[n] = '\0';
    return i;
}

/* t[i] == '['; returns the index of the matching ']' or 'end' */
static int match_bracket(const char *t, int i, int end)
{
    int depth = 0;

    while (i < end) {
        if (t[i] == '"') {
            i = skip_string(t, i, end);
            continue;
        }
        if (t[i] == '[') {
            depth++;
        } else if (t[i] == ']') {
            depth--;
            if (depth == 0) {
                return i;
            }
        }
        i++;
    }
    return end;
}

/* Removes // comments, honouring quoted strings. Offsets stay valid. */
static void strip_comments(char *t, int len)
{
    int in_string = 0;
    int i;

    for (i = 0; i < len; i++) {
        if (in_string) {
            if (t[i] == '\\') {
                i++;
            } else if (t[i] == '"') {
                in_string = 0;
            }
            continue;
        }
        if (t[i] == '"') {
            in_string = 1;
            continue;
        }
        if (t[i] == '/' && i + 1 < len && t[i + 1] == '/') {
            while (i < len && t[i] != '\n') {
                t[i] = ' ';
                i++;
            }
        }
    }
}

/* Finds the '=' between a class header and its name, skipping quotes and
 * () / {} blocks (model({{ ... }}) lives there). */
static int find_class_eq(const char *t, int i, int end)
{
    int paren = 0;
    int brace = 0;

    while (i < end) {
        char c = t[i];

        if (c == '"') {
            i = skip_string(t, i, end);
            continue;
        }
        if (c == '(') {
            paren++;
        } else if (c == ')') {
            if (paren > 0) {
                paren--;
            }
        } else if (c == '{') {
            brace++;
        } else if (c == '}') {
            if (brace > 0) {
                brace--;
            }
        } else if (paren == 0 && brace == 0) {
            if (c == '=') {
                return i;
            }
            if (c == '@') {
                return -1;
            }
        }
        i++;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Class index                                                         */
/* ------------------------------------------------------------------ */
static int kind_from_keyword(const char *kw)
{
    if (strieq(kw, "pointclass")) {
        return FGD_KIND_POINT;
    }
    if (strieq(kw, "solidclass")) {
        return FGD_KIND_SOLID;
    }
    if (strieq(kw, "baseclass") || strieq(kw, "flagclass")) {
        return FGD_KIND_BASE;
    }
    return -1;
}

static void add_base(FgdClass *cls, const char *name)
{
    if (cls->base_count >= FGD_MAX_BASES || name[0] == '\0') {
        return;
    }
    copy_str(cls->bases[cls->base_count], FGD_NAME_LEN, name);
    cls->base_count++;
}

static void parse_bases(FgdClass *cls, const char *t, int start, int end)
{
    int i = start;

    while (i < end) {
        if ((t[i] == 'b' || t[i] == 'B') && i + 4 <= end &&
            lower_c(t[i + 1]) == 'a' && lower_c(t[i + 2]) == 's' &&
            lower_c(t[i + 3]) == 'e') {
            int j = skip_ws(t, i + 4, end);

            if (j < end && t[j] == '(') {
                int close = j + 1;
                int k;

                while (close < end && t[close] != ')') {
                    close++;
                }
                k = j + 1;
                while (k < close) {
                    char name[FGD_NAME_LEN];

                    k = skip_ws(t, k, close);
                    if (k < close && t[k] == ',') {
                        k++;
                        continue;
                    }
                    k = read_ident(t, k, close, name, (int)sizeof(name));
                    if (name[0] != '\0') {
                        add_base(cls, name);
                    } else {
                        k++;
                    }
                }
                i = close + 1;
                continue;
            }
        }
        i++;
    }
}

static void add_class(Fgd *fgd, const char *name, const char *desc, int kind,
                      int body_start, int body_end, const char *t, int header_start,
                      int header_end)
{
    FgdClass *cls;

    if (fgd->class_count >= FGD_MAX_CLASSES) {
        return;
    }
    cls = &fgd->classes[fgd->class_count];
    copy_str(cls->name, FGD_NAME_LEN, name);
    copy_str(cls->desc, FGD_DESC_LEN, desc);
    cls->kind = (unsigned char)kind;
    cls->body_start = body_start;
    cls->body_end = body_end;
    cls->base_count = 0;
    parse_bases(cls, t, header_start, header_end);
    fgd->class_count++;
}

int fgd_count(const Fgd *fgd)
{
    return fgd->class_count;
}

const FgdClass *fgd_class(const Fgd *fgd, int index)
{
    if (index < 0 || index >= fgd->class_count) {
        return NULL;
    }
    return &fgd->classes[index];
}

int fgd_find(const Fgd *fgd, const char *name)
{
    int i;

    if (name == NULL) {
        return -1;
    }
    for (i = 0; i < fgd->class_count; i++) {
        if (strieq(fgd->classes[i].name, name)) {
            return i;
        }
    }
    return -1;
}

int fgd_point_list(const Fgd *fgd, int *out, int max)
{
    int i;
    int n = 0;

    for (i = 0; i < fgd->class_count && n < max; i++) {
        if (fgd->classes[i].kind == FGD_KIND_POINT) {
            out[n++] = i;
        }
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* Properties                                                          */
/* ------------------------------------------------------------------ */
static void add_prop(Fgd *fgd, const char *key, const char *type,
                     const char *value, const char *desc)
{
    FgdProp *prop;
    int i;

    if (key[0] == '\0') {
        return;
    }
    for (i = 0; i < fgd->prop_count; i++) {
        if (strieq(fgd->props[i].key, key)) {
            prop = &fgd->props[i]; /* a derived class overrides its bases */
            if (type[0] != '\0') {
                copy_str(prop->type, FGD_TYPE_LEN, type);
            }
            if (value[0] != '\0') {
                copy_str(prop->value, FGD_VALUE_LEN, value);
            }
            if (desc[0] != '\0') {
                copy_str(prop->desc, FGD_DESC_LEN, desc);
            }
            return;
        }
    }
    if (fgd->prop_count >= FGD_MAX_PROPS) {
        return;
    }
    prop = &fgd->props[fgd->prop_count];
    copy_str(prop->key, FGD_NAME_LEN, key);
    copy_str(prop->type, FGD_TYPE_LEN, type);
    copy_str(prop->value, FGD_VALUE_LEN, value);
    copy_str(prop->desc, FGD_DESC_LEN, desc);
    fgd->prop_count++;
}

/* Reads one default value: "text", 42, or a [ ... ] list (summarised). */
static int read_value(const char *t, int i, int end, char *out, int cap)
{
    out[0] = '\0';
    i = skip_ws(t, i, end);
    if (i >= end) {
        return i;
    }
    if (t[i] == '[') {
        copy_str(out, cap, "(list)");
        i = match_bracket(t, i, end);
        return i < end ? i + 1 : end;
    }
    if (t[i] == '"') {
        return read_string(t, i, end, out, cap);
    }
    {
        int n = 0;
        while (i < end && !is_space(t[i]) && t[i] != ',' && t[i] != ']') {
            if (n < cap - 1) {
                out[n++] = t[i];
            }
            i++;
        }
        out[n] = '\0';
    }
    return i;
}

static void parse_body(Fgd *fgd, int start, int end)
{
    const char *t = fgd->text;
    int i = start;

    if (start < 0 || end <= start) {
        return;
    }
    while (i < end) {
        char key[FGD_NAME_LEN];
        char type[FGD_TYPE_LEN];
        char value[FGD_VALUE_LEN];
        char desc[FGD_DESC_LEN];

        i = skip_ws(t, i, end);
        if (i >= end) {
            break;
        }
        if (t[i] == ',' || t[i] == '[' || t[i] == ']') {
            i++;
            continue;
        }

        key[0] = '\0';
        i = read_ident(t, i, end, key, (int)sizeof(key));
        if (key[0] == '\0') {
            i++;
            continue;
        }
        i = skip_ws(t, i, end);
        type[0] = '\0';
        if (i < end && t[i] == '(') {
            int close = i + 1;
            int n = 0;

            while (close < end && t[close] != ')') {
                close++;
            }
            while (i + 1 + n < close && n < FGD_TYPE_LEN - 1) {
                type[n] = t[i + 1 + n];
                n++;
            }
            type[n] = '\0';
            i = close < end ? close + 1 : end;
        }
        i = skip_ws(t, i, end);
        value[0] = '\0';
        desc[0] = '\0';
        if (i < end && t[i] == ':') {
            i = read_string(t, i + 1, end, desc, (int)sizeof(desc));
            i = skip_ws(t, i, end);
            if (i < end && (t[i] == '=' || t[i] == ':')) {
                i = read_value(t, i + 1, end, value, (int)sizeof(value));
            }
        } else if (i < end && t[i] == '=') {
            i = read_value(t, i + 1, end, value, (int)sizeof(value));
        }

        /* a trailing "= [ ... ]" list of choices/flags */
        for (;;) {
            i = skip_ws(t, i, end);
            if (i < end && (t[i] == '=' || t[i] == ':')) {
                i++;
                i = skip_ws(t, i, end);
                if (i < end && t[i] == '[') {
                    i = match_bracket(t, i, end);
                    if (i < end) {
                        i++;
                    }
                }
                continue;
            }
            break;
        }

        add_prop(fgd, key, type, value, desc);
    }
}

static void resolve_class(Fgd *fgd, int index, unsigned char *visited, int depth)
{
    const FgdClass *cls;
    int i;

    if (index < 0 || index >= fgd->class_count || depth > 8) {
        return;
    }
    if (visited[index]) {
        return;
    }
    visited[index] = 1;
    cls = &fgd->classes[index];

    for (i = 0; i < cls->base_count; i++) {
        resolve_class(fgd, fgd_find(fgd, cls->bases[i]), visited, depth + 1);
    }
    parse_body(fgd, cls->body_start, cls->body_end);
}

int fgd_resolve(Fgd *fgd, int index)
{
    unsigned char visited[FGD_MAX_CLASSES];

    memset(visited, 0, sizeof(visited));
    fgd->prop_count = 0;
    fgd->props_class = index;
    resolve_class(fgd, index, visited, 0);
    return fgd->prop_count;
}

/* ------------------------------------------------------------------ */
/* Loading                                                             */
/* ------------------------------------------------------------------ */
static int load_file(Fgd *fgd, const char *path)
{
    FILE *file;
    size_t got;

    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    got = fread(fgd->text, 1, (size_t)(FGD_TEXT_MAX - 1), file);
    fclose(file);
    if (got == 0) {
        return 0;
    }
    fgd->text[got] = '\0';
    fgd->text_len = (int)got;
    return 1;
}

int fgd_load(Fgd *fgd, const char *path)
{
    static const char *candidates[] = {
        FGD_FILE_NAME,
        "resources/" FGD_FILE_NAME,
        "../resources/" FGD_FILE_NAME,
        "../../resources/" FGD_FILE_NAME,
        "../../../resources/" FGD_FILE_NAME,
        "../../../../resources/" FGD_FILE_NAME,
        NULL
    };
    const char *t;
    int end;
    int i = 0;
    int found = 0;

    fgd->loaded = 0;
    fgd->class_count = 0;
    fgd->prop_count = 0;
    fgd->props_class = -1;
    fgd->text_len = 0;

    if (path != NULL && path[0] != '\0') {
        found = load_file(fgd, path);
    } else {
        int c;
        for (c = 0; candidates[c] != NULL && !found; c++) {
            found = load_file(fgd, candidates[c]);
        }
    }
    if (!found) {
        return 0;
    }

    strip_comments(fgd->text, fgd->text_len);
    t = fgd->text;
    end = fgd->text_len;

    while (i < end) {
        int at = i;

        while (at < end && t[at] != '@') {
            at++;
        }
        if (at >= end) {
            break;
        }
        {
            char keyword[24];
            int after = at + 1;
            int kind;
            int eq;

            after = skip_ws(t, after, end);
            after = read_ident(t, after, end, keyword, (int)sizeof(keyword));
            kind = kind_from_keyword(keyword);
            eq = find_class_eq(t, after, end);

            if (kind >= 0 && eq > after) {
                char name[FGD_NAME_LEN];
                char desc[FGD_DESC_LEN];
                int k = skip_ws(t, eq + 1, end);

                k = read_ident(t, k, end, name, (int)sizeof(name));
                if (name[0] != '\0') {
                    int body_start = -1;
                    int body_end = -1;

                    k = skip_ws(t, k, end);
                    desc[0] = '\0';
                    if (k < end && t[k] == ':') {
                        k = read_string(t, k + 1, end, desc, (int)sizeof(desc));
                    }
                    k = skip_ws(t, k, end);
                    if (k < end && t[k] == '[') {
                        body_start = k + 1;
                        body_end = match_bracket(t, k, end);
                        i = body_end < end ? body_end + 1 : end;
                    } else {
                        i = k > at ? k : at + 1;
                    }
                    add_class(fgd, name, desc, kind, body_start, body_end, t, at, eq);
                    continue;
                }
            }
        }
        i = at + 1;
    }

    fgd->loaded = fgd->class_count > 0;
    return fgd->loaded;
}
