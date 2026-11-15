#pragma once

#include <NTL/ZZ_pX.h>
#include <NTL/vec_ZZ_p.h>
#include <vector>

namespace fastpoly {

using NTL::ZZ_p;
using NTL::ZZ_pX;
using NTL::vec_ZZ_p;

inline void build_tree(std::vector<ZZ_pX> &tree, const vec_ZZ_p &x,
                       long node, long l, long r)
{
    if (r - l == 1) {
        tree[node].SetLength(2);
        NTL::SetCoeff(tree[node], 0, -x[l]);
        NTL::SetCoeff(tree[node], 1, 1);
        return;
    }
    long m = (l + r) / 2;
    build_tree(tree, x, 2 * node, l, m);
    build_tree(tree, x, 2 * node + 1, m, r);
    NTL::mul(tree[node], tree[2 * node], tree[2 * node + 1]);
}

inline void eval_rec(vec_ZZ_p &out, const ZZ_pX &f, const vec_ZZ_p &x,
                     const std::vector<ZZ_pX> &tree, long node, long l, long r)
{
    if (r - l == 1) {
        out[l] = NTL::eval(f, x[l]);
        return;
    }
    long m = (l + r) / 2;
    ZZ_pX fl, fr;
    NTL::rem(fl, f, tree[2 * node]);
    NTL::rem(fr, f, tree[2 * node + 1]);
    eval_rec(out, fl, x, tree, 2 * node, l, m);
    eval_rec(out, fr, x, tree, 2 * node + 1, m, r);
}

inline void multipoint_eval(vec_ZZ_p &out, const ZZ_pX &f, const vec_ZZ_p &x)
{
    long n = x.length();
    out.SetLength(n);
    if (n == 0) return;
    std::vector<ZZ_pX> tree(4 * n);
    build_tree(tree, x, 1, 0, n);
    eval_rec(out, f, x, tree, 1, 0, n);
}

inline ZZ_pX interp_rec(const vec_ZZ_p &w, const std::vector<ZZ_pX> &tree,
                        long node, long l, long r)
{
    if (r - l == 1) {
        ZZ_pX p;
        NTL::SetCoeff(p, 0, w[l]);
        return p;
    }
    long m = (l + r) / 2;
    ZZ_pX left = interp_rec(w, tree, 2 * node, l, m);
    ZZ_pX right = interp_rec(w, tree, 2 * node + 1, m, r);
    ZZ_pX a, b;
    NTL::mul(a, left, tree[2 * node + 1]);
    NTL::mul(b, right, tree[2 * node]);
    ZZ_pX out;
    NTL::add(out, a, b);
    return out;
}

inline void interpolate(ZZ_pX &g, const vec_ZZ_p &x, const vec_ZZ_p &y)
{
    long n = x.length();
    if (n == 0) {
        NTL::clear(g);
        return;
    }
    if (n == 1) {
        NTL::SetCoeff(g, 0, y[0]);
        return;
    }

    std::vector<ZZ_pX> tree(4 * n);
    build_tree(tree, x, 1, 0, n);

    ZZ_pX derivative;
    NTL::diff(derivative, tree[1]);
    vec_ZZ_p den;
    multipoint_eval(den, derivative, x);

    vec_ZZ_p w;
    w.SetLength(n);
    for (long i = 0; i < n; ++i) {
        w[i] = y[i] / den[i];
    }
    g = interp_rec(w, tree, 1, 0, n);
}

} // namespace fastpoly
