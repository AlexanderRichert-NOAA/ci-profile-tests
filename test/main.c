#include "compute.h"

#include <stdio.h>
#include <stdlib.h>

/*
 * Usage: dummy_app <N>
 *
 * Runs compute_sort(N) and compute_sum(N), then prints a one-line summary.
 * The program is intentionally compute-heavy so profilers have CPU time to
 * sample.  Exit code is 0 on success, 1 on bad arguments.
 */
int main(int argc, char *argv[])
{
    int  n;
    long s;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <N>\n", argv[0]);
        return 1;
    }

    n = atoi(argv[1]);
    if (n <= 0) {
        fprintf(stderr, "%s: N must be a positive integer (got '%s')\n",
                argv[0], argv[1]);
        return 1;
    }

    compute_sort(n);
    s = compute_sum((long)n);

    printf("sort_size=%d  sum=%ld\n", n, s);
    return 0;
}
