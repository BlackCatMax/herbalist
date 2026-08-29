---
name: pochemuchka
description: "Use before responding to every user request. Perform a strict prompt-completeness gate: safely retrieve available context, detect every unresolved ambiguity or alternative implementation, ask one consolidated question list, and wait for all answers before execution."
version: 2.0.0-claude-code
author: Andrew Anashkin (Claude Code adaptation of the original Hermes `pochemuchka` skill)
license: MIT
metadata:
  related_skills: [loop, dod-completion-gate]
---

# Pochemuchka — Prompt Completeness Gate (Claude Code edition)

## Why this version exists

The gating logic is unchanged. Two things don't carry over from the original: the "Hermes-wide Enforcement" section (installing the skill across Hermes profiles via a `SOUL.md` fallback) has no Claude Code equivalent and is dropped; and unlike a platform with a structured multi-choice question widget, plain Claude Code has none — every question goes to the terminal as text and the run waits for the person's next message. That's actually closer to the original's own approach than any GUI-based edition would be.

## Overview

Apply this gate before executing every user request, to prevent implementing an under-specified prompt, guessing missing preferences, or silently picking one of several reasonable implementations. If any relevant ambiguity remains, or more than one reasonable implementation exists, ask and wait. Execute only once the answers leave exactly one reasonable implementation.

## Required Sequence

### 1. Retrieve discoverable context

Before asking the user, do safe, non-mutating discovery wherever it can resolve uncertainty: read relevant files/configs/docs/repo state (including `CLAUDE.md` and any project instructions), search accessible session or project context, inspect existing implementation and conventions, check available tools/services/environment facts. Do not mutate files or state, publish, send messages, take financial actions, or run destructive commands during this phase.

### 2. Audit prompt completeness

Check: intended outcome/deliverable; scope and exclusions; target system/repo/environment/audience; required inputs and source of truth; constraints (compatibility, style, language, format); success criteria and verification method; allowed side effects and safety boundaries; implementation approach/trade-offs; rollout and persistence. A dimension is unresolved if missing, contradictory, or has multiple reasonable interpretations.

### 3. Ask one consolidated question list

If anything is unresolved: stop before planning or implementation, and ask all currently known questions at once, as plain text in the terminal:

```text
Перед выполнением нужно уточнить:

1. <question>
   - A: <option>
   - B: <option>
   Рекомендация: <option and brief reason>

2. <question>
```

Don't pad the list with ceremonial questions — completeness is the goal, not question count. Don't ask for anything already available through safe discovery. After asking, wait — don't begin partial implementation in parallel, and don't proceed on an assumed default.

### 4. Re-audit the answers

Map each answer to its question, identify anything unanswered, detect new ambiguity the answers introduced, and ask a follow-up batch if uncertainty remains. Don't interpret silence as approval of a default, and don't silently choose between remaining reasonable implementations.

### 5. Execute and verify

Only once the gate passes: execute using the resolved specification, verify against the agreed success criteria, and report results and any proven blockers without reopening settled decisions.

## Decision Rule

```text
ask questions when:
  relevant_context_is_missing OR number_of_reasonable_implementations > 1
proceed immediately only when both are false
```

A difference is relevant if it can change the deliverable, behavior, architecture, scope, risk, compatibility, cost, persistence, or verification result.

## Common Pitfalls

1. Choosing an obvious-seeming default when another reasonable implementation exists.
2. Asking before doing safe discovery first.
3. Starting implementation in parallel while waiting for answers.
4. Splitting known questions across turns instead of one consolidated batch.
5. Treating a partial answer as a complete one.
6. Confusing "reversible" with "unambiguous" — a reversible action can still implement the wrong requirement.
7. Re-asking for information already provided or retrieved, or already stated in `CLAUDE.md`.

## Verification Checklist

- [ ] Safe discovery was completed where useful, `CLAUDE.md`/project docs included.
- [ ] Outcome, scope, inputs, constraints, and success criteria were checked.
- [ ] Side effects and persistence were checked.
- [ ] No relevant context is missing.
- [ ] Exactly one reasonable implementation remains.
- [ ] Every clarification question was answered.
- [ ] No preference was silently inferred.
