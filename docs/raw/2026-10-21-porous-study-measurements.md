# Porous shell study — the measurement dump of P01

Immutable raw output of the P01 study of [roadmap 23](../roadmap/23-porous-shell/01-study.md). Host: this Linux box (8 hardware threads), GCC 15, CMake 4, Ninja; drivers in scratch (`/tmp/porous-study`, not kept), `-O2`, one thread unless named. Timings taken with no build running.

## Downloads

```
geogram Delaunay_psm_1.10.1.tar.gz (release v1.10.1, 2026-09-01)
4b0430c279e2cdda1ddceccb275db2a022bc44b2be51033847ae0bcce86636e4  Delaunay_psm_1.10.1.tar.gz
13118 Delaunay_example.cpp
1128086 Delaunay_psm.cpp
342628 Delaunay_psm.h
514 README.txt
Shewchuk predicates.c (https://www.cs.cmu.edu/afs/cs/project/quake/public/code/predicates.c, May 18 1996, 'Placed in the public domain')
f8662c3f407d1c1c5dcd4dd49ea8b8ddd801a71827d65734206ddc3741792029  predicates.c
```

## geogram PSM, compile

```
g++ -std=c++17 -O2 -Wall -Wextra -c Delaunay_psm.cpp  -> 0 warnings, 46 s wall (build running in parallel)
g++ -std=c++20 -O2 -Wall -Wextra -c Delaunay_psm.cpp  -> 0 warnings, 56 s wall (build running in parallel)
```

## geogram PSM, 100k random points (mt19937_64 seed 42) and the 20^3 grid; hash = FNV-1a of the tet list as returned; two processes

```
BDEL random100k rep0: 595.3 ms, 671842 tets, hash a8ea576261d86ae2
BDEL grid20^3   rep0: 398.7 ms, 41154 tets, hash f34f2d8ad565226f
BDEL random100k rep1: 585.6 ms, 671842 tets, hash 07ad1fe1251a0a5e
BDEL grid20^3   rep1: 403.0 ms, 41154 tets, hash b818fdebd5b86ac5
BDEL random100k rep2: 589.8 ms, 671842 tets, hash 3469140783ec7956
BDEL grid20^3   rep2: 398.9 ms, 41154 tets, hash 796284a6976309b3
BDEL random100k rep0: 580.4 ms, 671842 tets, hash a8ea576261d86ae2
BDEL grid20^3   rep0: 397.6 ms, 41154 tets, hash f34f2d8ad565226f
BDEL random100k rep1: 581.8 ms, 671842 tets, hash 07ad1fe1251a0a5e
BDEL grid20^3   rep1: 401.6 ms, 41154 tets, hash b818fdebd5b86ac5
BDEL random100k rep2: 581.6 ms, 671842 tets, hash 3469140783ec7956
BDEL grid20^3   rep2: 396.6 ms, 41154 tets, hash 796284a6976309b3
PDEL random100k rep0: 226.4 ms, 671842 tets, hash 00b412856e7b2512
PDEL grid20^3   rep0: 157.8 ms, 41154 tets, hash 31e6b2b1b4a67cfb
PDEL random100k rep1: 219.5 ms, 671842 tets, hash 210ca8f030d6a752
PDEL grid20^3   rep1: 178.0 ms, 41154 tets, hash 37f32c996e4c621d
PDEL random100k rep2: 227.4 ms, 671842 tets, hash 5213a544692b4af2
PDEL grid20^3   rep2: 156.1 ms, 41154 tets, hash 7134822e98c21a79
```

## Same, on the PCG32 points of the prototype; hash = FNV-1a of the canonical (sorted) tet list

```
BDEL random100k rep0: 591.4 ms, 672288 tets, hash 023e8635cf0fd7a5
BDEL grid20^3   rep0: 400.4 ms, 41154 tets, hash de9ff41e15aad423
PDEL random100k rep0: 220.7 ms, 672288 tets, hash 023e8635cf0fd7a5
PDEL grid20^3   rep0: 156.0 ms, 41154 tets, hash de9ff41e15aad423
```

