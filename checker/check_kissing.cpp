// check_kissing.cpp: exact all-pairs checker for kissing configurations.
//
// Compile: g++ -O2 -std=c++17 -pthread check_kissing.cpp -o check_kissing
// Run:     ./check_kissing path/to/file.txt [--threads N]
//
// Two input formats, detected from the file:
//   integer:  lines "q x_1 ... x_n" (point x/sqrt(q), q = x.x), header comment
//             "... in dimension N: M points".
//   radical:  "dimension N", "points M", then tail/class/pole/extra lines with
//             coordinates in Z[sqrt2, sqrt3] (see the data file headers).
//             Tail, class and pole lines need N >= 25; a file with only extra
//             lines may have any dimension N <= 64.
//
// For every pair of points x != y it decides x.y <= |x||y|/2 exactly:
// either x.y <= 0, or 4(x.y)^2 - |x|^2|y|^2 <= 0. All decisions use integer
// arithmetic. Floating point is used only for the reported max cosine.
// Exit code 0 means PASS, anything else means FAIL or an input error.

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using i128 = __int128;
using u128 = unsigned __int128;

static void die(const std::string& msg) {
    std::fprintf(stderr, "input error: %s\nresult: FAIL\n", msg.c_str());
    std::printf("result: FAIL (input error: %s)\n", msg.c_str());
    std::exit(2);
}

// ---------------------------------------------------------------------------
// Exact sign of a + b*sqrt2 + c*sqrt3 + d*sqrt6.

// Compare x^2 with 2*y^2 for x, y < 2^127, using 256-bit products.
struct U256 { u128 hi, lo; };

static U256 square(u128 x) {
    const uint64_t x0 = (uint64_t)x, x1 = (uint64_t)(x >> 64);
    const u128 p00 = (u128)x0 * x0, p01 = (u128)x0 * x1, p11 = (u128)x1 * x1;
    U256 r{p11, p00};
    const u128 add_lo = p01 << 65, add_hi = p01 >> 63;  // 2*p01*2^64
    r.lo += add_lo;
    r.hi += add_hi + (r.lo < add_lo ? 1 : 0);
    return r;
}

static int cmp_sq_2sq(u128 x, u128 y) {  // sign of x^2 - 2y^2
    const U256 a = square(x);
    U256 b = square(y);
    b.hi = (b.hi << 1) | (b.lo >> 127);
    b.lo <<= 1;
    if (a.hi != b.hi) return a.hi > b.hi ? 1 : -1;
    if (a.lo != b.lo) return a.lo > b.lo ? 1 : -1;
    return 0;
}

static u128 uabs(i128 x) { return x < 0 ? (u128)0 - (u128)x : (u128)x; }

// Sign of u + v*sqrt2 for any 128-bit u, v.
static int sign_q2(i128 u, i128 v) {
    const int su = (u > 0) - (u < 0), sv = (v > 0) - (v < 0);
    if (sv == 0) return su;
    if (su == 0 || su == sv) return sv;
    // Opposite signs: the term with the larger square wins.
    const int c = cmp_sq_2sq(uabs(u), uabs(v));  // sign of u^2 - 2v^2
    return c == 0 ? 0 : (c > 0 ? su : sv);
}

static const i128 COEF_LIMIT = (i128)1 << 61;  // keeps u, v below in 2^127

// Sign of a + b*sqrt2 + c*sqrt3 + d*sqrt6, |coefficients| < 2^61.
// Write it as P + Q*sqrt3 with P = a + b*sqrt2, Q = c + d*sqrt2.
static int sign_quad(i128 a, i128 b, i128 c, i128 d) {
    const int sP = sign_q2(a, b), sQ = sign_q2(c, d);
    if (sQ == 0) return sP;
    if (sP == 0 || sP == sQ) return sQ;
    // Opposite signs: compare P^2 with 3Q^2. P^2 - 3Q^2 = u + v*sqrt2.
    const i128 u = a * a + 2 * b * b - 3 * c * c - 6 * d * d;
    const i128 v = 2 * a * b - 6 * c * d;
    const int sDiff = sign_q2(u, v);
    return sP > 0 ? sDiff : -sDiff;
}

