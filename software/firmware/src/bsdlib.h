#ifndef BSDLIB_H
#define BSDLIB_H

/* Values for humanize_number(3)'s flags parameter. */
#define HN_DECIMAL      0x01
#define HN_NOSPACE      0x02
#define HN_B            0x04
#define HN_DIVISOR_1000 0x08
#define HN_IEC_PREFIXES 0x10

/* Values for humanize_number(3)'s scale parameter. */
#define HN_GETSCALE     0x10
#define HN_AUTOSCALE    0x20

int humanize_number(char *_buf, size_t _len, int32_t _number,
        const char *_suffix, int _scale, int _flags);

#endif /* BSDLIB_H */
