/* json.c — see json.h. */
#include "json.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================= sbuf ================= */

void sbuf_init(sbuf *b) { b->buf = NULL; b->len = 0; b->cap = 0; b->oom = 0; }
void sbuf_free(sbuf *b) { free(b->buf); b->buf = NULL; b->len = b->cap = 0; }

static void sbuf_grow(sbuf *b, size_t need)
{
    size_t nc;
    char *nb;
    if (b->oom) return;
    if (b->len + need + 1 <= b->cap) return;
    nc = b->cap ? b->cap : 64;
    while (nc < b->len + need + 1) nc *= 2;
    nb = (char *)realloc(b->buf, nc);
    if (!nb) { b->oom = 1; return; }
    b->buf = nb; b->cap = nc;
}

void sbuf_putn(sbuf *b, const void *data, size_t n)
{
    if (b->oom || n == 0) return;
    sbuf_grow(b, n);
    if (b->oom) return;
    memcpy(b->buf + b->len, data, n);
    b->len += n;
    b->buf[b->len] = '\0';
}
void sbuf_puts(sbuf *b, const char *s) { if (s) sbuf_putn(b, s, strlen(s)); }
void sbuf_putc(sbuf *b, char c) { sbuf_putn(b, &c, 1); }

void sbuf_printf(sbuf *b, const char *fmt, ...)
{
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    sbuf_puts(b, tmp);
}

char *sbuf_steal(sbuf *b)
{
    char *r;
    if (!b->buf) { r = xstrdup(""); }
    else r = b->buf;
    b->buf = NULL; b->len = b->cap = 0;
    return r;
}

char *xstrndup(const char *s, size_t n)
{
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}
char *xstrdup(const char *s) { return xstrndup(s, strlen(s)); }

/* ================= value basics ================= */

static jval *jv_alloc(jv_type t)
{
    jval *v = (jval *)calloc(1, sizeof(jval));
    if (v) v->type = t;
    return v;
}
jval *jv_new_null(void) { return jv_alloc(JV_NULL); }
jval *jv_new_bool(int b) { jval *v = jv_alloc(JV_BOOL); if (v) v->v.boolean = !!b; return v; }
jval *jv_new_int(int64_t i) { jval *v = jv_alloc(JV_INT); if (v) v->v.i = i; return v; }
jval *jv_new_uint(uint64_t u) { jval *v = jv_alloc(JV_UINT); if (v) v->v.u = u; return v; }
jval *jv_new_dbl(double d) { jval *v = jv_alloc(JV_DBL); if (v) v->v.d = d; return v; }
jval *jv_new_strn(const char *s, size_t len)
{
    jval *v = jv_alloc(JV_STR);
    if (!v) return NULL;
    v->v.str.s = xstrndup(s, len);
    v->v.str.len = len;
    return v;
}
jval *jv_new_str(const char *s) { return jv_new_strn(s, strlen(s)); }
jval *jv_new_arr(void) { return jv_alloc(JV_ARR); }
jval *jv_new_obj(void) { return jv_alloc(JV_OBJ); }

void jv_free(jval *v)
{
    size_t i;
    if (!v) return;
    switch (v->type) {
    case JV_BIGINT: free(v->v.bigint); break;
    case JV_STR: free(v->v.str.s); break;
    case JV_ARR:
        for (i = 0; i < v->v.arr.len; ++i) jv_free(v->v.arr.items[i]);
        free(v->v.arr.items);
        break;
    case JV_OBJ:
        for (i = 0; i < v->v.obj.len; ++i) {
            free(v->v.obj.keys[i]);
            jv_free(v->v.obj.vals[i]);
        }
        free(v->v.obj.keys); free(v->v.obj.klens); free(v->v.obj.vals);
        break;
    default: break;
    }
    free(v);
}

jval *jv_copy(const jval *v)
{
    jval *r;
    size_t i;
    if (!v) return NULL;
    switch (v->type) {
    case JV_NULL: return jv_new_null();
    case JV_BOOL: return jv_new_bool(v->v.boolean);
    case JV_INT: return jv_new_int(v->v.i);
    case JV_UINT: return jv_new_uint(v->v.u);
    case JV_DBL: return jv_new_dbl(v->v.d);
    case JV_BIGINT:
        r = jv_alloc(JV_BIGINT);
        if (r) r->v.bigint = xstrdup(v->v.bigint);
        return r;
    case JV_STR: return jv_new_strn(v->v.str.s, v->v.str.len);
    case JV_ARR:
        r = jv_new_arr();
        if (!r) return NULL;
        for (i = 0; i < v->v.arr.len; ++i) jv_arr_push(r, jv_copy(v->v.arr.items[i]));
        return r;
    case JV_OBJ:
        r = jv_new_obj();
        if (!r) return NULL;
        for (i = 0; i < v->v.obj.len; ++i)
            jv_obj_setn(r, v->v.obj.keys[i], v->v.obj.klens[i], jv_copy(v->v.obj.vals[i]));
        return r;
    }
    return NULL;
}

void jv_arr_push(jval *arr, jval *item)
{
    if (!arr || arr->type != JV_ARR || !item) { jv_free(item); return; }
    if (arr->v.arr.len == arr->v.arr.cap) {
        size_t nc = arr->v.arr.cap ? arr->v.arr.cap * 2 : 8;
        jval **ni = (jval **)realloc(arr->v.arr.items, nc * sizeof(jval *));
        if (!ni) { jv_free(item); return; }
        arr->v.arr.items = ni; arr->v.arr.cap = nc;
    }
    arr->v.arr.items[arr->v.arr.len++] = item;
}

