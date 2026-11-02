# Security

Stitch is an accounting primitive, not a complete security boundary. Its
verdicts are useful only when the reader is precise about what was
checked, which optional key checks were enabled, and what the holder of
a ledger file could have changed outside stitch.

The normative semantics are in [SPEC.md](./SPEC.md), especially
“Agreement (v0.2)”, “The head-hash pin (anchor)”, “Ed25519 signatures
(v0.3)”, and “Signed bookings”. This document states the operational
security model that follows from them.

## What a ledger can establish

### Internal order and changes to the presented chain

Each entry's `h` is an FNV-1a 64-bit hash of that entry's canonical
content, seeded by the preceding entry's hash. `Ledger.verify` and the
auditor recompute the chain. Changing an entry without also changing the
dependent hashes breaks the chain at the first affected position.

FNV-1a is not a cryptographic hash and the chain is not keyed. A person
who controls the whole file can rewrite it and recompute a new,
internally consistent chain. The chain is therefore a tamper-evident
canary for the file as presented, not standalone proof that a file is
the writer's original ledger.

The agreement anchor narrows that limit when its pin is kept separately
and trusted. An `Agreement` records a ledger's head hash and entry
count. `verify_anchor` can then distinguish an intact pinned prefix from
`truncated`, `rewritten`, `chain_broken`, and `missing`. If an attacker
can replace both the ledger and the stored pin, the anchor does not
help. See SPEC, “The head-hash pin (anchor)”.

### Conservation-law accounting

In the Python reference, an `EFFECT` that would make a tenant's
`γ + s` exceed the budget in force is rejected: no entry is appended and
the cell is not mutated. The Python foreign audit replays the recorded
`s` and `g` values, applies `TICK` resets, uses position-aware budget
declarations, and reports whether the presented ledger obeys the law
and, if not, the maximum excess.

That proves internal accounting consistency for the record being
audited. It does not prove that the recorded predictions or observations
were truthful, that a non-stitch writer followed the same rule while
creating a file, or that a budget declaration is honest. A tenant can
configure or declare a large budget; the declaration makes the choice
auditable, not justified. Use `audit_ledger`, not `verify()` alone,
when the conservation verdict matters: `verify()` checks the chain, not
the law.

### Agreement between records

For an exchange, `agree` first requires both presented ledgers to pass
chain and conservation audit. It mints only if a SEND booking and a
mirror RECEIVE booking name the same parties in opposite roles and
commit to the same payload hash. The payload itself is not in the
booking; the hash is the commitment.

Agreement therefore establishes that the two records agree on the
booked exchange. It does not establish that the payload moved, was read,
was correct, or corresponds to a real-world event.

### Authorship, only where keys are used

By default, a tenant identity (`ns`) is a string claim. An unsigned
`ATTEST` uses the historical SHA-256 placeholder in `sig`; it is not an
Ed25519 signature.

Two opt-in signature paths exist:

- **Signed attestations.** If `attest` is given a 32-byte Ed25519 secret
  key, it signs `"<tenant>:<cell>:<claim>"`. If `audit_ledger` is given
  `attest_keys`, it checks every `ATTEST` from each keyed tenant.
  `AuditVerdict.signatures_ok` is the signature judgement; it is
  separate from `AuditVerdict.ok`, which still means only chain plus
  conservation law.
- **Signed bookings.** If `book_send` or `book_receive` is given a
  secret key, the booking descriptor carries an Ed25519 signature over
  the domain-separated `stitch-booking/1` material: tenant, side,
  counterpart, exchange id, and payload hash. If an agreement call is
  given `booking_keys`, every booking by a keyed party is checked. A
  missing signature refuses as `unsigned_booking`; a non-verifying one
  refuses as `bad_signature`.

A valid booking signature proves that the holder of the secret key for
the auditor-supplied public key produced that booking's covered fields.
Forging a new booking, or changing a covered field, requires the key.

Signatures are checked only for parties for whom the auditor supplies a
key. Supplying no keys gives the keyless semantics. To hold every party
to signatures, supply a key for every party.

## What stitch does not prove

### Key-to-identity binding

A signature binds a statement to a key. Which key belongs to a named
person, agent, or machine is established out of band. Using the public
key's 64-character hex form as the tenant identity makes the ledger
self-describing, but anyone can claim that string as `ns`; only the
secret-key holder can sign as it.

Auditors must obtain identity-to-key mappings through a channel they
trust. Supplying the wrong public key for a genuine party correctly
produces `bad_signature`; stitch cannot know that the mapping, rather
than the booking, is wrong.

### Verbatim replay of a genuine signed booking

The booking signature does **not** cover the entry's timestamp, its
position, the ledger's head, or the containing ledger. The known
residual from SPEC, “What a booking signature does NOT prove”, is:

