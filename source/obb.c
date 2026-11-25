/* obb.c -- Standard ZIP archive reader for assets/assets.zip */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <zlib.h>
#include "obb.h"

extern int debugPrintf(char *fmt, ...);

#define ZIP_SIG_LOCAL   0x04034b50u
#define ZIP_SIG_CENTRAL 0x02014b50u
#define ZIP_SIG_EOCD    0x06054b50u

typedef struct {
    char *name;
    uint16_t method;
    uint32_t comp_size;
    uint32_t uncomp_size;
    uint32_t local_hdr_offset;
} ZipEntry;

static FILE *g_fp = NULL;
static ZipEntry *g_entries = NULL;
static size_t g_count = 0;

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int entry_cmp(const void *a, const void *b) {
    return strcmp(((const ZipEntry *)a)->name, ((const ZipEntry *)b)->name);
}

static const char *normalize_name(const char *name) {
    while (*name == '/') name++;
    if (strncmp(name, "assets/", 7) == 0) name += 7;
    return name;
}

int obb_open(const char *path) {
    obb_close();
    g_fp = fopen(path, "rb");
    if (!g_fp) {
        debugPrintf("zip: cannot open %s\n", path);
        return -1;
    }

    if (fseek(g_fp, 0, SEEK_END) != 0) { obb_close(); return -1; }
    long fsize = ftell(g_fp);
    if (fsize < 22) { obb_close(); return -1; }

    /* 파일 끝에서 최대 64KB 범위 내 EOCD 시그니처 탐색 */
    long search_len = fsize < 65557L ? fsize : 65557L;
    long search_start = fsize - search_len;
    uint8_t *tail = (uint8_t *)malloc((size_t)search_len);
    if (!tail) { obb_close(); return -1; }

    fseek(g_fp, search_start, SEEK_SET);
    if (fread(tail, 1, (size_t)search_len, g_fp) != (size_t)search_len) {
        free(tail);
        obb_close();
        return -1;
    }

    long eocd_pos = -1;
    for (long i = search_len - 22; i >= 0; --i) {
        if (rd32(tail + i) == ZIP_SIG_EOCD) {
            eocd_pos = i;
            break;
        }
    }
    if (eocd_pos < 0) {
        debugPrintf("zip: EOCD not found in %s\n", path);
        free(tail);
        obb_close();
        return -1;
    }

    uint16_t total_entries = rd16(tail + eocd_pos + 10);
    uint32_t cd_size       = rd32(tail + eocd_pos + 12);
    uint32_t cd_offset     = rd32(tail + eocd_pos + 16);
    free(tail);

    uint8_t *cd = (uint8_t *)malloc(cd_size ? cd_size : 1);
    if (!cd) { obb_close(); return -1; }

    fseek(g_fp, (long)cd_offset, SEEK_SET);
    if (fread(cd, 1, cd_size, g_fp) != cd_size) {
        free(cd);
        obb_close();
        return -1;
    }

    g_entries = (ZipEntry *)calloc(total_entries ? total_entries : 1, sizeof(ZipEntry));
    if (!g_entries) {
        free(cd);
        obb_close();
        return -1;
    }

    size_t pos = 0;
    size_t valid = 0;
    for (uint16_t i = 0; i < total_entries && pos + 46 <= cd_size; ++i) {
        if (rd32(cd + pos) != ZIP_SIG_CENTRAL) break;
        uint16_t method    = rd16(cd + pos + 10);
        uint32_t comp_sz   = rd32(cd + pos + 20);
        uint32_t uncomp_sz = rd32(cd + pos + 24);
        uint16_t fn_len    = rd16(cd + pos + 28);
        uint16_t ex_len    = rd16(cd + pos + 30);
        uint16_t cm_len    = rd16(cd + pos + 32);
        uint32_t loc_off   = rd32(cd + pos + 42);

        if (pos + 46 + fn_len > cd_size) break;

        if (fn_len > 0 && cd[pos + 46 + fn_len - 1] != '/') {
            char *raw_name = (char *)malloc(fn_len + 1);
            if (raw_name) {
                memcpy(raw_name, cd + pos + 46, fn_len);
                raw_name[fn_len] = '\0';
                const char *norm = normalize_name(raw_name);
                g_entries[valid].name = strdup(norm);
                free(raw_name);
                if (g_entries[valid].name) {
                    g_entries[valid].method = method;
                    g_entries[valid].comp_size = comp_sz;
                    g_entries[valid].uncomp_size = uncomp_sz;
                    g_entries[valid].local_hdr_offset = loc_off;
                    valid++;
                }
            }
        }
        pos += 46 + fn_len + ex_len + cm_len;
    }
    free(cd);

    g_count = valid;
    if (g_count > 1) {
        qsort(g_entries, g_count, sizeof(ZipEntry), entry_cmp);
    }
    debugPrintf("zip: opened %s (%zu files)\n", path, g_count);
    return 0;
}