/* Key identity follows Python dict semantics: code point order,
 * which for UTF-8 bytes is memcmp order with shorter-prefix-first. */
static int keycmp(const char *a, size_t alen, const char *b, size_t blen)
{
    size_t m = alen < blen ? alen : blen;
    int c = m ? memcmp(a, b, m) : 0;
    if (c) return c;
    if (alen < blen) return -1;
    if (alen > blen) return 1;
    return 0;
}

void jv_obj_setn(jval *obj, const char *key, size_t klen, jval *val)
{
    size_t i;
    if (!obj || obj->type != JV_OBJ || !val) { jv_free(val); return; }
    for (i = 0; i < obj->v.obj.len; ++i) {
        if (keycmp(obj->v.obj.keys[i], obj->v.obj.klens[i], key, klen) == 0) {
            jv_free(obj->v.obj.vals[i]);
            obj->v.obj.vals[i] = val;
            return;
        }
    }
    if (obj->v.obj.len == obj->v.obj.cap) {
        size_t nc = obj->v.obj.cap ? obj->v.obj.cap * 2 : 8;
        char **nk = (char **)realloc(obj->v.obj.keys, nc * sizeof(char *));
        size_t *nl = (size_t *)realloc(obj->v.obj.klens, nc * sizeof(size_t));
        jval **nv = (jval **)realloc(obj->v.obj.vals, nc * sizeof(jval *));
        if (!nk || !nl || !nv) { jv_free(val); return; }
        obj->v.obj.keys = nk; obj->v.obj.klens = nl; obj->v.obj.vals = nv;
        obj->v.obj.cap = nc;
    }
    obj->v.obj.keys[obj->v.obj.len] = xstrndup(key, klen);
    obj->v.obj.klens[obj->v.obj.len] = klen;
    obj->v.obj.vals[obj->v.obj.len] = val;
    obj->v.obj.len++;
}
void jv_obj_set(jval *obj, const char *key, jval *val)
{ jv_obj_setn(obj, key, strlen(key), val); }

jval *jv_obj_getn(const jval *obj, const char *key, size_t klen)
{
    size_t i;
    if (!obj || obj->type != JV_OBJ) return NULL;
    for (i = 0; i < obj->v.obj.len; ++i)
        if (keycmp(obj->v.obj.keys[i], obj->v.obj.klens[i], key, klen) == 0)
            return obj->v.obj.vals[i];
    return NULL;
}
jval *jv_obj_get(const jval *obj, const char *key)
{ return jv_obj_getn(obj, key, strlen(key)); }

int jv_obj_del(jval *obj, const char *key)
{
    size_t i, klen = strlen(key);
    if (!obj || obj->type != JV_OBJ) return 0;
    for (i = 0; i < obj->v.obj.len; ++i) {
        if (keycmp(obj->v.obj.keys[i], obj->v.obj.klens[i], key, klen) == 0) {
            free(obj->v.obj.keys[i]);
            jv_free(obj->v.obj.vals[i]);
            memmove(&obj->v.obj.keys[i], &obj->v.obj.keys[i + 1],
                    (obj->v.obj.len - i - 1) * sizeof(char *));
            memmove(&obj->v.obj.klens[i], &obj->v.obj.klens[i + 1],
                    (obj->v.obj.len - i - 1) * sizeof(size_t));
            memmove(&obj->v.obj.vals[i], &obj->v.obj.vals[i + 1],
                    (obj->v.obj.len - i - 1) * sizeof(jval *));
            obj->v.obj.len--;
            return 1;
        }
    }
    return 0;
}

/* ================= exact shortest float digits =================
 *
 * py_float_repr needs the shortest decimal that reads back as
 * exactly v under round-to-nearest-even; among the shortest, the
 * closest to v; an exact tie goes to an even last digit. That is
 * CPython's dtoa rule (json.dumps / repr spelling).
 *
 * The previous formatter took printf's correctly-rounded %.*e
 * candidate and its two decimal neighbours and compared distances
 * in long double. On near-ties — values whose exact expansion sits
 * almost exactly between two shortest candidates, e.g. ending
 * ...75 at the 18th digit — the true distance gap (~1e-289 for a
 * value near 1e-256) is far below the resolution of the long
 * double subtraction (~1e-275), so the choice was made by rounding
 * noise: 20 of the 200,942 corpus patterns came out one decimal
 * ulp too high (the x86-64 corpus run; the same code passed on
 * aarch64, where long double is 128-bit quad — luck, not margin).
 *
 * This implementation compares exactly, with integers only. A
 * finite double is a dyadic rational F * 2^E whose decimal
 * expansion terminates, and its round-trip interval is
 * [(2F-1)*2^(E-1), (2F+1)*2^(E-1)] — the lower midpoint halved
 * once more when F == 2^52 above the smallest binade, where the
 * spacing below is half the spacing above. Endpoints belong to
 * the interval iff F is even (a midpoint parses to the neighbour
 * with the even significand). All three decimals are computed
 * exactly with a small bignum: F * 5^-E for E < 0, F << E else.
 * The scan then walks prefixes of v's own digits: at each length
 * the two grid neighbours of v are tested against the interval;
 * the first length with a member wins, and closeness is decided
 * by v's exact remaining digits. No tables, no floating point.
 */

