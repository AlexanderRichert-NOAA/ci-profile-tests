#ifndef COMPUTE_H
#define COMPUTE_H

/*
 * compute_sort: allocate an array of length n, fill it with pseudo-random
 * values, and sort it with bubble sort.  The intentionally O(n²) sort gives
 * profilers enough CPU time to collect meaningful samples.
 */
void compute_sort(int n);

/*
 * compute_sum: return 1 + 2 + ... + n.
 */
long compute_sum(long n);

#endif /* COMPUTE_H */