struct Quad { i128 a = 0, b = 0, c = 0, d = 0; };

static Quad mul(const Quad& x, const Quad& y) {
    return {x.a * y.a + 2 * x.b * y.b + 3 * x.c * y.c + 6 * x.d * y.d,
            x.a * y.b + x.b * y.a + 3 * x.c * y.d + 3 * x.d * y.c,
            x.a * y.c + x.c * y.a + 2 * x.b * y.d + 2 * x.d * y.b,
            x.a * y.d + x.d * y.a + x.b * y.c + x.c * y.b};
}

// A rational element (b = c = d = 0) has the sign of a for any 128-bit a, so
// only elements with an irrational part need the 2^61 bound.
static bool in_range(const Quad& x) {
    if (x.b == 0 && x.c == 0 && x.d == 0) return true;
    auto ok = [](i128 t) { return t < COEF_LIMIT && t > -COEF_LIMIT; };
    return ok(x.a) && ok(x.b) && ok(x.c) && ok(x.d);
}

static int sign(const Quad& x) { return sign_quad(x.a, x.b, x.c, x.d); }

static double to_double(const Quad& x) {
    return (double)x.a + (double)x.b * std::sqrt(2.0) + (double)x.c * std::sqrt(3.0) +
           (double)x.d * std::sqrt(6.0);
}

// ---------------------------------------------------------------------------
// Pair verdicts and per-thread tallies.

enum Verdict { OK = 0, VIOLATION = 1, SAME_DIRECTION = 2, OUT_OF_RANGE = 3 };

struct Tally {
    i64 pairs = 0, violations = 0, same_dir = 0, out_of_range = 0;
    double max_cos = -2;
    i64 bad_i = -1, bad_j = -1;
    void add(int v, i64 i, i64 j) {
        if (v == OK) return;
        if (v == OUT_OF_RANGE) ++out_of_range;
        else ++violations;
        if (v == SAME_DIRECTION) ++same_dir;
        if (bad_i < 0) { bad_i = i; bad_j = j; }
    }
    void merge(const Tally& o) {
        pairs += o.pairs; violations += o.violations; same_dir += o.same_dir;
        out_of_range += o.out_of_range;
        max_cos = std::max(max_cos, o.max_cos);
        if (bad_i < 0) { bad_i = o.bad_i; bad_j = o.bad_j; }
    }
};

// Run check(i, j) on every pair i < j < n with tiled loops, over nthreads threads.
template <class F>
static Tally all_pairs(i64 n, int nthreads, F check) {
    const i64 TI = 64, TJ = 512;
    const i64 ntiles = (n + TI - 1) / TI;
    std::atomic<i64> next{0}, done{0};
    std::vector<Tally> tallies(nthreads);
    const auto t0 = std::chrono::steady_clock::now();
    auto worker = [&](int tid) {
        Tally t;
        for (;;) {
            const i64 tile = next.fetch_add(1);
            if (tile >= ntiles) break;
            const i64 i0 = tile * TI, i1 = std::min(n, i0 + TI);
            for (i64 j0 = i0 + 1; j0 < n; j0 += TJ) {
                const i64 j1 = std::min(n, j0 + TJ);
                for (i64 i = i0; i < i1; ++i)
                    for (i64 j = std::max(j0, i + 1); j < j1; ++j) check(i, j, t);
            }
            const i64 d = done.fetch_add(1) + 1;
            if (tid == 0 && d % 256 == 0) {
                const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
                std::fprintf(stderr, "  tiles %lld/%lld, %.0f s\n", (long long)d, (long long)ntiles, s);
            }
        }
        tallies[tid] = t;
    };
    std::vector<std::thread> pool;
    for (int t = 0; t < nthreads; ++t) pool.emplace_back(worker, t);
    for (auto& th : pool) th.join();
    Tally total;
    for (auto& t : tallies) total.merge(t);
    return total;
}

// ---------------------------------------------------------------------------
// Parsing helpers.

