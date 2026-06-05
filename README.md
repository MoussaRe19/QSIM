## Phase 1 — Future Event List (FEL)

**Objective:** Implement a core priority queue component to handle event scheduling.

### Components

- **`EventNotice`**: Structure to store event metadata, timestamps, tracking indices, and validity states.
    
- **`FEL` Operations**: Core min-heap mechanics to support extraction, insertion, peek, and lazy cancellation.
    

### Key Requirements

|**Operation**|**Description**|
|---|---|
|**Insertion & Extraction**|Standard min-heap behavior prioritizing the earliest timestamp.|
|**Secondary Ordering**|Tie-breaking logic for identical timestamps (e.g., using unique IDs).|
|**Cancellation**|$O(1)$ invalidation strategy using node tracking.|

### Verification Criteria

- **Correct Ordering**: Elements are extracted in strict chronological order.
    
- **State Management**: Invalidated events are handled properly without breaking heap integrity.
    
- **Memory Safety**: Code must compile with strict warning flags and run without memory leaks or undefined behavior.

---

## Phase 2 — Simulation Clock & Context API

**Objective:** Wrap the Phase 1 Future Event List in a controlled engine layer that enforces time consistency and provides a safe interface for simulation models.

### Components

- **Kernel**: Holds simulation state (clock, FEL, event ID counter, stop flag).
- **Clock**: Advances time and enforces monotonic progression.
- **Context**: Only API exposed to models for reading time, scheduling, and canceling events.

### Key Rules

| Rule | Description |
|------|------------|
| Monotonic time | Time can only move forward. Any backward update halts execution. |
| No negative delay | Scheduling with negative delay is invalid and stops the engine. |
| API isolation | Models cannot access Kernel, Clock, or FEL directly. Only Context is allowed. |
| FEL invariant | All scheduled events must have timestamps ≥ current time. |

### Verification Criteria

- **Correct scheduling**: Events appear at `current_time + delay` for delay ≥ 0.
- **Backward time protection**: Any attempt to set time backward triggers a controlled failure.
- **Invalid scheduling rejection**: Negative delay scheduling triggers a controlled failure.
- **Safe cancellation**: Canceled events are ignored during execution without side effects.

---

## Phase 3 — Dispatch Loop, Termination Contract & Rescheduling

**Objective:** Implement the execution runtime by adding termination conditions, event rescheduling, and the main dispatch loop.

### Components

* **`TerminationCondition`**: Tagged union representing either a time limit or a predicate function used to stop execution.
* **`InterpretResult`**: Enumeration describing successful termination or runtime failure.
* **`dispatch_loop`**: Main event-processing loop responsible for executing scheduled events.

### Key Rules

| Rule                         | Description                                                                                      |
| ---------------------------- | ------------------------------------------------------------------------------------------------ |
| **Logarithmic Rescheduling** | `fel_reschedule` updates event priorities in $O(\log n)$ time using Phase 1 heap index tracking. |
| **Causality Protection**     | Events scheduled before the current simulation time trigger a runtime failure.                   |
| **Monotonic Advancement**    | The simulation clock advances only when processing the next event timestamp.                     |
| **Domain Agnosticism**       | The engine interacts only through registered predicates and the `Context` API.                   |

### Verification Criteria

* **Two-Phase Termination**: Termination conditions are evaluated before event extraction and after handler execution.
* **Reschedule Integrity**: Heap indexes remain synchronized during `sift_up` and `sift_down` operations.
* **Lazy Cancellation Sweep**: Canceled events are skipped and reclaimed safely during extraction.
* **Crash Isolation**: Invariant failures are captured through `setjmp`/`longjmp` without terminating the test harness.

---

## Phase 4 — PRNG & Distributions

**Objective:** Build a verified stochastic foundation isolated from model logic.

### Components

- **`prng`**: xoshiro256** state seeded via splitmix64 expansion.
- **`prng_uniform`**: Returns value in $(0, 1)$ with `DBL_MIN` guard against zero.
- **`Distribution`**: Tagged union with `.sample(prng)` dispatch.
- **`Exponential`**, **`Deterministic`**, **`Erlang`**: Concrete distribution types.

### Key Rules

| Rule | Description |
|------|-------------|
| Zero‑guard | `prng_uniform` never returns $0$ — prevents $-\mu \ln(0) = \infty$. |
| Reproducibility | Same seed produces identical sequence across runs. |
| Deterministic | Returns fixed value ignoring PRNG state. |
| Erlang construction | $\text{Erlang}(k, \mu_{\text{phase}})$ samples sum of $k$ exponentials; total mean $= k \cdot \mu_{\text{phase}}$. |

### Verification Criteria

- **Mean convergence**: $100{,}000$ samples from $\text{Exponential}(\mu=2.0)$ have sample mean within $\pm0.01$ of $2.0$, all samples $> 0$.
- **Reproducibility**: Two streams from same seed produce identical values.
- **Deterministic correctness**: Always returns the fixed value regardless of PRNG state.
- **Erlang mean**: $\text{Erlang}(k=3, \mu_{\text{phase}}=1.0)$ sample mean $\approx 3.0$.
