// A zero target with no margin is exact equality, including split lines.
REQUIRE(f(1.0) == Approx(0.0).margin(1e-12));
REQUIRE(f(0.0) ==
        Catch::Approx(0.0));
