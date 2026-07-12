/* Behavioral known-answer test suite for libsais.
 *
 * Upstream ships no test suite, so this is the functional oracle: every result
 * produced by libsais is checked against an independent brute-force reference
 * implementation (O(n^2 log n) suffix sort / LCP), plus differential checks
 * (32-bit vs 64-bit API agreement, BWT -> unBWT round-trip).  Each assertion
 * checks concrete output values, so a no-op / exit(0) sabotage of the library
 * cannot pass.
 *
 * Prints one line per test and a final "RESULTS: total=<N> passed=<P> failed=<F>"
 * summary that mayhem/test.sh parses.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libsais.h"
#include "libsais16.h"
#include "libsais64.h"

static int g_total = 0, g_passed = 0, g_failed = 0;

static void report(const char *name, int ok)
{
    g_total++;
    if (ok) { g_passed++; printf("PASS %s\n", name); }
    else    { g_failed++; printf("FAIL %s\n", name); }
}

/* ---- independent brute-force reference ---- */

static const uint8_t  *g_T8;
static const uint16_t *g_T16;
static int32_t g_n;

static int cmp_suffix8(const void *a, const void *b)
{
    int32_t i = *(const int32_t *)a, j = *(const int32_t *)b;
    while (i < g_n && j < g_n)
    {
        if (g_T8[i] != g_T8[j]) { return g_T8[i] < g_T8[j] ? -1 : 1; }
        i++; j++;
    }
    if (i == g_n && j == g_n) { return 0; }
    return i == g_n ? -1 : 1; /* shorter suffix sorts first */
}

static int cmp_suffix16(const void *a, const void *b)
{
    int32_t i = *(const int32_t *)a, j = *(const int32_t *)b;
    while (i < g_n && j < g_n)
    {
        if (g_T16[i] != g_T16[j]) { return g_T16[i] < g_T16[j] ? -1 : 1; }
        i++; j++;
    }
    if (i == g_n && j == g_n) { return 0; }
    return i == g_n ? -1 : 1;
}

static void ref_sa8(const uint8_t *T, int32_t *SA, int32_t n)
{
    for (int32_t i = 0; i < n; i++) { SA[i] = i; }
    g_T8 = T; g_n = n;
    qsort(SA, (size_t)n, sizeof(int32_t), cmp_suffix8);
}

static void ref_sa16(const uint16_t *T, int32_t *SA, int32_t n)
{
    for (int32_t i = 0; i < n; i++) { SA[i] = i; }
    g_T16 = T; g_n = n;
    qsort(SA, (size_t)n, sizeof(int32_t), cmp_suffix16);
}

static int32_t ref_lcp_pair(const uint8_t *T, int32_t n, int32_t a, int32_t b)
{
    int32_t l = 0;
    while (a + l < n && b + l < n && T[a + l] == T[b + l]) { l++; }
    return l;
}

/* deterministic LCG so the suite is reproducible */
static uint32_t g_rng = 0x12345678u;
static uint32_t rnd(void) { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }

/* ---- per-input test battery ---- */

