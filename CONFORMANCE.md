# Conformance

This document defines what the project means when it calls something
“a stitch implementation”, a scoped stitch port, or the stitch
formalisation. It is the listing standard for implementations in this
repository and for third-party ports the project is asked to list.

[SPEC.md](./SPEC.md) is normative. The Python implementation in
`stitch-python/` is the operational reference. If SPEC and the Python
reference appear to disagree, that is a defect to report and resolve;
it is not permission for a port to choose whichever behaviour is easier.

Conformance is always relative to a stated scope. A port that proves
the keyless bilateral core has not thereby proved N-party agreement or
Ed25519 enforcement, and the project will list it only under the scope
it actually demonstrated.

## Normative contract

A port claiming a feature must follow the corresponding part of SPEC:

- **Constants** and **The conservation law**, including rejection of an
  over-budget `EFFECT`, `TICK` reset, and the limited role of
  `WITHDRAW`.
- **Wire format**, including the common fields and every per-stitch
  field for the stitches in scope.
- **Hash computation**: canonical JSON, the FNV-1a 64-bit chain, and
  64-bit unsigned wraparound.
- **Surprise oracle** for the default discrete oracle in the reference.
  A replacement oracle is an implementation seam, not a change to the
  ledger semantics; its recorded `s` values are what audit replays.
- **Tenant identity** and **Cell ID**, including the fact that an
  identity string is a claim unless an explicitly scoped signature path
  is in use.
- **Byte-exactness**, including sorted keys, `(",", ":")` separators,
  raw UTF-8 rather than `\uXXXX` escapes for non-ASCII string content,
  the control-character rules, and the rule that conformance requires
  no global clock or coordinator.
- **Numeric policy (v0.4)**: auditors hash original line bytes or a
  canonical form that preserves number text verbatim; they do not hash
  a re-serialisation of parsed floats. Writers in scope must follow the
  non-finite-float, out-of-range-integer, and measured float-band rules
  for that scope.
- **Agreement (v0.2)**, if agreement is claimed: exchange bookings,
  foreign-ledger audit, mirror minting, the distinct refusal reasons,
  and the head-hash anchor.
- **Tenant budget declarations (v0.3)**, if audit or writing of
  declarations is claimed: the position-aware rule and the two-knobs
  distinction between enforced and declared budget.
- **N-party agreement (v0.3)**, if claimed: `agree_n` and
  `agreed_state_n` semantics and refusal behaviour.
- **Ed25519 signatures (v0.3)**, if keyed attestations are claimed:
  RFC 8032 signatures, the exact signing material, opt-in audit wiring,
  and the separate `signatures_ok` judgement.
- **Signed bookings**, if keyed bookings are claimed: the
  `stitch-booking/1` signing material, `booking_keys` enforcement,
  `unsigned_booking` and `bad_signature`, enforcement order, and the
  limits in “What a booking signature does NOT prove”.

The older **Implementation status** table inside SPEC is a historical
status note and is not the current scope record. The current record is
the table below plus each implementation's own conformance record.

## The byte-exactness bar

For the same deterministic inputs, a conforming writer in scope must
produce the same ledger bytes and the same `h` values as the Python
reference — not merely JSON with the same meaning. Deterministic inputs
include the entry fields and timestamps; cross-language fixtures use
fixed or explicit timestamps because the Python writer otherwise reads
its local clock.

For a claimed read surface, the port must also produce the reference
verdict for the shared fixtures: chain and law results, agreement and
refusal reasons, state folds, and anchor states. Where the reference
returns human-readable `detail` strings and the port claims
verdict-identical output, those strings are part of the comparison, as
they are in the C battery.

Byte-exactness applies to ledger lines, hash recomputation, cell IDs,
payload hashes, and audit re-serialisation for the surfaces in scope.
It does not make timestamps into a global ordering: a verifier must not
compare `t` across ledgers or require writers to be synchronised.

