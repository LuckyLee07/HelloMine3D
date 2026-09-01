# AL-A6 Architecture Lab Documentation Pipeline Contract v1

> Status: Frozen after AL-A6 verification
>
> Runtime behavior: documentation and verification graph only
>
> Authoritative task status: `docs/current/todolist.md`

## 1. Purpose and approved scope

AL-A6 turns the Architecture Lab tutorial convention into a maintained,
machine-checked pipeline. It does not create more tutorial files or write
chapters for unimplemented capabilities.

The pipeline has one authoritative living tutorial:

```text
docs/current/architecture-lab-tutorial.md
```

Every implemented Architecture Lab batch contributes one manifest row and one
non-empty section in that file. A row records the batch, owning Part, section
anchor and frozen contract/report evidence. Candidate batches remain only in
the roadmap and task ledger; they do not receive placeholder tutorial text.

## 2. Section contract

Each implemented section contains these seven logical headings:

```text
Problem
Naive Solution
Failure
Design Evolution
Implementation
Validation
Trade-offs
```

The headings are the stable reading path. Data structure, runtime flow, debug
method, benchmark and exercises are included under the nearest logical heading
only when the implemented batch has real material for them. They must not be
created as empty headings to satisfy a template.

One section may cover more than one batch when the batches are inseparable in
the explanation, but every completed batch still owns a distinct manifest row
and evidence path. An evidence path must be repository-relative, must not
escape the repository, and must exist.

## 3. Part and file lifecycle

- Track A is maintained under `Part 00`.
- Track B adds `Part 01` only with the first verified B batch.
- Track C adds `Part 02` only with the first verified C batch.
- Track D adds `Part 03` only with the first verified D batch.
- Integrated review adds `Part 04` only after at least one Track exit has real
  integration evidence.

The default remains one physical tutorial file. Splitting by Track requires a
separately approved documentation refactor, at least two completed Tracks, and
evidence that the single file has crossed a practical maintenance boundary.
The file must never be split by Sprint or by the seven logical headings.

## 4. Per-batch update workflow

Before a batch is closed, its owner must:

1. add or update the manifest row;
2. update one implemented section using the seven-heading contract;
3. link the frozen contract or verification report;
4. record the actual validation and trade-offs, including evidence that was
   not run or not claimed;
5. run the focused documentation-pipeline validator;
6. run the batch's behavioral gates and the complete Windows gate.

The tutorial summarizes why the architecture evolved. It does not duplicate
the task ledger, contract or report, and it must not promote a proposal into an
implemented fact.

## 5. Machine-checked boundary

`tools/validate_architecture_lab_documentation.ps1` verifies:

- exactly one living tutorial file exists;
- the manifest markers and rows are well formed and unique;
- AL-A0 through AL-A6 remain represented after this batch;
- every manifest batch has a completed state in the current task ledger;
- every evidence path exists and stays inside the repository;
- every declared Part and section exists and contains all seven non-empty
  logical headings;
- no Part exists without an implemented manifest row;
- the roadmap retains the one-file, no-empty-chapter and Core/Extended rules.

The validator is part of `scripts/verify_build.ps1`, so later implementation
batches cannot silently stop maintaining the tutorial.

## 6. Non-goals and preserved boundaries

AL-A6 does not:

- change Gameplay, save v12, resources, recipes, rendering or runtime code;
- create B1 state machines, jobs, cancellation, backpressure or spatial
  interest;
- create empty Track B/C/D/Integrated tutorial Parts;
- create one tutorial file per batch or per logical heading;
- turn `AI-01..AI-08=NOT_RUN` into PASS;
- claim human fun, aesthetics, comfort or physical input feel.

## 7. Acceptance

AL-A6 may be marked verified only when:

1. the living tutorial is reorganized into the Part/section model without
   losing the implemented A0-A5 design history;
2. the AL-A6 section explains the documentation pipeline through the same
   seven-heading structure it enforces;
3. positive validation passes and isolated negative fixtures prove malformed
   manifest, missing evidence, empty section and placeholder Part failures;
4. the validator is wired into the complete Windows verification graph;
5. VS2017/v141 Debug and Release complete gates and the isolated clean package
   pass with no runtime behavior change.

AI gameplay remains `NOT_RUN` unless executed separately through the active
package-only protocol. Human subjective experience remains `NOT_CLAIMED`.