/* Bignum: little-endian 32-bit limbs. The worst values here are
 * (2F+1) * 5^1075 < 2^2551 and integers < 2^1026, so 96 limbs
 * (3072 bits) carries both with margin. */
#define BIG_LIMBS 96
typedef struct { uint32_t w[BIG_LIMBS]; int n; } bignum;

static void big_from_u64(bignum *b, uint64_t x)
{
    b->w[0] = (uint32_t)x;
    b->w[1] = (uint32_t)(x >> 32);
    b->n = (x >> 32) ? 2 : (x ? 1 : 0);
}

static void big_mul_u32(bignum *b, uint32_t m)
{
    uint64_t carry = 0;
    int i;
    for (i = 0; i < b->n; ++i) {
        uint64_t cur = (uint64_t)b->w[i] * m + carry;
        b->w[i] = (uint32_t)cur;
        carry = cur >> 32;
    }
    if (carry) b->w[b->n++] = (uint32_t)carry;
}

static void big_shl(bignum *b, int bits)
{
    int words = bits / 32, rem = bits % 32, i;
    if (b->n == 0 || bits == 0) return;
    for (i = b->n - 1; i >= 0; --i) b->w[i + words] = b->w[i];
    for (i = 0; i < words; ++i) b->w[i] = 0;
    b->n += words;
    if (rem) {
        uint32_t carry = 0;
        for (i = 0; i < b->n; ++i) {
            uint32_t nc = b->w[i] >> (32 - rem);
            b->w[i] = (b->w[i] << rem) | carry;
            carry = nc;
        }
        if (carry) b->w[b->n++] = carry;
    }
    while (b->n > 0 && b->w[b->n - 1] == 0) b->n--;
}

/* Decimal digits of *src, most significant first, NUL-terminated.
 * out must hold 800 chars (768 digits is the double worst case).
 * Returns the digit count. */
static int big_digits(const bignum *src, char *out)
{
    bignum t = *src;
    uint32_t groups[BIG_LIMBS];
    int ng = 0, len = 0;
    if (t.n == 0) { out[0] = '0'; out[1] = '\0'; return 1; }
    while (t.n > 0) {
        uint64_t rem = 0;
        int i;
        for (i = t.n - 1; i >= 0; --i) {
            uint64_t cur = (rem << 32) | t.w[i];
            t.w[i] = (uint32_t)(cur / 1000000000u);
            rem = cur % 1000000000u;
        }
        while (t.n > 0 && t.w[t.n - 1] == 0) t.n--;
        groups[ng++] = (uint32_t)rem;
    }
    {
        int i = ng - 1, tl = 0;
        char tmp[16];
        uint32_t g = groups[i];
        do { tmp[tl++] = (char)('0' + (g % 10)); g /= 10; } while (g);
        while (tl) out[len++] = tmp[--tl];
        for (i = ng - 2; i >= 0; --i) {
            uint32_t gg = groups[i];
            int k;
            for (k = 8; k >= 0; --k) {
                out[len + k] = (char)('0' + (gg % 10));
                gg /= 10;
            }
            len += 9;
        }
    }
    out[len] = '\0';
    return len;
}

/* Exact decimal expansion of M * 2^E (M > 0): digits into `digits`,
 * *decpt set so that value == 0.digits * 10^decpt. Trailing zeros
 * are stripped (same value); the first digit is never zero. */
static void exact_decimal(uint64_t M, int E, char *digits, int *decpt)
{
    bignum b;
    int len, q = 0;
    big_from_u64(&b, M);
    if (E >= 0) {
        big_shl(&b, E);
    } else {
        int k = -E;
        q = E;
        while (k >= 13) { big_mul_u32(&b, 1220703125u); k -= 13; } /* 5^13 */
        while (k-- > 0) big_mul_u32(&b, 5u);
    }
    len = big_digits(&b, digits);
    *decpt = len + q;   /* decpt counts from the leading digit:
                         * stripping trailing zeros must not move it */
    while (len > 1 && digits[len - 1] == '0') digits[--len] = '\0';
}

/* Compare 0.da * 10^pa with 0.db * 10^pb; first digits are nonzero
 * and digits past the end of either string read as zero. */
static int cmp_decimal(const char *da, int pa, const char *db, int pb)
{
    int la = (int)strlen(da), lb = (int)strlen(db), i = 0;
    if (pa != pb) return pa < pb ? -1 : 1;
    for (;;) {
        char ca = i < la ? da[i] : '0';
        char cb = i < lb ? db[i] : '0';
        if (ca != cb) return ca < cb ? -1 : 1;
        if (i >= la && i >= lb) return 0;
        ++i;
    }
}

/* ================= Python float repr ================= */

