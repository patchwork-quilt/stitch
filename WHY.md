# Why stitch?

Agents now change real state. They edit files, call tools, hand work to
other agents, record observations, and act on the results. The usual
record is an application log, a mutable memory object, or a JSON file
written as a side effect. Later, a basic question is hard to answer:

**Who changed this state, in what order, under what budget — and can
anyone else check the record without trusting the writer?**

Stitch is a small substrate primitive for that question. It does not run
the agent. It gives the agent's state changes a form that can be priced,
replayed, and compared.

## The idea

Each tenant — an agent, a device, a person, or another accountable
actor — appends its operations to its own JSONL ledger. There are 11
operations (“stitches”) for creating cells, writing observed facts,
linking cells, attesting to claims, delegating, contesting, merging,
revoking, and advancing time.

Every entry is hash-chained to the entry before it. State is therefore
not just the latest value in memory; it is a sequence that a foreign
auditor can replay later, from a copy, without asking the writer to
explain itself.

An `effect` is called with what an agent predicted and what it
observed. The difference is charged as *surprise*, and the entry records
the observed value for one key. For each tenant and tick, stitch
enforces the conservation rule

```text
γ + η ≤ C
```

where `γ` is surprise already dissipated, `η` is the headroom left, and
`C` is the tenant's per-tick budget. The default `C` is `log₂3`, about
1.585 bits.

This is not a physical law. It is an accounting rule: a tenant may be
wrong, but not in unlimited, unrecorded amounts during one tick. If the
next effect would exceed the budget, the reference writer rejects it
before appending anything. The tenant must tick, change its budget
configuration, or stop.

## Agreement without a central record

One ledger is one party's record. Shared state is not created by putting
both parties on the same server. Each side books its side of an exchange
into its own ledger, committing to the same payload hash. When the two
files later meet — by copy, sync, email, or any other transport —
`agree` derives shared state only if both ledgers pass chain and
conservation audit, the bookings mirror each other, and the payload
hashes match. Every failure has a distinct refusal reason.

That makes the primitive useful where agents are separate, offline, or
not operated by the same party. There is no central sequencer, global
clock, or coordinator in the write path.

## What stitch is not

Stitch is not an LLM, agent runtime, orchestrator, database, message
bus, or hosted service. It does not provide consensus, confidentiality,
access control, or multi-tenant isolation.

It also does not prove that a recorded observation was true, that a
payload actually moved, or that a named identity belongs to a particular
person or machine. Agreement proves that two *records* agree. The
security boundaries, including the limits of signatures and the
hash chain, are stated in [SECURITY.md](./SECURITY.md).

## The implementations and the proofs

Python is the complete reference implementation. Rust and C99 implement
scoped, tested parts of the same contract; [CONFORMANCE.md](./CONFORMANCE.md)
states the scopes exactly instead of implying parity.

Lean does not port the library. It formalises the agreement and
conservation core and machine-checks minting soundness and the
conservation law for that model, with its simplifications stated in
[`lean/README.md`](./lean/README.md).

## Where to go next

- **Do real work with it:** start with the three
  [tutorials](./tutorials/) — an accountable journal, an agent-to-agent
  handoff, and corroborated observers.
- **Implement it:** read [SPEC.md](./SPEC.md), the normative wire
  format and semantics, then [CONFORMANCE.md](./CONFORMANCE.md).
- **Judge the evidence:** [VERIFICATION.md](./VERIFICATION.md) records
  what was run, what passed, and what remains limited.
- **Use it safely:** read [SECURITY.md](./SECURITY.md) before treating a
  ledger, an audit verdict, or a signature as proof of more than it is.