> A signature proves a booking is genuine, not that the file presenting
> it is that tenant's ledger.

A genuine signed booking can therefore be copied verbatim into a
different ledger claiming the same `ns`, and the signature will still
verify. A replay cannot change the payload hash, counterpart, side,
exchange id, or tenant string, and it cannot invent a booking the key
holder never made. It can present a real statement as if it belonged to
another file's history.

Deployments that need file provenance — *this* ledger, *this* head,
*this* history — need a mechanism outside signed bookings, such as a
separately witnessed or signed head-hash attestation. Stitch's agreement
anchor can detect changes relative to a trusted pin, but the pin itself
must be protected by the deployment.

### Freshness

Bookings have no nonce and stitch has no shared or trusted clock. A
signed booking from an earlier, genuine exchange remains a valid signed
statement when presented later. If an application needs freshness or
one-time use, it must define and enforce that policy above stitch.

### Truth, performance, and intent

Neither a valid chain, a lawful replay, a mint, nor a signature proves
that:

- an observation was accurate;
- a payload was delivered, read, or acted on;
- a payload hash corresponds to a particular real-world thing;
- a tenant's surprise inputs were honestly calculated; or
- a declared budget was appropriate.

Those are application and governance questions.

### Confidentiality and isolation

A ledger is plaintext JSONL. Exchange bookings record a payload hash
rather than the payload, but other stitches record their payloads and
claims in the file. Do not put secrets in ledger entries. A payload hash
is a commitment, not encryption.

Stitch does not encrypt ledgers, control who can read or write the file,
isolate tenants sharing a process, or make a file immutable. Filesystem
permissions, backup policy, and process isolation belong to the
deployment. “Append-only” is the library's write discipline, not a
property stitch can impose on someone who controls the file.

Keep one writer per ledger file. The Python reference flushes appends,
but two processes writing to the same file can interleave appends and
corrupt the chain.

### The Python Ed25519 implementation's stated limits

The Python implementation is pinned against RFC 8032 §7.1 TEST 1 and
TEST 2 and was cross-checked against OpenSSL in both directions for the
cases recorded in [VERIFICATION.md](./VERIFICATION.md). Its source also
records two limits plainly: verification does not screen public keys or
`R` points for the prime-order subgroup (no small-order check), and
point decompression mishandles the `x = 0`, sign-bit-1 edge instead of
rejecting it. The module itself says to patch those before it guards
anything valuable. A deployment with that risk profile should use a
reviewed native Ed25519 implementation producing and consuming the same
RFC 8032 bytes, and should not describe the Python verifier as having
checks it does not have.

Cell IDs are also not authentication: a cell ID uses the first 64 bits
of a SHA-256 construction, as SPEC states, and is an identifier rather
than a signature.

## Key handling

Stitch accepts raw 32-byte Ed25519 secret keys through its APIs and
returns signatures; it does not provide a key registry, key store,
rotation protocol, or revocation protocol.

- Generate a key with `stitch.crypto.Key.generate()` or
  `secret_key_bytes()` (which uses `os.urandom(32)`), or construct one
  with `Key.from_secret(...)` from an existing 32-byte secret.
- Keep the secret key out of ledgers, payloads, claims, source code,
  test fixtures, tickets, and logs. Store it in the deployment's secret
  store, or in a file outside the repository and ledger directories
  with access restricted to the signing identity.
- Give auditors public keys only. A verify-only integration can use
  `Key.from_public(...)`; it does not need the secret.
- Use a separate key per tenant. If several tenants share one secret,
  a signature proves only that *some holder of that shared key* made
  the statement.
- Distribute each identity-to-public-key mapping through an
  authenticated out-of-band channel and keep the mapping with the
  audit policy, not inside an untrusted ledger.
- Protect agreement pins separately from ledger copies when truncation
  or rewrite detection matters.
- If a secret key is compromised, stop trusting it: remove its public
  key from auditor key maps, tell relying parties out of band, and move
  the tenant to a new key. SPEC defines no revocation or rotation
  mechanism, and old signatures remain mathematically verifiable under
  the old public key. Stitch cannot distinguish statements signed before
  compromise from statements signed after it.

## Reporting a vulnerability

Do not open a public issue or pull request for a vulnerability, and do
not attach unredacted keys or sensitive ledger payloads to a report.

Use GitHub's private vulnerability reporting for this repository:
**Security → Advisories → Report a vulnerability**. Include the
affected commit or release, the implementation (Python, Rust, C99, or
Lean model), a minimal redacted reproduction, the verdict or bytes that
differ from SPEC, and the security property affected. If private
reporting is not available in GitHub's interface, contact the
maintainers privately and wait for a private channel before sending
exploit details.
