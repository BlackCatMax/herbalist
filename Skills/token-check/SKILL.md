---
name: token-check
description: "Use during iterative agent work (e.g. the `loop` skill). Check the session's remaining token budget after every completed iteration and pause below a safe floor."
version: 2.0.0-cowork
author: Andrew Anashkin (Cowork adaptation of the original Hermes `token-check` skill)
license: MIT
metadata:
  related_skills: [loop, dod-completion-gate]
---

# Token / Context Budget Check (Cowork edition)

## Why this version exists

The original skill ran `python3 "$HERMES_HOME/skills/.../check_token_quota.py"` against a live Codex `app-server` quota snapshot. There is no Codex CLI, no `$HERMES_HOME`, and no equivalent live quota endpoint in Claude Code / Cowork. What this environment does expose is a `<total_tokens>N tokens left</total_tokens>` system-reminder that appears periodically in the conversation. This edition gates on that instead, and is honest about being an approximation rather than a live, authoritative check.

## When to Use

Load before iterative work: a TODO/backlog processed item by item (see the `loop` skill), one subagent run followed by another, an implement → verify cycle repeated, or an orchestrator dispatching sequential worker tasks. An "iteration" is one completed TODO item, one completed `Agent` run, or one completed implement → verify cycle — not every internal tool call.

## Mandatory Gate

After each completed iteration, before starting the next:

1. Find the most recent `<total_tokens>` value from a system-reminder in the current context.
2. If no such reminder has appeared yet this session, treat the budget as unknown — continue, but note the gate could not run.
3. If the value is at or above the safe floor (default **300,000 tokens**; adjust per project/user preference), continue to the next iteration.
4. If it is below the floor, stop before dispatching further work.

This is fail-*open*, not fail-closed like the original: an unknown value does not block, because unlike a missing Codex process (a real error condition), a not-yet-seen reminder is normal early in a session.

## When Blocked

1. Finish reporting the just-completed iteration; do not start another.
2. State the last known remaining-token figure.
3. Ask the user explicitly whether to continue for one more iteration (or to wrap up / hand off remaining TODOs as a report).
4. Wait for the answer before dispatching, editing, or executing anything further.
5. If approved, do exactly one more iteration, then re-check. Approval does not carry over.

## Common Pitfalls

1. Estimating usage from conversation length instead of the actual reported figure.
2. Checking after every tool call instead of once per completed iteration.
3. Reusing a prior approval for more than one additional iteration.
4. Treating this as a hard, provider-verified quota the way the original Codex check was — it isn't; say so if the user asks.

## Verification Checklist

- [ ] The previous iteration is complete and its verification result is known.
- [ ] The most recent `<total_tokens>` figure (if any) was checked.
- [ ] Below-floor cases got an explicit stop-and-ask, not a silent continue.
- [ ] A prior approval was not reused for more than one extra iteration.
