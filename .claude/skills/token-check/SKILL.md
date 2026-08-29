---
name: token-check
description: "Use during iterative agent work (e.g. the `loop` skill). Check context pressure after every completed iteration and pause before it forces a lossy compaction mid-task."
version: 2.0.0-claude-code
author: Andrew Anashkin (Claude Code adaptation of the original Hermes `token-check` skill)
license: MIT
metadata:
  related_skills: [loop, dod-completion-gate]
---

# Context Budget Check (Claude Code edition)

## Why this version exists

The original skill ran `python3 "$HERMES_HOME/skills/.../check_token_quota.py"` against a live Codex `app-server` quota snapshot — a precise, numeric, provider-verified figure. Plain Claude Code has no equivalent live quota endpoint reachable from inside a running session, and no guaranteed numeric reminder appears automatically in-context the way it might on other platforms. Be upfront about this: this edition is a **weaker, best-effort gate** built from whatever signals your specific Claude Code setup actually surfaces, not a hard provider-verified check.

## When to Use

Load before iterative work: a TODO/backlog processed item by item (see the `loop` skill), one subagent run followed by another, an implement → verify cycle repeated, or an orchestrator dispatching sequential worker tasks. An "iteration" is one completed TODO item, one completed `Task` run, or one completed implement → verify cycle — not every internal tool call.

## Mandatory Gate

After each completed iteration, before starting the next:

1. Check for any context-pressure signal your environment actually exposes: a low-context or auto-compact warning the harness surfaced, the output of a `/context` (or equivalent) slash command if your setup allows invoking one mid-run, or a `PreCompact`-style hook firing.
2. If a signal is available and shows plenty of headroom, continue.
3. If a signal shows the session is close to forced compaction, or a compaction/context-warning has already fired mid-task, stop before dispatching further work — a lossy auto-compaction in the middle of a loop can silently drop task state.
4. If **no signal is queryable at all** in your setup (common in non-interactive/headless runs), fall back to a conservative substitute: pause and check in with the user every 5 completed iterations instead of relying on a percentage, and always pause immediately after any unusually large tool result (a huge diff, a long log dump, many full-file reads) that you did not need to keep in full.

This is fail-*open* on missing information (unlike the original's fail-closed default), because a genuinely unavailable signal is normal here, not an error condition the way a missing Codex process was.

## When Blocked

1. Finish reporting the just-completed iteration; do not start another.
2. State whatever concrete signal triggered the pause (the warning text, or "N iterations completed without a check-in").
3. Ask the user explicitly whether to continue for one more iteration, or to wrap up / hand off remaining TODOs as a report.
4. Wait for the answer before dispatching, editing, or executing anything further.
5. If approved, do exactly one more iteration, then re-check. Approval does not carry over.

## Common Pitfalls

1. Assuming a `/context`-style command is invocable mid-run everywhere — some setups only show it interactively to the person, not to you.
2. Treating a remembered pre-compaction figure as still valid after a compaction event has actually happened — re-establish from scratch.
3. Checking after every tool call instead of once per completed iteration.
4. Reusing a prior approval for more than one additional iteration.
5. Presenting this as a hard, provider-verified quota the way the original Codex check was — say plainly that it isn't, if asked.

## Verification Checklist

- [ ] The previous iteration is complete and its verification result is known.
- [ ] Whatever context-pressure signal is available in this setup was checked.
- [ ] No queryable signal → the 5-iteration check-in fallback was used instead of skipping the gate entirely.
- [ ] A pause-worthy signal got an explicit stop-and-ask, not a silent continue.
- [ ] A prior approval was not reused for more than one extra iteration.
