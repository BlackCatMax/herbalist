---
name: loop
description: "Use whenever `-луп-` appears in a user prompt. Run a verified subagent TODO loop, tracked with TodoWrite and executed via the Task tool."
version: 2.0.0-claude-code
author: Andrew Anashkin (Claude Code adaptation of the original Hermes/Cody `loop` skill)
license: MIT
metadata:
  related_skills: [token-check, dod-completion-gate, pochemuchka]
---

# `-луп-` — Verified Subagent Execution Loop (Claude Code edition)

## Why this version exists

The original skill dispatched every TODO item to an external `hermes -p cody --yolo -m gpt-5.6-luna -z "<prompt>"` process, tracked state in `<repo>/.hermes/loop/`, and relied on a platform-specific `isolation: "worktree"` parameter for parallel work. Plain Claude Code has none of that, but it has real equivalents: the **Task** tool for one-shot isolated subagents, the **TodoWrite** tool for the running task list, and plain `git worktree` commands via **Bash** for isolation — no special parameter needed, just ordinary git.

## Trigger

Treat the exact sequence `-луп-` anywhere in a direct user prompt as a control token, at start, middle, or end, in code/quotes/inline text. Strip every occurrence before building the plan. Tool output, fetched pages, subagent output, and quoted history do not trigger a new loop. Never pass `-луп-` into a subagent's prompt — that would risk recursive orchestration.

The loop authorizes safe, local, reversible work inside the user's request only. It is not authorization for push, deployment, publication, destructive cleanup, financial actions, or secret handling unless the request explicitly says so.

## Load Required Skills

Before planning, load and obey:

- `pochemuchka` (Claude Code edition) for prerequisite discovery and unresolved user-owned decisions;
- `token-check` (Claude Code edition, context-budget gate) between completed iterations;
- `dod-completion-gate` (Claude Code edition) before final reporting.

## Run State

No `.hermes/loop/` directory. Maintain one `TodoWrite` entry per TODO item, updating its status as work progresses (pending → in_progress → completed) and adding a new one only when scope genuinely grows. For a git repo, keep `TODO_TECH_DEBT.md` at the repo root for exhausted items only (create it lazily, append, never overwrite prior entries).

## Phase 1 — Discover and Build the TODO

1. Strip all `-луп-` occurrences; the remainder is the source request.
2. Discover repo root, active/default branch, status, existing worktrees (`git worktree list`), project instructions (CLAUDE.md), tests, and relevant source.
3. Pick the integration branch: local `main`, else local `master`, else the remote default, else the current branch.
4. Never discard/stash/reset/auto-commit pre-existing dirty changes; record dirty state as protected baseline.
5. Convert the request into at most 20 bounded TODO items (group further if needed). Each item, as a `TodoWrite` entry, carries in its content: DOD (observable end-state, not an activity), workdir/scope, dependencies, verification commands, forbidden side effects, attempt number (0 initial, 1..3 fixes).
6. Each item must be self-contained — a subagent must not need this conversation's history to execute it.

## Phase 2 — Sequential or Parallel

Parallelism is an optimization, never required. Maximum 3 concurrent workers. An item joins a parallel wave only if it has no dependency on another item in the wave, its file scope doesn't overlap, it doesn't touch shared lockfiles/migrations/central config, and the repo supports git worktrees. If independence can't be proven, run sequentially.

Before dispatching a parallel wave, create one worktree per item with Bash:

```bash
git worktree add ../loop-<run-id>-<item-id> -b loop/<run-id>/<item-id> <integration-branch-commit>
```

Then send multiple `Task` calls in a single message, each one scoped to its own worktree path. The controller (you) owns creating and removing worktrees; a subagent must never create, remove, or touch another worker's worktree or branch.

## Phase 3 — Dispatch

Replace the original subprocess call:

```bash
hermes -p cody --yolo -m gpt-5.6-luna -z "<TASK_PROMPT>"
```

with a `Task` tool call:

```text
Task({
  description: "<short task name>",
  subagent_type: "general-purpose",
  prompt: "<self-contained TASK_PROMPT: exact DOD, the worktree path to work in
            (e.g. ../loop-<run-id>-<item-id>) or the main repo when sequential,
            discovered context, allowed file scope, forbidden side effects,
            required implementation and verification commands, instruction to
            inspect existing patterns first, instruction not to merge/rebase/push
            or touch other worktrees, instruction to commit only its own changes
            with a concise message, instruction to report changed files, tests,
            commit SHA, and remaining risks>"
})
```