void py_float_repr(double v, char *buf, size_t bufsz)
{
    if (isnan(v)) { snprintf(buf, bufsz, "NaN"); return; }
    if (isinf(v)) { snprintf(buf, bufsz, "%sInfinity", signbit(v) ? "-" : ""); return; }
    if (v == 0.0) { snprintf(buf, bufsz, "%s0.0", signbit(v) ? "-" : ""); return; }
    {
        int neg = signbit(v) != 0;
        double a = fabs(v);
        char digits[24];
        int ndig = 0, decpt = 0;
        /* Shortest round-trip digits, computed exactly (see the
         * exact-shortest section above): decompose a = F * 2^E,
         * take the exact decimal expansions of a and of its
         * round-trip interval endpoints, and scan prefixes of a's
         * digits for the first length whose grid neighbour falls
         * inside the interval. A candidate is guaranteed by
         * length 17 (the correctly rounded 17-digit decimal always
         * round-trips, and it is one of the two grid neighbours). */
        {
            uint64_t bits, frac, F;
            int be, E, vlen, vdec, ldec, hdec, n, inclusive;
            char vd[800], ld[800], hd[800];
            memcpy(&bits, &a, 8);
            frac = bits & ((UINT64_C(1) << 52) - 1);
            be = (int)((bits >> 52) & 0x7FF);
            if (be == 0) { F = frac; E = -1074; }
            else { F = frac | (UINT64_C(1) << 52); E = be - 1075; }
            inclusive = (F % 2 == 0);
            exact_decimal(F, E, vd, &vdec);
            vlen = (int)strlen(vd);
            if (F == (UINT64_C(1) << 52) && be > 1)
                /* binade boundary: spacing below is half */
                exact_decimal(4 * F - 1, E - 2, ld, &ldec);
            else
                exact_decimal(2 * F - 1, E - 1, ld, &ldec);
            exact_decimal(2 * F + 1, E - 1, hd, &hdec);
            for (n = 1; n <= 17 && ndig == 0; ++n) {
                char dn[24], up[24];
                int ddec = vdec, udec = vdec, i, cl, ch, dn_ok, up_ok;
                for (i = 0; i < n; ++i) dn[i] = i < vlen ? vd[i] : '0';
                dn[n] = '\0';
                memcpy(up, dn, (size_t)n + 1);
                for (i = n - 1; i >= 0 && up[i] == '9'; --i) up[i] = '0';
                if (i < 0) { up[0] = '1'; udec++; } /* 999.. -> 100.. */
                else up[i]++;
                cl = cmp_decimal(dn, ddec, ld, ldec);
                dn_ok = cl > 0 || (cl == 0 && inclusive);
                ch = cmp_decimal(up, udec, hd, hdec);
                up_ok = ch < 0 || (ch == 0 && inclusive);
                if (!dn_ok && !up_ok) continue;
                if (dn_ok && up_ok) {
                    /* both neighbours round-trip: take the closer
                     * one, decided by the exact tail of a past
                     * digit n (digits past vlen are zero); an exact
                     * half tail is a tie and goes to the even last
                     * digit */
                    char tc = n < vlen ? vd[n] : '0';
                    int up_closer;
                    if (tc > '5') up_closer = 1;
                    else if (tc < '5') up_closer = 0;
                    else { /* tail starts with 5: past half unless
                            * the rest is exactly zero (a true tie) */
                        int tie = 1;
                        for (i = n + 1; i < vlen; ++i)
                            if (vd[i] != '0') { tie = 0; break; }
                        up_closer = tie ? ((dn[n - 1] - '0') % 2 != 0)
                                        : 1;
                    }
                    if (up_closer) {
                        memcpy(digits, up, (size_t)n + 1);
                        decpt = udec;
                    } else {
                        memcpy(digits, dn, (size_t)n + 1);
                        decpt = ddec;
                    }
                } else if (dn_ok) {
                    memcpy(digits, dn, (size_t)n + 1);
                    decpt = ddec;
                } else {
                    memcpy(digits, up, (size_t)n + 1);
                    decpt = udec;
                }
                ndig = n;
                /* same value without the trailing zeros, if any */
                while (ndig > 1 && digits[ndig - 1] == '0')
                    digits[--ndig] = '\0';
            }
            if (ndig == 0) { /* unreachable: length 17 always has a
                              * round-tripping neighbour */
                char tmp[64];
                const char *p;
                int cnt = 0, exp10;
                snprintf(tmp, sizeof tmp, "%.17e", a);
                for (p = tmp; *p && *p != 'e' && *p != 'E'; ++p)
                    if (isdigit((unsigned char)*p)) digits[cnt++] = *p;
                exp10 = atoi(p + 1);
                ndig = cnt; decpt = exp10 + 1;
                while (ndig > 1 && digits[ndig - 1] == '0') ndig--;
                digits[ndig] = '\0';
            }
        }
        {
            sbuf b; sbuf_init(&b);
            if (neg) sbuf_putc(&b, '-');
            if (decpt <= -4 || decpt >= 17) {
                /* scientific: d[.ddd]e±XX, exponent at least 2 digits */
                int e = decpt - 1, i;
                sbuf_putc(&b, digits[0]);
                if (ndig > 1) {
                    sbuf_putc(&b, '.');
                    for (i = 1; i < ndig; ++i) sbuf_putc(&b, digits[i]);
                }
                sbuf_printf(&b, "e%+03d", e);
            } else if (decpt <= 0) {
                int i;
                sbuf_puts(&b, "0.");
                for (i = 0; i < -decpt; ++i) sbuf_putc(&b, '0');
                for (i = 0; i < ndig; ++i) sbuf_putc(&b, digits[i]);
            } else if (decpt >= ndig) {
                int i;
                for (i = 0; i < ndig; ++i) sbuf_putc(&b, digits[i]);
                for (i = ndig; i < decpt; ++i) sbuf_putc(&b, '0');
                sbuf_puts(&b, ".0");
            } else {
                int i;
                for (i = 0; i < decpt; ++i) sbuf_putc(&b, digits[i]);
                sbuf_putc(&b, '.');
                for (i = decpt; i < ndig; ++i) sbuf_putc(&b, digits[i]);
            }
            snprintf(buf, bufsz, "%s", b.buf ? b.buf : "");
            sbuf_free(&b);
        }
    }
}

