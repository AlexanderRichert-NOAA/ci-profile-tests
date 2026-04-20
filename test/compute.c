#include "compute.h"

#include <stdlib.h>

/*
 * fill_array: populate arr[0..n-1] with values from a linear congruential
 * generator seeded with n.  Using a deterministic seed keeps results
 * reproducible across runs while still producing an unsorted sequence.
 */
static void fill_array(int *arr, int n)
{
    unsigned long state = (unsigned long)n;
    int i;
    for (i = 0; i < n; i++) {
        /* Knuth multiplicative hash, 64-bit LCG coefficients. */
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        arr[i] = (int)(state >> 33);
    }
}

/*
 * bubble_sort: O(n²) comparison sort.  The quadratic complexity is
 * intentional — it keeps the CPU busy long enough for sampling profilers
 * (VTune, HPCToolkit) to collect statistically useful data even for
 * moderate values of n.
 */
static void bubble_sort(int *arr, int n)
{
    int i, j;
    for (i = 0; i < n - 1; i++) {
        for (j = 0; j < n - 1 - i; j++) {
            if (arr[j] > arr[j + 1]) {
                int tmp    = arr[j];
                arr[j]     = arr[j + 1];
                arr[j + 1] = tmp;
            }
        }
    }
}

void compute_sort(int n)
{
    int *arr = malloc((size_t)n * sizeof(int));
    if (!arr)
        return;
    fill_array(arr, n);
    bubble_sort(arr, n);
    free(arr);
}

long compute_sum(long n)
{
    long s = 0;
    long i;
    for (i = 1; i <= n; i++)
        s += i;
    return s;
}
