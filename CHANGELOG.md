# Changelog

All notable changes to stitch are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
the project's own numbering (the wire format has not changed since v0.3 —
see SPEC.md). Entries up to 0.4.1 were reconstructed on 2026-11-02 from the
git record, SPEC.md's changelog, and VERIFICATION.md.

## [Unreleased]

Work merged after the v0.4.1 release assembly (2026-10-27); it ships with
the next version.

### Added

- **C99 port** (`c/`): a pure C99 implementation of the keyless stitch
  core — canonical JSON, the hash chain, audit, and bilateral minting —
  verified byte-for-byte against the Python reference by a 69-check
  conformance battery (committed 2026-10-29, merged 2026-11-02). Two
  x86_64 conformance failures found by that battery were fixed the same
  day: a merger tick-ordering bug in the scenario driver, and the float
  formatter now computes the exact Python-`repr` digits instead of
  trusting the platform `printf`.
- **Lean 4 formalisation** (`lean/`): a formal specification of the
  agreement and conservation core. `Stitch.minted_sound` depends on no
  axioms at all; the conservation and lawfulness theorems depend only on
  `propext` and `Quot.sound`. CI runs an axiom gate that fails if
  `sorryAx` ever appears under the headline theorems.
- **Release machinery**: this changelog; `scripts/attest_release.py`,
  which records a release's artifact hashes in a stitch ledger signed by
  the "stitch-release" tenant; tag-driven GitHub releases; OIDC publish
  workflows for PyPI and crates.io (pending the owner's one-time
  publisher registrations).

### Changed

- The Rust crate is renamed `stitch` → `stitch-substrate` (crates.io's
  `stitch` is an unrelated 2018 crate) and its version aligned to the
  project version, 0.4.1. The library name stays `stitch`, so
  `use stitch::` is unaffected.
- CI now runs four legs: the Python suite, Rust tests plus the
  cross-language harness, the C conformance battery, and the Lean build
  with its axiom gate.
- The repository's canonical home moved from `SuperInstance/stitch` to
  `patchwork-quilt/stitch` (2026-11-02).

## [0.4.1] - 2026-10-27

The tightening patch. No stitch added, no wire change.

### Changed

- Booking helpers now **enforce** the SPEC numeric policy:
  `book_send`/`book_receive` refuse non-finite floats (`NaN`, `±Infinity`,
  anywhere in the payload) with `ValueError` before any write or
  signature, leaving the ledger byte-identical. Readers are deliberately
  unpoliced — `payload_hash`, `agree`, and audit treat legacy content
  exactly as in 0.4.0. This is the only behaviour change: payloads that
  were previously bookable now refuse.

### Added

- CI (GitHub Actions): Python suite + install smoke, cargo tests, and the
  cross-language harness on every push. Suite: 181 tests.

## [0.4.0] - 2026-10-27

The R&D release: three research tracks run against v0.3 — byte-exactness
fuzz, performance, signed bookings — adopted on the strength of their
measurements. Suite: 174 Python tests, 10 Rust tests.

### Added

- **Signed bookings** (opt-in): `book_send`/`book_receive` accept
  `secret_key=`; `agree`/`agreed_state`/`agree_n`/`agreed_state_n` accept
  `booking_keys=` enforcement. New refusal reasons `unsigned_booking` and
  `bad_signature` — an impostor ledger now refuses when keys are
  enforced. Keyless behaviour is byte-identical to v0.3. Known limit,
  recorded honestly: verbatim replay of a genuine signed booking still
  mints — a signature proves the booking is the tenant's statement, not
  that the file presenting it is that tenant's ledger.
- **Numeric policy** (SPEC): a fuzz study (1,884 payloads) showed floats
  were the untested half of the byte-exactness contract. The policy pins
  an auditor rule (hash the original bytes, never re-serialisations) and
  writer guidance; the Rust audit was made precision-safe
  (`serde_json` arbitrary-precision numbers), growing the cargo suite
  from 6 to 10 tests.
- **Three tutorials** (`tutorials/`): an accountable journal, an
  agent-to-agent handoff that mints only on agreement, and corroborated
  readings from independent observers — added with `AGENTS.md` after a
  cold test by a fresh agent.