/* ================= parser ================= */

typedef struct {
    const char *s;
    size_t len, pos;
    int depth;
    const char *err;
} parser;

static void p_ws(parser *p)
{
    while (p->pos < p->len) {
        char c = p->s[p->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') p->pos++;
        else break;
    }
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void utf8_encode(sbuf *b, uint32_t cp)
{
    if (cp < 0x80) {
        sbuf_putc(b, (char)cp);
    } else if (cp < 0x800) {
        sbuf_putc(b, (char)(0xc0 | (cp >> 6)));
        sbuf_putc(b, (char)(0x80 | (cp & 0x3f)));
    } else if (cp < 0x10000) {
        sbuf_putc(b, (char)(0xe0 | (cp >> 12)));
        sbuf_putc(b, (char)(0x80 | ((cp >> 6) & 0x3f)));
        sbuf_putc(b, (char)(0x80 | (cp & 0x3f)));
    } else {
        sbuf_putc(b, (char)(0xf0 | (cp >> 18)));
        sbuf_putc(b, (char)(0x80 | ((cp >> 12) & 0x3f)));
        sbuf_putc(b, (char)(0x80 | ((cp >> 6) & 0x3f)));
        sbuf_putc(b, (char)(0x80 | (cp & 0x3f)));
    }
}

/* Parse a string starting at the opening quote. On success the
 * decoded bytes are appended to out (raw UTF-8). */
static int p_string_raw(parser *p, sbuf *out)
{
    /* p->s[p->pos] == '"' */
    p->pos++;
    while (p->pos < p->len) {
        unsigned char c = (unsigned char)p->s[p->pos];
        if (c == '"') { p->pos++; return 1; }
        if (c == '\\') {
            char e;
            p->pos++;
            if (p->pos >= p->len) break;
            e = p->s[p->pos++];
            switch (e) {
            case '"': sbuf_putc(out, '"'); break;
            case '\\': sbuf_putc(out, '\\'); break;
            case '/': sbuf_putc(out, '/'); break;
            case 'b': sbuf_putc(out, '\b'); break;
            case 'f': sbuf_putc(out, '\f'); break;
            case 'n': sbuf_putc(out, '\n'); break;
            case 'r': sbuf_putc(out, '\r'); break;
            case 't': sbuf_putc(out, '\t'); break;
            case 'u': {
                uint32_t cp = 0;
                int i;
                if (p->pos + 4 > p->len) { p->err = "bad \\u escape"; return 0; }
                for (i = 0; i < 4; ++i) {
                    int hv = hexval(p->s[p->pos++]);
                    if (hv < 0) { p->err = "bad \\u escape"; return 0; }
                    cp = (cp << 4) | (uint32_t)hv;
                }
                if (cp >= 0xd800 && cp <= 0xdbff) {
                    /* possible surrogate pair */
                    if (p->pos + 6 <= p->len && p->s[p->pos] == '\\' &&
                        p->s[p->pos + 1] == 'u') {
                        uint32_t lo = 0; int ok = 1;
                        for (i = 0; i < 4; ++i) {
                            int hv = hexval(p->s[p->pos + 2 + i]);
                            if (hv < 0) { ok = 0; break; }
                            lo = (lo << 4) | (uint32_t)hv;
                        }
                        if (ok && lo >= 0xdc00 && lo <= 0xdfff) {
                            p->pos += 6;
                            cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
                        }
                        /* else: lone surrogate — encode the code point
                         * itself (WTF-8). Python would carry a lone
                         * surrogate str and fail at .encode("utf-8")
                         * during hashing: a documented divergence. */
                    }
                }
                utf8_encode(out, cp);
                break;
            }
            default: p->err = "bad escape"; return 0;
            }
            continue;
        }
        if (c < 0x20) { p->err = "raw control char in string"; return 0; }
        sbuf_putc(out, (char)c);
        p->pos++;
    }
    p->err = "unterminated string";
    return 0;
}

static jval *p_value(parser *p);

static jval *p_number(parser *p)
{
    size_t start = p->pos;
    int is_float = 0;
    if (p->pos < p->len && p->s[p->pos] == '-') p->pos++;
    if (p->pos >= p->len || !isdigit((unsigned char)p->s[p->pos])) {
        p->err = "bad number"; return NULL;
    }
    if (p->s[p->pos] == '0') p->pos++;
    else while (p->pos < p->len && isdigit((unsigned char)p->s[p->pos])) p->pos++;
    if (p->pos < p->len && p->s[p->pos] == '.') {
        is_float = 1; p->pos++;
        if (p->pos >= p->len || !isdigit((unsigned char)p->s[p->pos])) {
            p->err = "bad number"; return NULL;
        }
        while (p->pos < p->len && isdigit((unsigned char)p->s[p->pos])) p->pos++;
    }
    if (p->pos < p->len && (p->s[p->pos] == 'e' || p->s[p->pos] == 'E')) {
        is_float = 1; p->pos++;
        if (p->pos < p->len && (p->s[p->pos] == '+' || p->s[p->pos] == '-')) p->pos++;
        if (p->pos >= p->len || !isdigit((unsigned char)p->s[p->pos])) {
            p->err = "bad number"; return NULL;
        }
        while (p->pos < p->len && isdigit((unsigned char)p->s[p->pos])) p->pos++;
    }
    {
        size_t n = p->pos - start;
        char *tok = xstrndup(p->s + start, n);
        jval *r = NULL;
        if (!tok) { p->err = "oom"; return NULL; }
        if (is_float) {
            r = jv_new_dbl(strtod(tok, NULL));
            free(tok);
        } else {
            /* Integer: exact int64, then uint64, else verbatim text
             * (Python arbitrary precision dumps the same digits). */
            const char *q = tok;
            int neg = 0;
            if (*q == '-') { neg = 1; q++; }
            {
                size_t nd = strlen(q);
                if (nd <= 19) {
                    uint64_t u = strtoull(q, NULL, 10);
                    if (!neg && u <= (uint64_t)INT64_MAX) r = jv_new_int((int64_t)u);
                    else if (!neg) r = jv_new_uint(u);
                    else if (u <= (uint64_t)INT64_MAX + 1u)
                        r = jv_new_int(u == (uint64_t)INT64_MAX + 1u ? INT64_MIN : -(int64_t)u);
                } else if (nd == 20 && !neg) {
                    char *end = NULL;
                    uint64_t u;
                    errno = 0;
                    u = strtoull(q, &end, 10);
                    /* strtoull saturates at UINT64_MAX on overflow;
                     * only ERANGE distinguishes a real UINT64_MAX. */
                    if (end && *end == '\0' && errno != ERANGE)
                        r = jv_new_uint(u);
                }
            }
            if (!r) {
                /* verify it really is all digits before keeping text */
                const char *q2 = tok + (neg ? 1 : 0);
                int alldig = *q2 != '\0';
                while (*q2) { if (!isdigit((unsigned char)*q2)) { alldig = 0; break; } q2++; }
                if (alldig) {
                    r = jv_alloc(JV_BIGINT);
                    if (r) r->v.bigint = tok; else free(tok);
                    return r;
                }
                free(tok);
                p->err = "bad number";
                return NULL;
            }
            free(tok);
        }
        return r;
    }
}

static jval *p_value(parser *p)
{
    char c;
    if (++p->depth > 512) { p->err = "nesting too deep"; p->depth--; return NULL; }
    p_ws(p);
    if (p->pos >= p->len) { p->err = "unexpected end"; p->depth--; return NULL; }
    c = p->s[p->pos];
    if (c == '{') {
        jval *o;
        p->pos++;
        o = jv_new_obj();
        if (!o) { p->err = "oom"; p->depth--; return NULL; }
        p_ws(p);
        if (p->pos < p->len && p->s[p->pos] == '}') { p->pos++; p->depth--; return o; }
        for (;;) {
            sbuf kb; jval *val;
            p_ws(p);
            if (p->pos >= p->len || p->s[p->pos] != '"') {
                p->err = "expected object key"; jv_free(o); p->depth--; return NULL;
            }
            sbuf_init(&kb);
            if (!p_string_raw(p, &kb)) { sbuf_free(&kb); jv_free(o); p->depth--; return NULL; }
            p_ws(p);
            if (p->pos >= p->len || p->s[p->pos] != ':') {
                p->err = "expected :"; sbuf_free(&kb); jv_free(o); p->depth--; return NULL;
            }
            p->pos++;
            val = p_value(p);
            if (!val) { sbuf_free(&kb); jv_free(o); p->depth--; return NULL; }
            jv_obj_setn(o, kb.buf ? kb.buf : "", kb.len, val);
            sbuf_free(&kb);
            p_ws(p);
            if (p->pos < p->len && p->s[p->pos] == ',') { p->pos++; continue; }
            if (p->pos < p->len && p->s[p->pos] == '}') { p->pos++; break; }
            p->err = "expected , or }"; jv_free(o); p->depth--; return NULL;
        }
        p->depth--;
        return o;
    }
    if (c == '[') {
        jval *a;
        p->pos++;
        a = jv_new_arr();
        if (!a) { p->err = "oom"; p->depth--; return NULL; }
        p_ws(p);
        if (p->pos < p->len && p->s[p->pos] == ']') { p->pos++; p->depth--; return a; }
        for (;;) {
            jval *item = p_value(p);
            if (!item) { jv_free(a); p->depth--; return NULL; }
            jv_arr_push(a, item);
            p_ws(p);
            if (p->pos < p->len && p->s[p->pos] == ',') { p->pos++; continue; }
            if (p->pos < p->len && p->s[p->pos] == ']') { p->pos++; break; }
            p->err = "expected , or ]"; jv_free(a); p->depth--; return NULL;
        }
        p->depth--;
        return a;
    }
    if (c == '"') {
        sbuf kb; jval *r;
        sbuf_init(&kb);
        if (!p_string_raw(p, &kb)) { sbuf_free(&kb); p->depth--; return NULL; }
        r = jv_new_strn(kb.buf ? kb.buf : "", kb.len);
        sbuf_free(&kb);
        p->depth--;
        return r;
    }
    if (c == 't' && p->pos + 4 <= p->len && memcmp(p->s + p->pos, "true", 4) == 0) {
        p->pos += 4; p->depth--; return jv_new_bool(1);
    }
    if (c == 'f' && p->pos + 5 <= p->len && memcmp(p->s + p->pos, "false", 5) == 0) {
        p->pos += 5; p->depth--; return jv_new_bool(0);
    }
    if (c == 'n' && p->pos + 4 <= p->len && memcmp(p->s + p->pos, "null", 4) == 0) {
        p->pos += 4; p->depth--; return jv_new_null();
    }
    /* Python json.loads accepts these non-standard constants. */
    if (c == 'N' && p->pos + 3 <= p->len && memcmp(p->s + p->pos, "NaN", 3) == 0) {
        p->pos += 3; p->depth--; return jv_new_dbl(NAN);
    }
    if (c == 'I' && p->pos + 8 <= p->len && memcmp(p->s + p->pos, "Infinity", 8) == 0) {
        p->pos += 8; p->depth--; return jv_new_dbl(INFINITY);
    }
    if (c == '-' && p->pos + 9 <= p->len && memcmp(p->s + p->pos, "-Infinity", 9) == 0) {
        p->pos += 9; p->depth--; return jv_new_dbl(-INFINITY);
    }
    if (c == '-' || isdigit((unsigned char)c)) {
        jval *r = p_number(p);
        p->depth--;
        return r;
    }
    p->err = "unexpected character";
    p->depth--;
    return NULL;
}

jval *jv_parse(const char *text, size_t len, const char **err)
{
    parser p;
    jval *v;
    p.s = text; p.len = len; p.pos = 0; p.depth = 0; p.err = NULL;
    v = p_value(&p);
    if (!v) { if (err) *err = p.err ? p.err : "parse error"; return NULL; }
    p_ws(&p);
    if (p.pos != p.len) {
        jv_free(v);
        if (err) *err = "trailing data";
        return NULL;
    }
    return v;
}

/* ================= canonical writer ================= */

static void write_string(const char *s, size_t len, sbuf *b)
{
    size_t i;
    sbuf_putc(b, '"');
    for (i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
        case '"': sbuf_puts(b, "\\\""); break;
        case '\\': sbuf_puts(b, "\\\\"); break;
        case '\b': sbuf_puts(b, "\\b"); break;
        case '\f': sbuf_puts(b, "\\f"); break;
        case '\n': sbuf_puts(b, "\\n"); break;
        case '\r': sbuf_puts(b, "\\r"); break;
        case '\t': sbuf_puts(b, "\\t"); break;
        default:
            if (c < 0x20) {
                char esc[8];
                snprintf(esc, sizeof esc, "\\u%04x", c);
                sbuf_puts(b, esc);
            } else {
                sbuf_putc(b, (char)c);   /* raw UTF-8 (v0.3 pin) */
            }
        }
    }
    sbuf_putc(b, '"');
}

/* Insertion sort of an index array by key (payload objects are
 * small; this avoids non-C99 qsort_r). */
static void sort_indices(const jval *o, size_t *idx)
{
    size_t i, j, n = o->v.obj.len;
    for (i = 0; i < n; ++i) idx[i] = i;
    for (i = 1; i < n; ++i) {
        size_t t = idx[i];
        j = i;
        while (j > 0 && keycmp(o->v.obj.keys[t], o->v.obj.klens[t],
                               o->v.obj.keys[idx[j - 1]],
                               o->v.obj.klens[idx[j - 1]]) < 0) {
            idx[j] = idx[j - 1];
            j--;
        }
        idx[j] = t;
    }
}

void jv_write_canonical(const jval *v, sbuf *b)
{
    size_t i;
    char numbuf[64];
    switch (v->type) {
    case JV_NULL: sbuf_puts(b, "null"); break;
    case JV_BOOL: sbuf_puts(b, v->v.boolean ? "true" : "false"); break;
    case JV_INT:
        snprintf(numbuf, sizeof numbuf, "%lld", (long long)v->v.i);
        sbuf_puts(b, numbuf);
        break;
    case JV_UINT:
        snprintf(numbuf, sizeof numbuf, "%llu", (unsigned long long)v->v.u);
        sbuf_puts(b, numbuf);
        break;
    case JV_BIGINT: sbuf_puts(b, v->v.bigint); break;
    case JV_DBL:
        py_float_repr(v->v.d, numbuf, sizeof numbuf);
        sbuf_puts(b, numbuf);
        break;
    case JV_STR: write_string(v->v.str.s, v->v.str.len, b); break;
    case JV_ARR:
        sbuf_putc(b, '[');
        for (i = 0; i < v->v.arr.len; ++i) {
            if (i) sbuf_putc(b, ',');
            jv_write_canonical(v->v.arr.items[i], b);
        }
        sbuf_putc(b, ']');
        break;
    case JV_OBJ: {
        size_t *idx = (size_t *)malloc((v->v.obj.len ? v->v.obj.len : 1) * sizeof(size_t));
        if (!idx) { b->oom = 1; break; }
        sort_indices(v, idx);
        sbuf_putc(b, '{');
        for (i = 0; i < v->v.obj.len; ++i) {
            size_t k = idx[i];
            if (i) sbuf_putc(b, ',');
            write_string(v->v.obj.keys[k], v->v.obj.klens[k], b);
            sbuf_putc(b, ':');
            jv_write_canonical(v->v.obj.vals[k], b);
        }
        sbuf_putc(b, '}');
        free(idx);
        break;
    }
    }
}

char *jv_canonical(const jval *v)
{
    sbuf b;
    sbuf_init(&b);
    jv_write_canonical(v, &b);
    return sbuf_steal(&b);
}

/* ================= Python str repr (for detail strings) ================= */

char *py_str_repr(const char *s, size_t len)
{
    /* Python repr: single quotes unless the string contains a single
     * quote and no double quote (then double quotes). Escapes \\, the
     * quote, \n \r \t, and non-printables as \xXX. Printable
     * non-ASCII is kept raw. Verdict details only carry identities
     * and ids; this covers the realistic cases byte-for-byte. */
    int has_sq = 0, has_dq = 0;
    size_t i;
    char q;
    sbuf b;
    for (i = 0; i < len; ++i) {
        if (s[i] == '\'') has_sq = 1;
        if (s[i] == '"') has_dq = 1;
    }
    q = (has_sq && !has_dq) ? '"' : '\'';
    sbuf_init(&b);
    sbuf_putc(&b, q);
    for (i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c == (unsigned char)q || c == '\\') {
            sbuf_putc(&b, '\\'); sbuf_putc(&b, (char)c);
        } else if (c == '\n') sbuf_puts(&b, "\\n");
        else if (c == '\r') sbuf_puts(&b, "\\r");
        else if (c == '\t') sbuf_puts(&b, "\\t");
        else if (c < 0x20 || c == 0x7f) sbuf_printf(&b, "\\x%02x", c);
        else sbuf_putc(&b, (char)c);
    }
    sbuf_putc(&b, q);
    return sbuf_steal(&b);
}

