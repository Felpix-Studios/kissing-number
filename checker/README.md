# Checker

`check_kissing.cpp` checks that a data file in this repository is a kissing configuration. It is one C++17 file with no dependencies beyond the standard library.

## Build and run

```sh
g++ -O2 -std=c++17 -pthread check_kissing.cpp -o check_kissing
./check_kissing ../preprints/odd-sign-d19-d21/d19/data/D19-12460.txt --threads 4
```

`--threads N` sets the number of worker threads (default 1). The program reads the format from the file, so the same binary handles all nine files. Exit code 0 means PASS. Exit code 1 means a pair failed, and exit code 2 means the file could not be read or parsed.

## What it checks

1. The file parses, and the number of points and the dimension match the header.
2. Every point is nonzero. For the integer files it also checks that the first number on each line equals the squared norm of the rest.
3. Every pair of points x, y (all N(N-1)/2 pairs, no symmetry reduction) satisfies x.y <= |x||y|/2, that is, the angle between them is at least 60 degrees.

Two points with the same direction have cosine 1, so step 3 also proves that the points are distinct as directions. The program reports any such pair separately as a "same-direction pair".

## The exact test

For a pair x, y let s = x.y, a = |x|^2 and b = |y|^2. The pair is fine if s <= 0, and otherwise if 4s^2 - ab <= 0. Both signs are decided with integer arithmetic.

Integer files (D18 section, D21). The D18 section file uses 19 coordinates on the hyperplane x2 + x11 + x14 = 0, so the 19-dimensional inner products are the 18-dimensional ones. The coordinates are integers with a = q_x < 2^62. The program computes s in 128-bit integers. By Cauchy-Schwarz s^2 <= ab < 2^124, so 4s^2 and ab both fit in 128 bits and the comparison is exact.

Radical files (D18 six-family and D18 Kravatskiy rebuild, D19, D28 to D31). These files start with `dimension N` and `points M`. The Leech-lift files (D28 to D31) store the points compactly as tails, classes of Leech heads, poles and extra rows; the D19 and the two radical D18 files list every point as an `extra` row. Tail, class and pole lines need N >= 25; a file of `extra` rows may have any dimension up to 64. Each coordinate is a + b*sqrt2 + c*sqrt3 + d*sqrt6 with integers a, b, c, d (|.| < 2^20). In the D19 file only a and d occur, with |a| <= 1000 and |d| <= 1. Then s, a, b and 4s^2 - ab lie in the same ring Z[sqrt2, sqrt3], computed exactly as integer 4-tuples. Rows whose coefficients are all at most 1000 use 16-bit storage with 32-bit sums, which cannot overflow at these sizes. Rows with larger coefficients use 64-bit sums. These are the integer rows added in D29 to D31 (63, 97 and 167 rows, coordinates up to 30000). The sign of an element z = P + Q*sqrt3, with P = a + b*sqrt2 and Q = c + d*sqrt2, is decided as follows.

- The sign of u + v*sqrt2: if u and v have the same sign (or one is 0), that is the sign. Otherwise compare u^2 with 2v^2; the larger term wins.
- The sign of z: find the signs of P and Q as above. If they agree, or one is 0, that is the sign. Otherwise compare P^2 with 3Q^2, and P^2 - 3Q^2 = (a^2 + 2b^2 - 3c^2 - 6d^2) + (2ab - 6cd)*sqrt2 is again of the form u + v*sqrt2.

The squares in the last step use 256-bit products, so the test is exact whenever all four coefficients are below 2^61 in absolute value. If z is rational (b = c = d = 0), its sign is the sign of the 128-bit integer a, and no bound is needed. This covers pairs of two integer rows, where 4s^2 - ab can reach about 10^21. The program counts any other pair outside the range and reports FAIL; no pair in the six radical files has one. A short self-test of the sign routine runs at start-up.

Floating point is used only for the reported max cosine, which is for information.

## Output

The program prints the file, the format, the dimension, the number of points, the number of pairs checked, the max cosine, the number of violations, the thread count, the time and `result: PASS` or `result: FAIL`. For a failure it also prints the first bad pair (0-based indices in file order).

## Expected results

Run on an Apple M2 Pro with `--threads 4`. Every file gives max cosine 0.500000000000 (to float precision) and 0 violations.

| File | Points | Pairs checked | Max cosine | Violations | Result | Time |
|---|---:|---:|---:|---:|---|---:|
| `hyperplane-section-d18/d18/data/D18-8358.txt` | 8358 | 34923903 | 0.500000000000 | 0 | PASS | 0.1 s |
| `hyperplane-section-d18/d18/data/D18-8358-six-design.txt` | 8358 | 34923903 | 0.500000000000 | 0 | PASS | 1.1 s |
| `hyperplane-section-d18/d18/data/D18-8358-kravatskiy.txt` | 8358 | 34923903 | 0.500000000000 | 0 | PASS | 0.3 s |
| `odd-sign-d19-d21/d19/data/D19-12460.txt` | 12460 | 77619570 | 0.500000000000 | 0 | PASS | 0.5 s |
| `odd-sign-d19-d21/d21/data/D21-31560.txt` | 31560 | 498001020 | 0.500000000000 | 0 | PASS | 1.8 s |
| `leech-lift-d28-d31/d28/data/D28-205172.txt` | 205172 | 21047672206 | 0.500000000000 | 0 | PASS | 99 s |
| `leech-lift-d28-d31/d29/data/D29-210495.txt` | 210495 | 22153967265 | 0.500000000000 | 0 | PASS | 110 s |
| `leech-lift-d28-d31/d30/data/D30-222073.txt` | 222073 | 24658097628 | 0.500000000000 | 0 | PASS | 133 s |
| `leech-lift-d28-d31/d31/data/D31-241079.txt` | 241079 | 29059421581 | 0.500000000000 | 0 | PASS | 174 s |

The Leech-lift runs use under 1 GB of memory. The program prints tile progress to stderr.
