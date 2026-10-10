# Checker

`check_kissing.cpp` checks that a data file in this repository is a kissing configuration. It is one C++17 file with no dependencies beyond the standard library.

## Build and run

```sh
g++ -O2 -std=c++17 -pthread check_kissing.cpp -o check_kissing
./check_kissing ../preprints/odd-sign-d19-d21/d19/data/D19-12276.txt --threads 2
```

`--threads N` sets the number of worker threads (default 1). The program reads the format from the file, so the same binary handles all six files. Exit code 0 means PASS. Exit code 1 means a pair failed, and exit code 2 means the file could not be read or parsed.

## What it checks

1. The file parses, and the number of points and the dimension match the header.
2. Every point is nonzero. For the integer files it also checks that the first number on each line equals the squared norm of the rest.
3. Every pair of points x, y (all N(N-1)/2 pairs, no symmetry reduction) satisfies x.y <= |x||y|/2, that is, the angle between them is at least 60 degrees.

Two points with the same direction have cosine 1, so step 3 also proves that the points are distinct as directions. The program reports any such pair separately as a "same-direction pair".

## The exact test

For a pair x, y let s = x.y, a = |x|^2 and b = |y|^2. The pair is fine if s <= 0, and otherwise if 4s^2 - ab <= 0. Both signs are decided with integer arithmetic.

Integer files (D19, D21). The coordinates are integers with a = q_x < 2^62. The program computes s in 128-bit integers. By Cauchy-Schwarz s^2 <= ab < 2^124, so 4s^2 and ab both fit in 128 bits and the comparison is exact.

Leech-lift files (D28 to D31). Each coordinate is a + b*sqrt2 + c*sqrt3 + d*sqrt6 with integers a, b, c, d (|.| < 2^20). Then s, a, b and 4s^2 - ab lie in the same ring Z[sqrt2, sqrt3], computed exactly as integer 4-tuples. Rows whose coefficients are all at most 1000 use 16-bit storage with 32-bit sums, which cannot overflow at these sizes. The 16 D31 rows with larger coefficients (up to 2984) use 64-bit sums. The sign of an element z = P + Q*sqrt3, with P = a + b*sqrt2 and Q = c + d*sqrt2, is decided as follows.

- The sign of u + v*sqrt2: if u and v have the same sign (or one is 0), that is the sign. Otherwise compare u^2 with 2v^2; the larger term wins.
- The sign of z: find the signs of P and Q as above. If they agree, or one is 0, that is the sign. Otherwise compare P^2 with 3Q^2, and P^2 - 3Q^2 = (a^2 + 2b^2 - 3c^2 - 6d^2) + (2ab - 6cd)*sqrt2 is again of the form u + v*sqrt2.

The squares in the last step use 256-bit products, so the test is exact whenever all four coefficients are below 2^61 in absolute value. The program counts any pair outside this range and reports FAIL; no pair in the six files comes near it. A short self-test of the sign routine runs at start-up.

Floating point is used only for the reported max cosine, which is for information.

## Output

The program prints the file, the format, the dimension, the number of points, the number of pairs checked, the max cosine, the number of violations, the thread count, the time and `result: PASS` or `result: FAIL`. For a failure it also prints the first bad pair (0-based indices in file order).

## Expected results

Run with `--threads 2` on an Apple M2 Pro. Every file gives max cosine 0.500000000000 (to float precision) and 0 violations.

| File | Points | Pairs checked | Max cosine | Violations | Result | Time (2 threads) |
|---|---:|---:|---:|---:|---|---:|
| `odd-sign-d19-d21/d19/data/D19-12276.txt` | 12276 | 75343950 | 0.500000000000 | 0 | PASS | 0.4 s |
| `odd-sign-d19-d21/d21/data/D21-31272.txt` | 31272 | 488953356 | 0.500000000000 | 0 | PASS | 2.7 s |
| `leech-lift-d28-d31/d28/data/D28-205172.txt` | 205172 | 21047672206 | 0.500000000000 | 0 | PASS | 223 s |
| `leech-lift-d28-d31/d29/data/D29-210473.txt` | 210473 | 22149336628 | 0.500000000000 | 0 | PASS | 244.4 s |
| `leech-lift-d28-d31/d30/data/D30-222072.txt` | 222072 | 24657875556 | 0.500000000000 | 0 | PASS | 292 s |
| `leech-lift-d28-d31/d31/data/D31-241077.txt` | 241077 | 29058939426 | 0.500000000000 | 0 | PASS | 376 s |

The Leech-lift runs use under 1 GB of memory. The program prints tile progress to stderr.