/* ================= equality / coercion ================= */

static int to_ld(const jval *v, long double *out)
{
    switch (v->type) {
    case JV_BOOL: *out = v->v.boolean ? 1.0L : 0.0L; return 1;
    case JV_INT: *out = (long double)v->v.i; return 1;
    case JV_UINT: *out = (long double)v->v.u; return 1;
    case JV_DBL: *out = (long double)v->v.d; return 1;
    case JV_BIGINT: *out = strtold(v->v.bigint, NULL); return 1;
    default: return 0;
    }
}

static int is_numeric(const jval *v)
{
    return v->type == JV_BOOL || v->type == JV_INT || v->type == JV_UINT ||
           v->type == JV_DBL || v->type == JV_BIGINT;
}

int jv_equal(const jval *a, const jval *b)
{
    size_t i;
    if (is_numeric(a) && is_numeric(b)) {
        long double x, y;
        /* Python: True == 1 == 1.0; bool vs bool also lands here.
         * NaN compares unequal to everything, including itself. */
        to_ld(a, &x); to_ld(b, &y);
        if (isnan(x) || isnan(y)) return 0;
        return x == y;
    }
    if (a->type != b->type) return 0;
    switch (a->type) {
    case JV_NULL: return 1;
    case JV_STR:
        return a->v.str.len == b->v.str.len &&
               (a->v.str.len == 0 || memcmp(a->v.str.s, b->v.str.s, a->v.str.len) == 0);
    case JV_ARR:
        if (a->v.arr.len != b->v.arr.len) return 0;
        for (i = 0; i < a->v.arr.len; ++i)
            if (!jv_equal(a->v.arr.items[i], b->v.arr.items[i])) return 0;
        return 1;
    case JV_OBJ:
        if (a->v.obj.len != b->v.obj.len) return 0;
        for (i = 0; i < a->v.obj.len; ++i) {
            jval *bv = jv_obj_getn(b, a->v.obj.keys[i], a->v.obj.klens[i]);
            if (!bv) return 0;
            if (!jv_equal(a->v.obj.vals[i], bv)) return 0;
        }
        return 1;
    default: return 0;
    }
}