A known divergence on an input outside the shared fixtures does not
become conforming by being obscure. It must be written in the port's
public conformance record, with the input class and the difference in
behaviour. The listing may be narrowed to exclude that class, as the C
record does for pathological inputs on which Python raises and C must
return a verdict. Unexplained failures on a claimed corpus are not
acceptable.

## Shared evidence

No single test file is the whole standard. The project uses four
mutually checking bodies of evidence.

### 1. The Python reference suite

`stitch-python/tests/` is the behavioural reference for the current
library surface. Its files cover the core and all 11 stitches, byte
exactness, non-ASCII content, budgets, bilateral and N-party agreement,
the numeric policy and its write-time enforcement, Ed25519, RFC 8032
vectors, signed attestations, and signed bookings.

A non-Python port need not execute pytest itself. It must map every
behaviour in its claimed scope to fixtures or native tests derived from
the reference behaviour, and state which reference behaviours are out
of scope.

### 2. The cross-language vectors

`tests/cross_lang.py` contains the canonical deterministic sequence and
fixed ledger reference entries. The Python side asserts the reference
hashes `b513cabe780e2894` and `24c56ec21ef20816`; the Rust test
`test_cross_lang_byte_exact_with_python` asserts the same hashes and
file bytes.

Those vectors are necessary, not sufficient. The file's own status note
says the full-sequence comparison across every implementation is not
contained in that one harness; the C battery and each port's native
suite supply the wider evidence for their scopes.

### 3. The C conformance method

`c/conformance/gen_fixtures.py` generates fixtures with the Python
reference and records its verdicts. `c/conformance/run.sh` rebuilds the
C tool, runs it against the generated files, and diffs bytes and
verdicts. The present battery has **69 checks** covering:

- the cross-language hashes and canonical scenario;
- the rich two-ledger `scenario2`, byte-compared in both directions;
- audit and verify fixtures, including tampering, law violations, and
  position-aware budget declarations;
- agreement, state folds, refusal reasons, and the anchor states;
- a 200,942-pattern float-formatting corpus;
- canonicalisation and payload-hash corpora; and
- Python accepting and minting from C-written ledgers.

A future port may use a different driver, but not a weaker method:
fixtures must be generated from the reference, outputs must be diffed
rather than merely asserted plausible, and the corpus must exercise
both directions where the port both reads and writes.

The complete C record, including known divergences on pathological
inputs, is [`c/CONFORMANCE.md`](./c/CONFORMANCE.md).

### 4. CI as the reference enforcement

[`.github/workflows/ci.yml`](./.github/workflows/ci.yml) runs four jobs
on pushes to `main` and on pull requests:

| Job | Gate |
|---|---|
| `python` | Install `./stitch-python[test]`; import/version smoke; run the Python suite from the repo root. |
| `rust` | Run `cargo test` in `stitch-rs/`; run the Python side of `tests/cross_lang.py`. |
| `c` | Install the Python package; build with `make -C c`; run `c/conformance/run.sh` and require `conformance: 69 passed, 0 failed`. |
| `lean` | Install the toolchain pinned in `lean/lean-toolchain`; run `lake build`; run `lean/check_axioms.sh`, which fails if `sorryAx` appears under the headline theorems. |

CI is the enforcement record for the implementations in this
repository. An external port is not certified merely because this
repository's CI is green; it needs its own reproducible CI or scripted
battery over the same reference fixtures.

## Current scopes

