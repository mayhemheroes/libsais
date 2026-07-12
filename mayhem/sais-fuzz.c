/* Fuzz harness for libsais: suffix array construction, PLCP/LCP derivation,
 * and a BWT -> unBWT round-trip differential check over the same input. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "libsais.h"

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    if (Size == 0 || Size > (size_t)1 << 20)
    {
        return 0;
    }

    int32_t n = (int32_t)Size;
    int32_t *SA   = (int32_t *)malloc(((size_t)n + 1) * sizeof(int32_t));
    int32_t *PLCP = (int32_t *)malloc((size_t)n * sizeof(int32_t));
    int32_t *LCP  = (int32_t *)malloc((size_t)n * sizeof(int32_t));
    int32_t *A    = (int32_t *)malloc(((size_t)n + 1) * sizeof(int32_t));
    uint8_t *U    = (uint8_t *)malloc((size_t)n);
    uint8_t *V    = (uint8_t *)malloc((size_t)n);

    if (SA != NULL && PLCP != NULL && LCP != NULL && A != NULL && U != NULL && V != NULL)
    {
        if (libsais(Data, SA, n, 0, NULL) == 0)
        {
            if (libsais_plcp(Data, SA, PLCP, n) == 0)
            {
                libsais_lcp(PLCP, SA, LCP, n);
            }
        }

        int32_t i = libsais_bwt(Data, U, A, n, 0, NULL);
        if (i > 0)
        {
            if (libsais_unbwt(U, V, A, n, NULL, i) == 0 && memcmp(Data, V, (size_t)n) != 0)
            {
                abort(); /* BWT round-trip mismatch: real library defect */
            }
        }
    }

    free(SA); free(PLCP); free(LCP); free(A); free(U); free(V);
    return 0;
}