double jv_as_double(const jval *v, int *ok)
{
    *ok = 1;
    switch (v->type) {
    case JV_BOOL: return v->v.boolean ? 1.0 : 0.0;
    case JV_INT: return (double)v->v.i;
    case JV_UINT: return (double)v->v.u;
    case JV_DBL: return v->v.d;
    case JV_BIGINT: return strtod(v->v.bigint, NULL);
    case JV_STR: {
        /* Python float(str): whole-string parse, whitespace tolerated */
        char *end = NULL;
        double d;
        if (v->v.str.len == 0) { *ok = 0; return 0.0; }
        d = strtod(v->v.str.s, &end);
        if (end == v->v.str.s) { *ok = 0; return 0.0; }
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
        if (*end != '\0') { *ok = 0; return 0.0; }
        return d;
    }
    default:
        *ok = 0;
        return 0.0;
    }
}

int jv_contains_nonfinite(const jval *v)
{
    size_t i;
    switch (v->type) {
    case JV_DBL: return !isfinite(v->v.d);
    case JV_ARR:
        for (i = 0; i < v->v.arr.len; ++i)
            if (jv_contains_nonfinite(v->v.arr.items[i])) return 1;
        return 0;
    case JV_OBJ:
        for (i = 0; i < v->v.obj.len; ++i)
            if (jv_contains_nonfinite(v->v.obj.vals[i])) return 1;
        return 0;
    default: return 0;
    }
}