void obb_close(void) {
    if (g_entries) {
        for (size_t i = 0; i < g_count; ++i) {
            free(g_entries[i].name);
        }
        free(g_entries);
        g_entries = NULL;
    }
    g_count = 0;
    if (g_fp) {
        fclose(g_fp);
        g_fp = NULL;
    }
}

static const ZipEntry *find_entry(const char *name) {
    if (!g_entries || !g_count || !name) return NULL;
    ZipEntry key;
    key.name = (char *)normalize_name(name);
    return (const ZipEntry *)bsearch(&key, g_entries, g_count, sizeof(ZipEntry), entry_cmp);
}

int obb_exists(const char *name) {
    return find_entry(name) != NULL;
}

void *obb_read(const char *name, size_t *out_size) {
    if (out_size) *out_size = 0;
    const ZipEntry *e = find_entry(name);
    if (!e || !g_fp) return NULL;

    uint8_t lhdr[30];
    fseek(g_fp, (long)e->local_hdr_offset, SEEK_SET);
    if (fread(lhdr, 1, 30, g_fp) != 30 || rd32(lhdr) != ZIP_SIG_LOCAL) {
        return NULL;
    }
    uint16_t fn_len = rd16(lhdr + 26);
    uint16_t ex_len = rd16(lhdr + 28);
    long data_offset = (long)e->local_hdr_offset + 30 + fn_len + ex_len;

    fseek(g_fp, data_offset, SEEK_SET);

    if (e->method == 0) {
        /* 0-저장 (ZIP_STORED) */
        uint8_t *buf = (uint8_t *)malloc(e->uncomp_size + 1);
        if (!buf) return NULL;
        if (e->uncomp_size > 0 && fread(buf, 1, e->uncomp_size, g_fp) != e->uncomp_size) {
            free(buf);
            return NULL;
        }
        buf[e->uncomp_size] = '\0';
        if (out_size) *out_size = e->uncomp_size;
        return buf;
    } else if (e->method == 8) {
        /* Deflate (zlib raw inflate) */
        uint8_t *comp = (uint8_t *)malloc(e->comp_size ? e->comp_size : 1);
        uint8_t *buf  = (uint8_t *)malloc(e->uncomp_size + 1);
        if (!comp || !buf) {
            free(comp);
            free(buf);
            return NULL;
        }
        if (fread(comp, 1, e->comp_size, g_fp) != e->comp_size) {
            free(comp);
            free(buf);
            return NULL;
        }
        z_stream zs;
        memset(&zs, 0, sizeof(zs));
        zs.next_in = comp;
        zs.avail_in = e->comp_size;
        zs.next_out = buf;
        zs.avail_out = e->uncomp_size;

        if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
            free(comp);
            free(buf);
            return NULL;
        }
        int rc = inflate(&zs, Z_FINISH);
        inflateEnd(&zs);
        free(comp);
        if (rc != Z_STREAM_END) {
            free(buf);
            return NULL;
        }
        buf[e->uncomp_size] = '\0';
        if (out_size) *out_size = e->uncomp_size;
        return buf;
    }

    debugPrintf("zip: unsupported compression method %u for %s\n", e->method, e->name);
    return NULL;
}
