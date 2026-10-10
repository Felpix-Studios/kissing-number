# Kissing numbers

Preprints and improved kissing number constructions.

## Layout

Preprints are grouped by method. Each method folder holds one dimension folder per result, with `data/` (the points), and `paper.pdf`. The [`checker/`](checker) folder has one C++ program that verifies any of the data files exactly, pair by pair.

- [`preprints/odd-sign-d19-d21`](preprints/odd-sign-d19-d21): start from the Cohn-Li odd-sign configurations, delete a small block of points, add more new points than were deleted.
- [`preprints/leech-lift-d28-d31`](preprints/leech-lift-d28-d31): lift disjoint sets of 528 Leech minimal vectors with short tails in the extra coordinates, add poles, then add layer and hole points.

## Improved bounds

| Dimension | Best published | New lower bound | Method | Folder |
|---:|---:|---:|---|---|
| 19 | 11948 (Ho) | 12276 | Odd-sign delete and add | [d19](preprints/odd-sign-d19-d21/d19) |
| 21 | 29768 (Cohn-Li) | 31272 | Odd-sign delete and add | [d21](preprints/odd-sign-d19-d21/d21) |
| 28 | 204522 (Takhanov-Yun) | 205172 | Leech lift | [d28](preprints/leech-lift-d28-d31/d28) |
| 29 | 209497 (Takhanov-Yun) | 210473 | Leech lift | [d29](preprints/leech-lift-d28-d31/d29) |
| 30 | 220440 (Ma et al.) | 222072 | Leech lift | [d30](preprints/leech-lift-d28-d31/d30) |
| 31 | 238354 (Takhanov-Yun) | 241077 | Leech lift | [d31](preprints/leech-lift-d28-d31/d31) |