## Prototype Bowyer-Watson (predicates.c as -std=gnu89 -ffp-contract=off), super-tet at 1000x extent, Morton order; two runs in one process

```
random100k: n=100000 1239.5 ms, 672241 real tets (of 672706 alive incl. super), nonpositive=0, empty-sphere violations=0/100000, walk steps/pt=7.7, canon hash 9105f7a2a35d1eaf
grid20^3: n=8000 231.1 ms, 41154 real tets (of 45718 alive incl. super), nonpositive=0, empty-sphere violations=0/100000, walk steps/pt=6.6, canon hash 9e3d91fe4728546b
random100k: n=100000 1232.2 ms, 672241 real tets (of 672706 alive incl. super), nonpositive=0, empty-sphere violations=0/100000, walk steps/pt=7.7, canon hash 9105f7a2a35d1eaf
grid20^3: n=8000 230.4 ms, 41154 real tets (of 45718 alive incl. super), nonpositive=0, empty-sphere violations=0/100000, walk steps/pt=6.6, canon hash 9e3d91fe4728546b
```

## Distributions, periodic 10^3 box, s = 0.5, PCG32; CV by Monte-Carlo, 200 samples per cell

```
bridson: 4583 seeds in 193.3 ms (23710 seeds/s)
poisson          N=  4583 density=0.573/s^3  cellVol CV=0.153 (MC noise ~0.071)  meanNN=1.051s minNN=1.000s  maxEmpty=1.162s
poisson+relax1   N=  4583 density=0.573/s^3  cellVol CV=0.130 (MC noise ~0.071)  meanNN=1.060s minNN=0.800s  maxEmpty=1.113s
  lloyd 1 it x 32 samples/seed: 285.2 ms/iteration
poisson+relax5   N=  4583 density=0.573/s^3  cellVol CV=0.115 (MC noise ~0.071)  meanNN=1.086s minNN=0.731s  maxEmpty=1.098s
  lloyd 5 it x 32 samples/seed: 283.5 ms/iteration
poisson+relax20  N=  4583 density=0.573/s^3  cellVol CV=0.105 (MC noise ~0.071)  meanNN=1.109s minNN=0.800s  maxEmpty=1.104s
  lloyd 20 it x 32 samples/seed: 285.1 ms/iteration
jitter(1.0)      N=  8000 density=1.000/s^3  cellVol CV=0.258 (MC noise ~0.071)  meanNN=0.632s minNN=0.067s  maxEmpty=1.168s
random           N=  8000 density=1.000/s^3  cellVol CV=0.430 (MC noise ~0.071)  meanNN=0.556s minNN=0.028s  maxEmpty=1.500s
jitter(0.5)      N=  8000 density=1.000/s^3  cellVol CV=0.155 (MC noise ~0.071)  meanNN=0.777s minNN=0.507s  maxEmpty=0.963s
random+relax20   N=  8000 density=1.000/s^3  cellVol CV=0.113 (MC noise ~0.071)  meanNN=0.917s minNN=0.640s  maxEmpty=0.880s
```

## GL limits on this host (glxinfo -l)

```
OpenGL renderer string: NV92
    GL_MAX_3D_TEXTURE_SIZE = 2048
    GL_MAX_TEXTURE_IMAGE_UNITS = 16
    GL_MAX_TEXTURE_SIZE = 8192
OpenGL core profile version string: 3.3 (Core Profile) Mesa 26.0.8-1ubuntu0.3
OpenGL renderer string: NV92
```

## Build trees of the worktree (Release, Ninja, -j8, fresh: fetches geometry-central, Catch2, GLFW)

```
build/: configure 134s build 295s rc=0
build-gl/: configure 81s rc=0
build-gl/: build 244s rc=0
(build/ configured with TESTS, EXAMPLES, C_ABI; build-gl/ adds FIELD_VIEW and GLSL_PARITY; the first build-gl configure was interrupted by a tool time limit and redone from scratch)
```