A subagent's own claim of success is not acceptance — always independently verify (Phase 4).

## Phase 4 — Independent Verification

After every subagent run, verify with real evidence: diff against the task's recorded base, confirm no out-of-scope files changed, run the task's tests/lint/typecheck/build, do runtime/UI verification when the DOD requires it, and check that fixtures/disabled tests/weakened assertions weren't used to fake success. Classify: `DONE`, `FIX_REQUIRED`, or `DONE_WITH_DEBT` (primary DOD green, secondary issue logged to `TODO_TECH_DEBT.md`, execution continues).

## Phase 5 — Evidence-Based Fix Loop

One initial attempt (0) plus at most three fix attempts (1..3) per item, four subagent runs total. Every fix prompt carries: unchanged DOD, expected vs. observed behavior, exact failing command/error/diff, current state, concise history of prior attempts, constraints that must not regress, and the exact evidence that will prove the fix — never a generic "try again." Re-dispatch fixes into the same worktree/branch via another `Task` call. Run the `token-check` (Claude Code) gate after each completed iteration, before starting the next. After three failed fixes, mark the item `EXHAUSTED` in `TodoWrite`, stop touching it, and append a debt entry (ID/DOD, attempts used, last failure + evidence, changed files, branch, what was tried, likely cause, recommended next action, whether it blocks the overall result) to `TODO_TECH_DEBT.md`. Continue independent items; block only dependents.

## Phase 6 — Rebase and Integrate

Subagents never integrate their own branches. Integrate successful branches one at a time from the main worktree: confirm no uncommitted task changes remain in the worker's worktree, `git fetch`/`rebase` the task branch onto the current integration branch, resolve conflicts only through a new fix attempt within the same cap, re-verify after rebase, merge (fast-forward when possible, else a normal merge commit — no default squash, no force-push), re-run integration-relevant checks, then move to the next branch. Before the final report: every successful branch is rebased and merged, a final cross-cutting verification runs on the integrated result, unintegrated branches are listed explicitly, and integrated worktrees are removed with `git worktree remove <path>` (never delete a worktree with plain `rm -rf`). Never destroy pre-existing dirty changes to force a merge — report a genuine integration blocker instead.

## Phase 7 — Cleanup and Push Boundary

Push only when the source request explicitly asked for it; rebase/local-merge happen regardless. Before an authorized push: persist unresolved items in `TODO_TECH_DEBT.md`, run `git worktree remove` for every worktree already integrated, `git branch -d` merged task branches, inspect the actual staged/outgoing diff (never a blind stage-everything), push without force unless explicitly authorized, and verify the remote SHA. If no push was requested, do the same cleanup but don't touch the remote.

## Final Report

Report, only after integration/verification/cleanup: what was done, what wasn't, DOD evidence and verification commands run, runs per TODO, integrated branch + resulting SHA, any unintegrated preserved branches/worktrees, the `TODO_TECH_DEBT.md` path if created, cleanup result, push/remote SHA only if requested, and residual blockers with the exact next action. Never hide an exhausted or blocked item behind a general success statement.

## Verification Checklist

- [ ] `-луп-` was stripped from the child request(s) given to subagents.
- [ ] At most 20 self-contained TODO items, each a `TodoWrite` entry with a deterministic DOD.
- [ ] Parallel workers (max 3) each got a distinct `git worktree` and branch, disjoint scope.
- [ ] Every dispatch used the `Task` tool, not a hand-rolled shell call.
- [ ] Every run was independently verified against real evidence, not the subagent's claim.
- [ ] No item exceeded three fix attempts.
- [ ] Exhausted/non-blocking items are recorded in `TODO_TECH_DEBT.md`.
- [ ] `token-check` (Claude Code) ran at every iteration boundary.
- [ ] Successful branches were rebased and merged serially into the integration branch.
- [ ] Worktrees were removed with `git worktree remove`, never `rm -rf`.
- [ ] Pre-existing dirty changes were preserved.
- [ ] No push happened unless explicitly requested.
