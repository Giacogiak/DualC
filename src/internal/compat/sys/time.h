/* An empty <sys/time.h> for MSVC, whose C library has none. Only the vendored
 * predicates.c includes it (src/internal/exact_predicates.c), and it uses
 * nothing from it. On the include path for that file on MSVC alone. */
#pragma once