### Changed

- **Fast Ed25519**: the pure-Python implementation's point arithmetic was
  replaced (extended coordinates + a fixed-base table) — signatures are
  byte-identical and the RFC 8032 vectors still pin, but signing is
  ~300x and verification ~80x faster, removing the per-attestation
  audit cliff.

## [0.3.0] - 2026-10-27

Adopted from the Z.ai (GLM) v0.3 draft after independent assessment, with
fixes. Suite: 54 → 150 tests. The v0.3 features are Python-first; the
Rust crate is unchanged from v0.2.

### Added

- **N-party agreement**: `agree_n()` / `agreed_state_n()` returning
  `AgreementN`. Every SEND booked in any ledger must have a mirror
  RECEIVE in some *other* ledger, all bookings must agree on the payload
  hash, and every ledger must carry at least one booking. For N=2 it
  reduces exactly to `agree`.
- **Tenant budget declarations**: `declare_budget()` writes a `BIND` of
  kind `"tenant"` recording the per-tenant budget in the ledger itself;
  the audit reads declared budgets instead of assuming one global
  budget. Declarations are **position-aware** (an adoption patch): a
  declaration governs entries from its own position onward, never
  earlier ones — the draft's position-blind audit let a later, larger
  declaration excuse an earlier overspend; a regression test pins the
  refusal.
- **Pure-Python Ed25519** (RFC 8032) in the `stitch.crypto` subpackage,
  stdlib-only via `hashlib.sha512`, wired in opt-in: `attest` writes a
  real signature when passed a secret key, `audit_ledger` verifies when
  passed public keys (`attest_keys`). RFC 8032 §7.1 vectors are pinned
  as tests and the implementation was cross-checked against OpenSSL in
  both directions. Without keys on either side, entries and verdicts are
  byte-identical to v0.2. Key-to-identity binding remains out of band.

### Changed

- **Non-ASCII encoding pin**: all `json.dumps` calls that produce hashed
  lines or payload commitments now use `ensure_ascii=False`, matching
  Rust's raw UTF-8 output (41 new byte-exactness tests).

### Deprecated / breaking

- Ledgers and payload hashes created under v0.2 with non-ASCII content
  do not verify under the encoding pin (they never matched the Rust side
  either). ASCII content is fully bidirectional.

## [0.2.0] - 2026-10-26

The agreement half: v0.1 recorded and verified; it could not agree. The
layer adds no new stitch and no new entry fields — agreement is built
entirely from the existing wire format. Suite: 54 tests.

### Added

- **Exchange bookings**: `book_send` / `book_receive` record an exchange
  as a `BIND` of kind `"exchange"` carrying the payload hash (SHA-256 of
  the canonical JSON) — the payload itself is not recorded; the hash is
  the commitment both sides must match. Booking costs no γ.
- **Foreign-ledger audit as library API**: `audit_ledger()` returns a
  structured verdict — entry count, chain status (or the first broken
  index), conservation-law replay, the head hash, and the tenant found.
- **Bilateral minting**: `agree()` / `agreed_state()` mint shared state
  only where two sovereign ledgers agree, with distinct refusal verdicts
  (`chain_broken`, `law_violated`, `peer_mismatch`, …).
- **Head-hash anchor**: `verify_anchor()` compares a ledger against a
  previously pinned head hash, closing v0.1's tail-truncation blindness
  for exchanges agreed before the cut.
- The audit was also ported to Rust; minting stayed Python-first.

## [0.1.0] - 2026-10-26

Initial public release: the v0.1 draft, verified, fixed, and made honest
(see Provenance in README.md).

### Added

- The 11 stitches — `bind`, `link`, `effect`, `view`, `tick`, `attest`,
  `delegate`, `contest`, `merger`, `revoke`, `withdraw` — over
  append-only, hash-chained JSONL ledgers (FNV-1a 64-bit chain).
- The conservation law: each tenant's writes are charged as KL surprise
  against a per-tick budget, γ + η ≤ log₂3 by default.
- The Python reference implementation and a Rust skeleton.