static void test_input(const char *name, const uint8_t *T, int32_t n)
{
    char label[256];
    int32_t *SA   = malloc(((size_t)n + 1) * sizeof(int32_t));
    int32_t *REF  = malloc(((size_t)n + 1) * sizeof(int32_t));
    int32_t *PLCP = malloc(((size_t)n + 1) * sizeof(int32_t));
    int32_t *LCP  = malloc(((size_t)n + 1) * sizeof(int32_t));
    int32_t *A    = malloc(((size_t)n + 1) * sizeof(int32_t));
    int64_t *SA64 = malloc(((size_t)n + 1) * sizeof(int64_t));
    uint8_t *U    = malloc((size_t)n + 1);
    uint8_t *V    = malloc((size_t)n + 1);
    if (!SA || !REF || !PLCP || !LCP || !A || !SA64 || !U || !V)
    {
        fprintf(stderr, "OOM in test %s\n", name);
        exit(2);
    }

    ref_sa8(T, REF, n);

    /* 1. libsais suffix array == brute-force reference */
    int ok = (libsais(T, SA, n, 0, NULL) == 0) && (memcmp(SA, REF, (size_t)n * sizeof(int32_t)) == 0);
    snprintf(label, sizeof(label), "sa32/%s", name);
    report(label, ok);

    /* 2. libsais64 agrees with the reference too */
    int ok64 = (libsais64(T, SA64, n, 0, NULL) == 0);
    if (ok64)
    {
        for (int32_t i = 0; i < n; i++)
        {
            if (SA64[i] != (int64_t)REF[i]) { ok64 = 0; break; }
        }
    }
    snprintf(label, sizeof(label), "sa64/%s", name);
    report(label, ok64);

    /* 3. PLCP + LCP against brute-force adjacent-suffix LCP */
    int oklcp = ok && (libsais_plcp(T, SA, PLCP, n) == 0) && (libsais_lcp(PLCP, SA, LCP, n) == 0);
    if (oklcp)
    {
        if (LCP[0] != 0) { oklcp = 0; }
        for (int32_t i = 1; i < n && oklcp; i++)
        {
            if (LCP[i] != ref_lcp_pair(T, n, REF[i - 1], REF[i])) { oklcp = 0; }
        }
    }
    snprintf(label, sizeof(label), "plcp+lcp/%s", name);
    report(label, oklcp);

    /* 4. BWT -> unBWT round-trip restores the input, and the BWT output is a
     *    permutation of the input bytes (value-level checks, not exit codes) */
    int okbwt = 0;
    int32_t idx = libsais_bwt(T, U, A, n, 0, NULL);
    if (idx > 0 && libsais_unbwt(U, V, A, n, NULL, idx) == 0)
    {
        okbwt = (memcmp(T, V, (size_t)n) == 0);
        if (okbwt)
        {
            size_t hist_t[256] = { 0 }, hist_u[256] = { 0 };
            for (int32_t i = 0; i < n; i++) { hist_t[T[i]]++; hist_u[U[i]]++; }
            okbwt = (memcmp(hist_t, hist_u, sizeof(hist_t)) == 0);
        }
    }
    snprintf(label, sizeof(label), "bwt-roundtrip/%s", name);
    report(label, okbwt);

    /* 5. libsais_int over the same data widened to int32 == reference */
    int32_t *TI = malloc((size_t)n * sizeof(int32_t));
    int okint = (TI != NULL);
    if (okint)
    {
        for (int32_t i = 0; i < n; i++) { TI[i] = T[i]; }
        okint = (libsais_int(TI, SA, n, 256, 0) == 0) && (memcmp(SA, REF, (size_t)n * sizeof(int32_t)) == 0);
    }
    snprintf(label, sizeof(label), "sa-int/%s", name);
    report(label, okint);
    free(TI);

    /* 6. libsais16 over the same data widened to uint16 == 16-bit reference */
    uint16_t *T16 = malloc((size_t)n * sizeof(uint16_t));
    int ok16 = (T16 != NULL);
    if (ok16)
    {
        for (int32_t i = 0; i < n; i++) { T16[i] = (uint16_t)(T[i] * 3u); } /* spread the alphabet */
        ref_sa16(T16, REF, n);
        ok16 = (libsais16(T16, SA, n, 0, NULL) == 0) && (memcmp(SA, REF, (size_t)n * sizeof(int32_t)) == 0);
    }
    snprintf(label, sizeof(label), "sa16/%s", name);
    report(label, ok16);
    free(T16);

    free(SA); free(REF); free(PLCP); free(LCP); free(A); free(SA64); free(U); free(V);
}

int main(void)
{
    /* fixed known strings */
    static const char *fixed[] = {
        "banana", "mississippi", "abracadabra", "yabbadabbadoo",
        "aaaaaaaaaaaaaaaa", "abcabcabcabcabcabc", "a",
        "the quick brown fox jumps over the lazy dog",
    };
    for (size_t i = 0; i < sizeof(fixed) / sizeof(fixed[0]); i++)
    {
        char name[64];
        snprintf(name, sizeof(name), "fixed-%zu", i);
        test_input(name, (const uint8_t *)fixed[i], (int32_t)strlen(fixed[i]));
    }

    /* hand-checked known answer: suffix array of "banana" is [5,3,1,0,4,2] */
    {
        static const int32_t expect[6] = { 5, 3, 1, 0, 4, 2 };
        int32_t sa[6];
        int ok = (libsais((const uint8_t *)"banana", sa, 6, 0, NULL) == 0)
              && (memcmp(sa, expect, sizeof(expect)) == 0);
        report("known-answer/banana", ok);
    }

    /* deterministic pseudo-random inputs over several alphabet sizes/lengths */
    static const int32_t lens[]  = { 64, 257, 1024, 2048 };
    static const uint32_t alphas[] = { 2, 4, 26, 256 };
    for (size_t li = 0; li < sizeof(lens) / sizeof(lens[0]); li++)
    {
        for (size_t ai = 0; ai < sizeof(alphas) / sizeof(alphas[0]); ai++)
        {
            int32_t n = lens[li];
            uint8_t *T = malloc((size_t)n);
            if (!T) { fprintf(stderr, "OOM\n"); return 2; }
            for (int32_t i = 0; i < n; i++) { T[i] = (uint8_t)(rnd() % alphas[ai]); }
            char name[64];
            snprintf(name, sizeof(name), "rnd-n%d-a%u", n, alphas[ai]);
            test_input(name, T, n);
            free(T);
        }
    }

    printf("RESULTS: total=%d passed=%d failed=%d\n", g_total, g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
