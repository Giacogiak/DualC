# Tool 2: `dualc_primitive`

**Retired 2026-10-16.** `dualc_primitive`, the CLI that contoured one analytic primitive
under a chain of decorator and domain-operator flags, was removed as-is: every primitive and
operator it took is a field-graph node (D-55 in [the settled decisions](../decisions/01-settled.md)); tool number 2
is never reused. The record of the retirement is [roadmap 21/08](../roadmap/21-cli-consolidation/08-retire-tools.md) ([#60](../roadmap/21-cli-consolidation/01-record.md#60-retire-the-eight-tools)).

Its whole catalogue, in graph syntax and with its IDs kept, is
[11/12](11-dualc_field/12-primitive-catalogue.md) — the same bytes for the same parameters:

| Was | Now |
| --- | --- |
| `dualc_primitive <name> [params...]`, the positional parameter order | `dualc_field --expr '<name>(…)'`: named keys or the same positional order ([11/12 § How a primitive is written](11-dualc_field/12-primitive-catalogue.md#how-a-primitive-is-written)) |
| The 30 primitives, Tier A / B / C | P1–P30 in one table, with their defaults ([11/12 § The 30 analytic primitives](11-dualc_field/12-primitive-catalogue.md#the-30-analytic-primitives)) |
| The decorator flags (`--onion`, `--scale`, …), applied left to right | decorator nodes nested innermost first, D1–D7 ([11/12 § The decorators](11-dualc_field/12-primitive-catalogue.md#the-decorators-one-child)) |
| The domain-operator flags (`--twist`, `--repeat`, …) | O1–O6 ([11/12 § The domain operators](11-dualc_field/12-primitive-catalogue.md#the-domain-operators-one-child)) |
| `--displace sine\|gyroid\|bumps` | the `displace` node, F1–F3; its default frequency is 6, the flag's was 4 (D-63) ([11/12 § The three displace functions](11-dualc_field/12-primitive-catalogue.md#the-three-displace-functions)) |
| `--list` | `dualc_field --list` |
| Recipes C1–C7 | C1–C7 ([11/12 § Recipes](11-dualc_field/12-primitive-catalogue.md#recipes)) |

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