| Surface | What the project may claim today |
|---|---|
| **Python** (`stitch-python/`) | The reference implementation of the current library surface: all 11 stitches, audit, bilateral and N-party agreement, budget declarations, numeric policy, opt-in signed attestations, and opt-in signed bookings. |
| **Rust** (`stitch-rs/`) | A scoped port: tenant and ledger, the `bind` stitch, hash-chain append/verify, the cross-language ledger vectors, and the precision-safe v0.2 foreign-ledger audit. Agreement/minting, N-party agreement, budget declarations as an audit input, and the v0.3 signature paths are not ported. |
| **C99** (`c/`) | A scoped port of the keyless core: canonical JSON, ledger append/verify, all 11 write-side stitches, unsigned booking helpers, position-aware audit, bilateral agreement/state, and the anchor. Ed25519 paths, key enforcement, and N-party agreement are not ported. Its pathological-input divergences are listed in `c/CONFORMANCE.md`. |
| **Lean** (`lean/`) | A formalisation, not a runtime implementation. It models the agreement and conservation core and proves `Stitch.minted_sound`, `Stitch.conservation_of_written`, and `Stitch.lawful_of_written` under the model's stated simplifications. It does not emit ledgers or claim byte-exactness. Signed bookings, N-party agreement, and the anchor's prefix bookkeeping are not modelled. |

This table is deliberately narrower than “four implementations”.
Python, Rust, and C99 are implementations with different scopes. Lean
is a machine-checked model of two core properties. Calling all four
full implementations would erase the distinctions the evidence supports.

## Checklist for listing a new port

Before the project lists a third-party implementation, the port must
provide all of the following:

- [ ] **A scope statement.** Name the SPEC sections and features
  implemented — for example “keyless bilateral core” or “full current
  Python surface”. Do not use an unqualified “stitch implementation”
  for a partial port.
- [ ] **A public source and build record.** Link the source, licence,
  supported toolchain and platforms, and the exact commands that build
  it from a clean checkout.
- [ ] **Canonical-format evidence.** Demonstrate sorted-key canonical
  JSON with the specified separators and raw UTF-8, FNV-1a chain
  hashes, cell IDs, and payload hashes for the claimed scope.
- [ ] **Deterministic byte comparison.** Pass the reference entries in
  `tests/cross_lang.py` and a deterministic sequence covering every
  write operation in scope, byte-compared against fixtures generated
  by the Python reference.
- [ ] **Read-path evidence.** For every claimed audit, agreement,
  state-fold, or anchor surface, diff verdicts and refusal reasons
  against reference fixtures, including broken-chain, law-violated,
  missing-mirror, peer-mismatch, and payload-mismatch cases that apply.
- [ ] **Numeric evidence.** Cover the SPEC numeric policy for the
  claimed scope, including non-finite values, integer boundaries,
  subnormals and maximum doubles, the `1e-9 ≤ |x| < 1e-4` writer
  divergence band, and verbatim number-text handling on audit.
- [ ] **Feature-specific evidence.** If budget declarations are
  claimed, test position-aware declarations. If N-party agreement is
  claimed, test the ring and refusal cases. If Ed25519 is claimed, pin
  RFC 8032 vectors and test both attestation and booking enforcement,
  including unsigned and wrong-key refusals.
- [ ] **A public conformance record.** State the commands, tool
  versions, platforms, corpus sizes, pass/fail totals, out-of-scope
  features, and every known divergence. “Tests pass” without the corpus
  and scope is not a conformance record.
- [ ] **A reproducible gate.** Provide CI or a checked-in script that
  runs the build and battery from a clean checkout and fails on any
  unexplained mismatch. An in-repo port must be added to the project's
  CI or to an equivalent required workflow.
- [ ] **A security statement.** Say whether the port is keyless or
  keyed, how it handles the limits in [SECURITY.md](./SECURITY.md),
  and — if signed bookings are claimed — how it states the
  verbatim-replay residual.
- [ ] **Maintainer review of the listing words.** The project lists the
  exact proven scope, such as “C99 keyless core”, not a broader parity
  claim inferred from a repository name or a passing subset.

A formalisation is listed under a separate rule: state the modelled
properties and simplifications, pin the toolchain, build from a clean
checkout, and run an axiom or `sorry` gate equivalent to
`lean/check_axioms.sh`. A proof about a model must not be listed as a
byte-exact implementation.