static bool parse_i64(const char*& p, i64& out) {
    errno = 0;
    char* end = nullptr;
    const long long v = std::strtoll(p, &end, 10);
    if (end == p || errno == ERANGE) return false;
    out = v;
    p = end;
    return true;
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// Coefficients of Z[sqrt2, sqrt3] coordinates. Rows with all |coefficients|
// <= SMALL_COEF use a fast int16/int32 path; larger rows use int64.
static const i64 MAX_COEF = (i64)1 << 20, SMALL_COEF = 1000;

// Parse "m", "m*sqrt(r)", "sqrt(r)", "-sqrt(r)" terms joined by + or -.
static bool parse_quad(const std::string& field, i64 q[4]) {
    q[0] = q[1] = q[2] = q[3] = 0;
    const std::string s = trim(field);
    size_t p = 0;
    bool any = false;
    while (p < s.size()) {
        int sg = 1;
        if (s[p] == '+' || s[p] == '-') {
            if (s[p] == '-') sg = -1;
            ++p;
        } else if (any) {
            return false;
        }
        i64 m = 1;
        bool has_int = false;
        if (p < s.size() && isdigit((unsigned char)s[p])) {
            const char* b = s.c_str() + p;
            const char* e = b;
            if (!parse_i64(e, m)) return false;
            p += e - b;
            has_int = true;
        }
        if (s.compare(p, 1, "*") == 0) {
            if (!has_int) return false;
            ++p;
            if (s.compare(p, 5, "sqrt(") != 0) return false;
        }
        int slot = 0;
        if (s.compare(p, 5, "sqrt(") == 0) {
            const size_t close = s.find(')', p);
            if (close == std::string::npos) return false;
            const std::string r = s.substr(p + 5, close - p - 5);
            if (r == "2") slot = 1;
            else if (r == "3") slot = 2;
            else if (r == "6") slot = 3;
            else return false;
            p = close + 1;
        } else if (!has_int) {
            return false;
        }
        if (m > MAX_COEF) return false;
        q[slot] += sg * m;
        if (q[slot] > MAX_COEF || q[slot] < -MAX_COEF) return false;
        any = true;
    }
    return any;
}

static std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t a = 0;
    for (;;) {
        const size_t b = s.find(sep, a);
        out.push_back(s.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Integer format: point x/sqrt(q), x in Z^n, q = x.x < 2^62.

struct Report {
    int dim = 0;
    i64 count = 0;
    Tally t;
    std::string notes;
};

static Report check_integer(std::ifstream& in, int nthreads) {
    Report rep;
    i64 header_n = -1, header_m = -1;
    std::vector<i64> X, Q;
    std::string line;
    i64 lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;
        const std::string s = trim(line);
        if (s.empty()) continue;
        if (s[0] == '#') {
            const size_t k = s.find("in dimension ");
            long long a, b;
            if (k != std::string::npos && std::sscanf(s.c_str() + k, "in dimension %lld: %lld points", &a, &b) == 2) {
                header_n = a;
                header_m = b;
            }
            continue;
        }
        if (header_n < 1 || header_n > 64) die("no header line '... in dimension N: M points' before data");
        const char* p = s.c_str();
        i64 q;
        if (!parse_i64(p, q)) die("bad integer at line " + std::to_string(lineno));
        i128 norm = 0;
        for (i64 k = 0; k < header_n; ++k) {
            i64 x;
            if (!parse_i64(p, x)) die("too few or bad coordinates at line " + std::to_string(lineno));
            if (x >= ((i64)1 << 31) || x <= -((i64)1 << 31)) die("coordinate too large at line " + std::to_string(lineno));
            norm += (i128)x * x;
            X.push_back(x);
        }
        while (*p == ' ' || *p == '\t') ++p;
        if (*p) die("too many fields at line " + std::to_string(lineno));
        if (norm != (i128)q) die("q is not the squared norm at line " + std::to_string(lineno));
        if (q <= 0) die("zero point at line " + std::to_string(lineno));
        if (q >= ((i64)1 << 62)) die("q too large at line " + std::to_string(lineno));
        Q.push_back(q);
    }
    rep.dim = (int)header_n;
    rep.count = (i64)Q.size();
    if (rep.count != header_m) die("header says " + std::to_string(header_m) + " points, file has " + std::to_string(rep.count));
    const int n = rep.dim;
    std::vector<double> inv(Q.size());
    for (size_t i = 0; i < Q.size(); ++i) inv[i] = 1.0 / std::sqrt((double)Q[i]);

    rep.t = all_pairs(rep.count, nthreads, [&](i64 i, i64 j, Tally& t) {
        const i64* x = &X[i * n];
        const i64* y = &X[j * n];
        i128 s = 0;  // |s| <= sqrt(q_i q_j) < 2^62
        for (int k = 0; k < n; ++k) s += (i128)x[k] * y[k];
        ++t.pairs;
        if (s <= 0) return;
        const double c = (double)s * inv[i] * inv[j];
        if (c > t.max_cos) t.max_cos = c;
        const i128 lhs = s * s, rhs = (i128)Q[i] * Q[j];  // both < 2^124
        if (4 * lhs <= rhs) return;
        t.add(lhs == rhs ? SAME_DIRECTION : VIOLATION, i, j);
    });
    return rep;
}

// ---------------------------------------------------------------------------
// Radical format: coordinates in Z[sqrt2, sqrt3].
// A point is stored as 4 integer blocks of length P (P = n rounded up to 8):
// block 0 = rational parts, 1 = sqrt2 parts, 2 = sqrt3 parts, 3 = sqrt6 parts.

static Report check_leech(std::ifstream& in, int nthreads) {
    Report rep;
    i64 header_n = -1, header_m = -1, dim = -1, npts = -1;
    struct Tail { i64 d; std::vector<i64> T; };  // T has 4k entries
    std::vector<Tail> tails;
    std::vector<std::vector<i64>> pts;  // each 4n entries, coordinate-major
    std::string line;
    i64 lineno = 0, pending_heads = 0;
    std::vector<int> cls_tails;
    auto need_dim = [&](i64 lo) { if (dim < lo || dim > 64) die("missing or bad 'dimension' line"); };
    auto parse_vec = [&](const std::string& body, i64 len) {
        const auto f = split(body, ',');
        if ((i64)f.size() != len) die("expected " + std::to_string(len) + " entries at line " + std::to_string(lineno));
        std::vector<i64> v(4 * len);
        for (i64 k = 0; k < len; ++k)
            if (!parse_quad(f[k], &v[4 * k])) die("bad number '" + f[k] + "' at line " + std::to_string(lineno));
        return v;
    };
    while (std::getline(in, line)) {
        ++lineno;
        const std::string s = trim(line);
        if (s.empty()) continue;
        if (s[0] == '#') {
            long long a, b;
            if (std::sscanf(s.c_str(), "# A kissing configuration of %lld points in R^%lld", &a, &b) == 2) {
                header_m = a;
                header_n = b;
            }
            continue;
        }
        if (pending_heads > 0) {
            const auto f = split(s, ',');
            if (f.size() != 24) die("head line needs 24 integers at line " + std::to_string(lineno));
            i64 h[24];
            for (int k = 0; k < 24; ++k) {
                const char* p = f[k].c_str();
                if (!parse_i64(p, h[k]) || *p || h[k] > SMALL_COEF || h[k] < -SMALL_COEF) die("bad head entry at line " + std::to_string(lineno));
            }
            for (int id : cls_tails) {
                const Tail& t = tails[id];
                std::vector<i64> v(4 * dim, 0);
                for (int k = 0; k < 24; ++k) v[4 * k] = t.d * h[k];
                for (i64 k = 0; k < dim - 24; ++k)
                    for (int c = 0; c < 4; ++c) v[4 * (24 + k) + c] = t.T[4 * k + c];
                pts.push_back(std::move(v));
            }
            --pending_heads;
            continue;
        }
        const size_t sp = s.find(' ');
        const std::string key = s.substr(0, sp);
        const std::string rest = sp == std::string::npos ? "" : trim(s.substr(sp + 1));
        if (key == "dimension") {
            dim = std::atoll(rest.c_str());
        } else if (key == "points") {
            npts = std::atoll(rest.c_str());
        } else if (key == "tail") {
            need_dim(25);
            const auto f = split(rest, ' ');
            if (f.size() != 3) die("bad tail line " + std::to_string(lineno));
            if (std::atoll(f[0].c_str()) != (i64)tails.size()) die("tail ids out of order at line " + std::to_string(lineno));
            const i64 d = std::atoll(f[1].c_str());
            if (d < 1 || d > SMALL_COEF) die("bad tail scale at line " + std::to_string(lineno));
            tails.push_back({d, parse_vec(f[2], dim - 24)});
        } else if (key == "class") {
            need_dim(25);
            const auto f = split(rest, ' ');
            if (f.size() != 2) die("bad class line " + std::to_string(lineno));
            cls_tails.clear();
            for (const auto& id : split(f[0], ',')) {
                const i64 t = std::atoll(id.c_str());
                if (t < 0 || t >= (i64)tails.size() || id.empty()) die("unknown tail id at line " + std::to_string(lineno));
                cls_tails.push_back((int)t);
            }
            pending_heads = std::atoll(f[1].c_str());
            if (pending_heads < 1) die("bad class size at line " + std::to_string(lineno));
        } else if (key == "pole") {
            need_dim(25);
            std::vector<i64> v(4 * dim, 0);
            const auto P = parse_vec(rest, dim - 24);
            std::copy(P.begin(), P.end(), v.begin() + 4 * 24);
            pts.push_back(std::move(v));
        } else if (key == "extra") {
            need_dim(1);
            pts.push_back(parse_vec(rest, dim));
        } else {
            die("unknown line type '" + key + "' at line " + std::to_string(lineno));
        }
    }
    if (pending_heads > 0) die("file ends inside a class");
    need_dim(1);
    if (header_n >= 0 && header_n != dim) die("header dimension disagrees with 'dimension' line");
    if (header_m >= 0 && header_m != npts) die("header count disagrees with 'points' line");
    rep.dim = (int)dim;
    rep.count = (i64)pts.size();
    if (rep.count != npts) die("'points' says " + std::to_string(npts) + ", file has " + std::to_string(rep.count));

    // Pack rows into int16 blocks. For a small row (|coefficient| <= 1000)
    // every weighted product is below 6*10^6, so a dot of 4P <= 256 terms
    // stays below 2^31. Big rows keep their int64 coordinates in pts.
    const i64 N = rep.count;
    const int n = (int)dim, P = (n + 7) / 8 * 8, L = 4 * P;
    std::vector<i16> X((size_t)N * L, 0);   // plain blocks
    std::vector<i16> W((size_t)N * 4 * L, 0);  // 4 weighted copies for the 4 output parts
    std::vector<uint8_t> is_int(N, 1), big(N, 0);
    std::vector<Quad> norm(N);
    std::vector<double> inv(N);
    for (i64 i = 0; i < N; ++i) {
        for (i64 v : pts[i])
            if (v > SMALL_COEF || v < -SMALL_COEF) big[i] = 1;
        for (int k = 0; k < n; ++k)
            for (int c = 1; c < 4; ++c)
                if (pts[i][4 * k + c] != 0) is_int[i] = 0;
        if (big[i]) continue;
        i16* x = &X[(size_t)i * L];
        for (int k = 0; k < n; ++k)
            for (int c = 0; c < 4; ++c) x[c * P + k] = (i16)pts[i][4 * k + c];
        // s = x.y has parts
        //   a = x0.y0 + 2 x1.y1 + 3 x2.y2 + 6 x3.y3
        //   b = x0.y1 + x1.y0 + 3 x2.y3 + 3 x3.y2
        //   c = x0.y2 + x2.y0 + 2 x1.y3 + 2 x3.y1
        //   d = x0.y3 + x3.y0 + x1.y2 + x2.y1
        // so each part is the plain row of x dotted with a weighted copy of y.
        static const int src[4][4] = {{0, 1, 2, 3}, {1, 0, 3, 2}, {2, 3, 0, 1}, {3, 2, 1, 0}};
        static const int wt[4][4] = {{1, 2, 3, 6}, {1, 1, 3, 3}, {1, 2, 1, 2}, {1, 1, 1, 1}};
        i16* w = &W[(size_t)i * 4 * L];
        for (int part = 0; part < 4; ++part)
            for (int blk = 0; blk < 4; ++blk)
                for (int k = 0; k < P; ++k)
                    w[part * L + blk * P + k] = (i16)(wt[part][blk] * x[src[part][blk] * P + k]);
        pts[i].clear();
        pts[i].shrink_to_fit();
    }
    auto dot = [](const i16* a, const i16* b, int len) {
        i32 s = 0;
        for (int k = 0; k < len; ++k) s += (i32)a[k] * (i32)b[k];
        return s;
    };
    // int64 path for pairs with a big row: |coefficient| < 2^20, so each
    // sum is below 64 * 4 * 6 * 2^40 < 2^51.
    auto coef = [&](i64 i, int k, int c) -> i64 {
        return big[i] ? pts[i][4 * k + c] : X[(size_t)i * L + c * P + k];
    };
    auto inner = [&](i64 i, i64 j) {
        if (big[i] || big[j]) {
            i64 a = 0, b = 0, c = 0, d = 0;
            for (int k = 0; k < n; ++k) {
                const i64 x0 = coef(i, k, 0), x1 = coef(i, k, 1), x2 = coef(i, k, 2), x3 = coef(i, k, 3);
                const i64 y0 = coef(j, k, 0), y1 = coef(j, k, 1), y2 = coef(j, k, 2), y3 = coef(j, k, 3);
                a += x0 * y0 + 2 * x1 * y1 + 3 * x2 * y2 + 6 * x3 * y3;
                b += x0 * y1 + x1 * y0 + 3 * x2 * y3 + 3 * x3 * y2;
                c += x0 * y2 + x2 * y0 + 2 * x1 * y3 + 2 * x3 * y1;
                d += x0 * y3 + x3 * y0 + x1 * y2 + x2 * y1;
            }
            return Quad{a, b, c, d};
        }
        const i16* x = &X[(size_t)i * L];
        const i16* w = &W[(size_t)j * 4 * L];
        if (is_int[i] && is_int[j]) return Quad{dot(x, w, P), 0, 0, 0};
        return Quad{dot(x, w, L), dot(x, w + L, L), dot(x, w + 2 * L, L), dot(x, w + 3 * L, L)};
    };
    for (i64 i = 0; i < N; ++i) {
        norm[i] = inner(i, i);
        if (sign(norm[i]) <= 0) die("zero point at index " + std::to_string(i));
        inv[i] = 1.0 / std::sqrt(to_double(norm[i]));
    }

    rep.t = all_pairs(N, nthreads, [&](i64 i, i64 j, Tally& t) {
        const Quad s = inner(i, j);
        ++t.pairs;
        if (sign(s) <= 0) return;
        const double c = to_double(s) * inv[i] * inv[j];
        if (c > t.max_cos) t.max_cos = c;
        const Quad ss = mul(s, s), nn = mul(norm[i], norm[j]);
        const Quad diff{4 * ss.a - nn.a, 4 * ss.b - nn.b, 4 * ss.c - nn.c, 4 * ss.d - nn.d};
        if (!in_range(diff)) { t.add(OUT_OF_RANGE, i, j); return; }
        if (sign(diff) <= 0) return;
        const Quad eq{ss.a - nn.a, ss.b - nn.b, ss.c - nn.c, ss.d - nn.d};
        t.add(in_range(eq) && sign(eq) == 0 ? SAME_DIRECTION : VIOLATION, i, j);
    });
    i64 nint = 0;
    for (auto v : is_int) nint += v;
    i64 nbig = 0;
    for (auto v : big) nbig += v;
    rep.notes = "rows with all-rational coordinates: " + std::to_string(nint) + ", other rows: " + std::to_string(N - nint) +
                ", rows with a coefficient above " + std::to_string(SMALL_COEF) + ": " + std::to_string(nbig);
    return rep;
}

// ---------------------------------------------------------------------------

static bool selftest() {
    // Spot checks of the exact sign routine.
    struct { i64 a, b, c, d; int want; } cases[] = {
        {1, 0, 0, 0, 1}, {0, -1, 0, 0, -1}, {1, 0, -1, 0, -1}, {2, 0, -1, 0, 1},
        {0, 0, 0, 0, 0}, {5, -2, -1, 0, 1},  // 5 - 2.828 - 1.732 = 0.44
        {-5, 2, 1, 1, 1},                     // -5 + 2.828 + 1.732 + 2.449 = 2.01
        {-10, 2, 1, 1, -1}, {0, 1, 0, -1, -1}, {1, 1, 1, -1, 1}, // 4.146 - 2.449
        {(i64)1 << 60, -((i64)1 << 60), 0, 0, -1}, {-((i64)1 << 60), 0, 0, (i64)1 << 59, 1},
    };
    for (auto& c : cases)
        if (sign_quad(c.a, c.b, c.c, c.d) != c.want) return false;
    // (1 + sqrt2)^2 = 3 + 2 sqrt2, so 3 + 2 sqrt2 - (1 + sqrt2)^2 = 0.
    const Quad p = mul({1, 1, 0, 0}, {1, 1, 0, 0});
    if (sign({p.a - 3, p.b - 2, 0, 0}) != 0) return false;
    // (sqrt2 + sqrt3)^2 = 5 + 2 sqrt6.
    const Quad r = mul({0, 1, 1, 0}, {0, 1, 1, 0});
    return r.a == 5 && r.b == 0 && r.c == 0 && r.d == 2;
}

int main(int argc, char** argv) {
    const char* path = nullptr;
    int nthreads = 1;
    for (int k = 1; k < argc; ++k) {
        if (std::strcmp(argv[k], "--threads") == 0 && k + 1 < argc) nthreads = std::atoi(argv[++k]);
        else if (!path) path = argv[k];
        else path = nullptr, k = argc;
    }
    if (!path || nthreads < 1) {
        std::fprintf(stderr, "usage: %s path/to/file.txt [--threads N]\n", argv[0]);
        return 2;
    }
    if (!selftest()) die("internal self-test of the exact arithmetic failed");
    std::ifstream in(path);
    if (!in) die(std::string("cannot open ") + path);

    // Detect the format from the first non-comment line.
    std::string line, first;
    while (std::getline(in, line)) {
        const std::string s = trim(line);
        if (!s.empty() && s[0] != '#') { first = s; break; }
    }
    in.clear();
    in.seekg(0);
    const bool leech = first.rfind("dimension", 0) == 0;
    const auto t0 = std::chrono::steady_clock::now();
    const Report r = leech ? check_leech(in, nthreads) : check_integer(in, nthreads);
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    const i64 expected_pairs = r.count * (r.count - 1) / 2;
    const bool pass = r.t.pairs == expected_pairs && r.t.violations == 0 && r.t.out_of_range == 0;
    std::printf("file: %s\n", path);
    std::printf("format: %s\n", leech ? "Z[sqrt2,sqrt3]" : "integer");
    if (!r.notes.empty()) std::printf("%s\n", r.notes.c_str());
    std::printf("dimension: %d\n", r.dim);
    std::printf("points: %lld\n", (long long)r.count);
    std::printf("pairs checked: %lld of %lld\n", (long long)r.t.pairs, (long long)expected_pairs);
    std::printf("max cosine (float, information only): %.12f\n", r.t.max_cos);
    std::printf("violations: %lld (same-direction pairs among them: %lld)\n", (long long)r.t.violations,
                (long long)r.t.same_dir);
    if (r.t.out_of_range) std::printf("pairs outside the exact arithmetic range: %lld\n", (long long)r.t.out_of_range);
    if (r.t.bad_i >= 0) std::printf("first bad pair: points %lld and %lld (0-based, file order)\n", (long long)r.t.bad_i, (long long)r.t.bad_j);
    std::printf("threads: %d\n", nthreads);
    std::printf("time: %.1f s\n", sec);
    std::printf("result: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
