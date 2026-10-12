#include "types.h"
#include "log.h"
#include "error.h"



struct cpio_newc_header {
    char c_magic[6];
    char c_ino[8];
    char c_mode[8];
    char c_uid[8];
    char c_gid[8];
    char c_nlink[8];
    char c_mtime[8];
    char c_filesize[8];
    char c_devmajor[8];
    char c_devminor[8];
    char c_rdevmajor[8];
    char c_rdevminor[8];
    char c_namesize[8];
    char c_check[8];
} __attribute__((packed));

#define CPIO_HDR_SIZE 110
#define ALIGN4(x) (((x) + 3u) & ~3u)

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex8(const char *s, uint32_t *out) {
    uint32_t v = 0;
    for (int i = 0; i < 8; i++) {
        int d = hexval(s[i]);
        if (d < 0) return -1;
        v = (v << 4) | (uint32_t)d;
    }
    *out = v;
    return 0;
}

static int name_is_trailer(const char *name, uint32_t namesz) {
    static const char t[] = "TRAILER!!!";
    if (namesz != sizeof(t)) return 0;      // 10 tecken + nollbyte = 11
    for (size_t i = 0; i < sizeof(t); i++)
        if (name[i] != t[i]) return 0;
    return 1;
}

/* base = start av initrd (mod_start), size = mod_end - mod_start */
int cpio_debug(const void *base, size_t size) {
    const uint8_t *start = base;
    const uint8_t *end   = start + size;
    const uint8_t *p     = start;
    int count = 0;

    while (p + CPIO_HDR_SIZE <= end) {
        const struct cpio_newc_header *h = (const void *)p;

        if (h->c_magic[0] != '0' || h->c_magic[1] != '7' ||
            h->c_magic[2] != '0' || h->c_magic[3] != '7' ||
            h->c_magic[4] != '0' || (h->c_magic[5] != '1' && h->c_magic[5] != '2')) {
            log_msg("cpio: felaktig magic vid offset %d\n", (unsigned)(p - start));
            return -1;
        }

        uint32_t namesz, filesz, mode;
        if (hex8(h->c_namesize, &namesz) || hex8(h->c_filesize, &filesz) ||
            hex8(h->c_mode, &mode)) {
            log_msg("cpio: felaktigt hex-fält vid offset %d\n", (unsigned)(p - start));
            return -1;
        }

        const char *name = (const char *)(p + CPIO_HDR_SIZE);
        const uint8_t *data = p + ALIGN4(CPIO_HDR_SIZE + namesz);

        if (data > end || data + filesz > end) {
            log_msg("cpio: post går utanför arkivet\n");
            return -1;
        }

        if (name_is_trailer(name, namesz))
            break;

        const char *typ;
        switch (mode & 0170000) {
            case 0100000: typ = "fil";     break;
            case 0040000: typ = "katalog"; break;
            case 0120000: typ = "symlink"; break;
            default:      typ = "annat";   break;
        }

        log_msg("%s %d   %s\n", typ, filesz, name);
        count++;

        p = data + ALIGN4(filesz);
    }

    log_msg("cpio: %d poster\n", count);
    return count;
}