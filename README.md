# Kissing numbers

Preprints and improved kissing number constructions.

## Layout

Preprints are grouped by method. Each method folder holds a `paper.pdf` and one dimension folder per result. Each dimension folder has a `data/` folder with the points. The [`checker/`](checker) folder has one C++ program that verifies any of the data files exactly, pair by pair.

- [`preprints/hyperplane-section-d18`](preprints/hyperplane-section-d18): cut a 19-dimensional odd-sign configuration by the hyperplane $x_2 + x_{11} + x_{14} = 0$ and add 960 projected points. This ties the 8358 points of Kravatskiy's repository. In coordinates adapted to the hyperplane the result is a hexagon configuration: a 16-dimensional head with the Reed-Muller codes of length 16, a hexagon of six tail directions, a bent family of sextads for each direction and added points at the midpoint angles. The paper proves this shape works under three conditions on the forms, gives a second, non-isometric 8358-point configuration that fills the shape with six different bent forms, shows that the tail of each added point of the section can be rescaled within an interval, and ships a rebuild of Kravatskiy's configuration (the same shape a third way) for the comparison.
- [`preprints/odd-sign-d19-d21`](preprints/odd-sign-d19-d21): start from the Cohn-Li odd-sign configurations. In d19, cut the 20-dimensional configuration by the hyperplane orthogonal to a tetrad T and add one reshaped projected sign vector for each of the 1792 head patterns of the punctured Golay code with a nonzero tail; a choice of half in each of 112 cosets of the tetrad code makes them fit. In d21, keep every base point and add 3840 sign vectors of punctured Golay code words, weighted 2 on one line of the projective plane of order 4 and 3 elsewhere.
- [`preprints/leech-lift-d28-d31`](preprints/leech-lift-d28-d31): lift disjoint sets of 528 Leech minimal vectors (cosets of an octad-free Golay subcode plus a matching of an octad) with short tails in the extra coordinates; this alone beats every previous bound. In d28 the poles are replaced by cross-polytopes of layer points, proved by hand. In d29-d31 the poles are replaced by layer or hole points and by computed points with integer coordinates, checked exactly.

## Bounds

| Dimension | Best published | Our lower bound | Method | Folder |
|---:|---:|---:|---|---|
| 18 | 7654 (Cohn-Li) | 8358 (tie with Kravatskiy's repository) | Hyperplane section; six bent families on a hexagon | [d18](preprints/hyperplane-section-d18/d18) |
| 19 | 11948 (Ho) | 12460 | Odd-sign hyperplane section plus reshaped projected sign vectors | [d19](preprints/odd-sign-d19-d21/d19) |
| 21 | 29768 (Cohn-Li) | 31560 | Odd-sign base plus line-weighted Golay sign vectors | [d21](preprints/odd-sign-d19-d21/d21) |
| 28 | 204522 (Takhanov-Yun) | 205172 | Leech lift | [d28](preprints/leech-lift-d28-d31/d28) |
| 29 | 209497 (Takhanov-Yun) | 210495 | Leech lift and computed points | [d29](preprints/leech-lift-d28-d31/d29) |
| 30 | 220442 (Takhanov-Yun) | 222073 | Leech lift and computed points | [d30](preprints/leech-lift-d28-d31/d30) |
| 31 | 238354 (Takhanov-Yun) | 241079 | Leech lift and computed points | [d31](preprints/leech-lift-d28-d31/d31) |

## References

- Boon Suan Ho, [A new lower bound for the kissing number in 19 dimensions](https://arxiv.org/abs/2603.10425).
- Henry Cohn and Anqi Li, [Improved kissing numbers in seventeen through twenty-one dimensions](https://arxiv.org/abs/2411.04916).
- Rustem Takhanov and Stanislav Yun, [New lower bounds for kissing numbers in dimensions 25–31](https://arxiv.org/abs/2609.21591).
- Chengdong Ma et al., [Finding Kissing Numbers with Game-theoretic Reinforcement Learning](https://arxiv.org/abs/2511.13391).

## Other GitHub repositories

- [alexlegeartis/KissingNumbers](https://github.com/alexlegeartis/KissingNumbers)
- [k-nic/kissing_d25-31](https://github.com/k-nic/kissing_d25-31)
- [griffinwork40/D21-Kissing](https://github.com/griffinwork40/D21-Kissing)
