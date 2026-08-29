---
name: dod-completion-gate
description: "Use before every final response. Continue until the accepted DoD is complete."
version: 2.0.0-claude-code
author: Andrew Anashkin (Claude Code adaptation of the original Hermes `dod-completion-gate` skill)
license: MIT
metadata:
  related_skills: [pochemuchka, token-check, loop]
---

# DoD Completion Gate (Claude Code edition)

## Why this version exists

The logic is unchanged from the original — it never depended on Codex or Cody specifically. Only the vocabulary is updated: "Cody/Codex workers" becomes "subagents dispatched via the `Task` tool," and the source-of-truth tracker is the running **TodoWrite** list rather than a bespoke TODO file or a Cowork-specific task widget.

## Overview

Run this gate immediately before sending any final response, to prevent a premature status report while the accepted request, task list, or Definition of Done (DoD) still has executable work left. A finished milestone is not the final deliverable: if the user approved a five-item plan, item one being done is progress, not completion. Continue autonomously until the whole accepted scope is done or a genuine blocker requires the user.

## When to Use

Before every final response, especially when the run includes an explicit DoD/acceptance criteria/checklist/TODO/numbered plan; multiple tasks the user approved ("go ahead", "continue", or equivalent); an end-to-end request spanning implementation, persistence, QA, verification, commit/push, or rollout; or work delegated to subagents via the `Task` tool. Do not use this gate to expand scope beyond what the user actually accepted.

## Mandatory Pre-Final Audit

1. List every accepted task and every explicit DoD/acceptance criterion, reading the current `TodoWrite` list as the source of truth alongside the conversation.
2. Classify each as `DONE` (backed by real verification or a produced artifact), `BLOCKED` (needs user input, permission, credentials, unavailable infra, or a safety decision), or `PENDING` (executable now with available tools/permissions).
3. Check promised side effects: tests, runtime verification, persistence, UI checks, commit/push, reports, deployment, cleanup.
4. A local summary can never override unchecked accepted items still open in `TodoWrite`.

## Decision Rule

```text
if any accepted item is PENDING:
    do not send a final or intermediate status response; keep executing
elif any accepted item is BLOCKED:
    report one consolidated blocker request with completed evidence
else:
    send the final completion report
```

Don't ask "continue?" for work already approved, and don't treat an intermediate milestone as something to surface as if it were done.

## Continue Mode

Select the next dependency-ready accepted item, execute and verify it, apply the `token-check` (Claude Code) gate, re-run this audit, repeat until complete or blocked. A `token-check` block is a hard stop with its own one-iteration confirmation rule — never reinterpret it as ordinary approval to keep going. Context pressure alone is not a reason to stop early; keep `TodoWrite` current and re-read state fresh instead of relying on memory.

## Genuine Blockers

Valid only when continued work needs something that can't be safely obtained/done autonomously: explicit approval for destructive/public/financial/private/externally consequential action; missing credentials or user-owned info that read-only discovery can't retrieve; an unavailable required tool/service after reasonable alternatives; contradictory acceptance criteria needing a user decision; or a `token-check` block. When blocked, ask once with: what remains, why it can't proceed, the exact input/approval needed, and what's already verified. Long runtime or many remaining tasks are not blockers by themselves.

## Final Response Contract

Only claim completion when all accepted work is done: concise end-to-end result, evidence for the final gates/DoD, artifact paths/URLs/commit SHAs where relevant, intentionally excluded scope or residual non-blocking risk, and repo/runtime state when requested. Never present a partial milestone as "done."

## Common Pitfalls

1. Treating one finished item in a multi-item plan as the final result.
2. A polished summary that doesn't satisfy unchecked DoD items.
3. Asking the user to re-approve work already authorized.
4. Ignoring `TodoWrite` / the project's own tracker as source of truth.
5. Calling something DONE without test/runtime evidence.
6. Scope creep beyond the accepted request.
7. Skipping `token-check` during autonomous continuation.

## Verification Checklist

- [ ] Every accepted task was reconstructed from conversation and `TodoWrite`.
- [ ] Every explicit DoD/acceptance criterion is verified.
- [ ] No executable `PENDING` item remains.
- [ ] Every `DONE` claim has real evidence.
- [ ] Required side effects were verified.
- [ ] `token-check` (Claude Code) was applied between iterations where required.
- [ ] Any blocker is genuine and names the exact missing input.
- [ ] The response doesn't present a milestone as completion.
