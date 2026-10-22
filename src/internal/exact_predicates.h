// Shewchuk's exact predicates (src/internal/third_party/shewchuk/predicates.c,
// public domain, see THIRD_PARTY.md), as renamed by exact_predicates.c.
#pragma once

extern "C" {
void dualc_pred_exactinit();
double dualc_pred_orient2d(double* pa, double* pb, double* pc);
double dualc_pred_orient3d(double* pa, double* pb, double* pc, double* pd);
double dualc_pred_insphere(double* pa, double* pb, double* pc, double* pd,
                           double* pe);
}

namespace dualc {
namespace internal {

// Sets the predicates' error bounds; must run once before any call below.
// Idempotent and cheap, but it writes globals: call it under a std::call_once.
inline void exactPredicatesInit() { dualc_pred_exactinit(); }

// The definitions take double* and never write through them.
//
// > 0 when (a, b, c, d) is positively oriented: d lies below the plane of a,
// b, c, which appear counter-clockwise seen from above. Exact.
inline double orient3d(const double* a, const double* b, const double* c,
                       const double* d) {
  return dualc_pred_orient3d(const_cast<double*>(a), const_cast<double*>(b),
                             const_cast<double*>(c), const_cast<double*>(d));
}

// > 0 when e lies strictly inside the sphere through a, b, c, d, which must be
// positively oriented (orient3d > 0). Exact.
inline double insphere(const double* a, const double* b, const double* c,
                       const double* d, const double* e) {
  return dualc_pred_insphere(const_cast<double*>(a), const_cast<double*>(b),
                             const_cast<double*>(c), const_cast<double*>(d),
                             const_cast<double*>(e));
}

// The 2D orientation of (a, b, c), each a pointer to two doubles. Exact.
inline double orient2d(const double* a, const double* b, const double* c) {
  return dualc_pred_orient2d(const_cast<double*>(a), const_cast<double*>(b),
                             const_cast<double*>(c));
}

} // namespace internal
} // namespace dualc
