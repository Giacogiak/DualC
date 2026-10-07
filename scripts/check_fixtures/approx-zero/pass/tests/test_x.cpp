// Guarded zero comparisons, and non-zero ones, pass.
REQUIRE(f(0.0) == Approx(0.0).margin(1e-12));
REQUIRE(f(1.0) ==
        Catch::Approx(0.0)
            .margin(1e-9));
REQUIRE(g == Approx(0.5));
REQUIRE(m == Approx(0.25));
REQUIRE(h == Approx(-0.5));
REQUIRE(k == Approx(10.0));
