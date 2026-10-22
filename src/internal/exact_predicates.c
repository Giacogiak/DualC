/* Shewchuk's exact predicates for the porous Delaunay (roadmap 23 #64).
 *
 * The vendored file (third_party/shewchuk/predicates.c, see THIRD_PARTY.md)
 * stays byte-identical to upstream; this DualC translation unit is the only
 * one that compiles it, and does three things around it:
 *
 *  - every external symbol it defines gets a dualc_pred_ prefix, so a host
 *    that links its own copy (Triangle, TetGen and many others carry one)
 *    does not collide with libdualc;
 *  - its random() calls (in test helpers DualC never calls) go to a local
 *    stub, so the file needs no POSIX random() and links on MSVC;
 *  - the C headers it includes come first, so the renames cannot touch them.
 *
 * It is compiled as C90 with extensions (the file uses K&R definitions, which
 * C23 removed) and with no FMA contraction (CMakeLists.txt): exact arithmetic
 * needs every product rounded on its own. On MSVC, whose C library has no
 * <sys/time.h>, an empty stand-in is put on the include path (compat/).
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define ccwerrboundA dualc_pred_ccwerrboundA
#define ccwerrboundB dualc_pred_ccwerrboundB
#define ccwerrboundC dualc_pred_ccwerrboundC
#define compress dualc_pred_compress
#define doublerand dualc_pred_doublerand
#define epsilon dualc_pred_epsilon
#define estimate dualc_pred_estimate
#define exactinit dualc_pred_exactinit
#define expansion_sum dualc_pred_expansion_sum
#define expansion_sum_zeroelim1 dualc_pred_expansion_sum_zeroelim1
#define expansion_sum_zeroelim2 dualc_pred_expansion_sum_zeroelim2
#define fast_expansion_sum dualc_pred_fast_expansion_sum
#define fast_expansion_sum_zeroelim dualc_pred_fast_expansion_sum_zeroelim
#define floatrand dualc_pred_floatrand
#define grow_expansion dualc_pred_grow_expansion
#define grow_expansion_zeroelim dualc_pred_grow_expansion_zeroelim
#define iccerrboundA dualc_pred_iccerrboundA
#define iccerrboundB dualc_pred_iccerrboundB
#define iccerrboundC dualc_pred_iccerrboundC
#define incircle dualc_pred_incircle
#define incircleadapt dualc_pred_incircleadapt
#define incircleexact dualc_pred_incircleexact
#define incirclefast dualc_pred_incirclefast
#define incircleslow dualc_pred_incircleslow
#define insphere dualc_pred_insphere
#define insphereadapt dualc_pred_insphereadapt
#define insphereexact dualc_pred_insphereexact
#define inspherefast dualc_pred_inspherefast
#define insphereslow dualc_pred_insphereslow
#define isperrboundA dualc_pred_isperrboundA
#define isperrboundB dualc_pred_isperrboundB
#define isperrboundC dualc_pred_isperrboundC
#define linear_expansion_sum dualc_pred_linear_expansion_sum
#define linear_expansion_sum_zeroelim dualc_pred_linear_expansion_sum_zeroelim
#define narrowdoublerand dualc_pred_narrowdoublerand
#define narrowfloatrand dualc_pred_narrowfloatrand
#define o3derrboundA dualc_pred_o3derrboundA
#define o3derrboundB dualc_pred_o3derrboundB
#define o3derrboundC dualc_pred_o3derrboundC
#define orient2d dualc_pred_orient2d
#define orient2dadapt dualc_pred_orient2dadapt
#define orient2dexact dualc_pred_orient2dexact
#define orient2dfast dualc_pred_orient2dfast
#define orient2dslow dualc_pred_orient2dslow
#define orient3d dualc_pred_orient3d
#define orient3dadapt dualc_pred_orient3dadapt
#define orient3dexact dualc_pred_orient3dexact
#define orient3dfast dualc_pred_orient3dfast
#define orient3dslow dualc_pred_orient3dslow
#define resulterrbound dualc_pred_resulterrbound
#define scale_expansion dualc_pred_scale_expansion
#define scale_expansion_zeroelim dualc_pred_scale_expansion_zeroelim
#define splitter dualc_pred_splitter
#define uniformdoublerand dualc_pred_uniformdoublerand
#define uniformfloatrand dualc_pred_uniformfloatrand

#define random dualc_pred_random
static long dualc_pred_random(void) { return 0; }

/* Through the SYSTEM include path, as the other vendored code. */
#include <shewchuk/predicates.c>
