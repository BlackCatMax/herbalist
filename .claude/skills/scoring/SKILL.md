---
name: scoring
description: Score and safely compact working request context.
version: 1.1.0-claude-code
author: Andrew Anashkin (Claude Code pass over the original Hermes `scoring` skill — no functional changes were needed)
license: MIT
metadata:
  related_skills: [pochemuchka, token-check, dod-completion-gate]
---

# Scoring (Claude Code edition)

## Why this version exists

Unlike the other four skills in this family, `scoring` never called out to Codex, Cody, or any Hermes-specific tooling — it's a self-contained model-side discipline for triaging what's in the working context. It works unmodified in plain Claude Code. This pass only updates authorship and cross-references to the other Claude Code-adapted skills; the procedure itself is unchanged from the original.

Score a complete request and compact only the agent's own working context for subsequent reasoning and tool calls. This is model-side: it does not claim to reduce tokens already sent to the model or any provider billing.

## When to Use

- Run after `pochemuchka` returns a complete, unambiguous request that can proceed.
- Do not run when `pochemuchka` requires clarification.
- Apply once per direct user turn, not after every tool result, retry, or internal iteration.

## Quality Scores

Assess five integer scores from 0 to 100 (100 best): `P` precision/absence of ambiguity, `C` completeness of inputs and success criteria, `E` prompt efficiency, `R` relevance of supplied context, `K` context cost-efficiency. Weighted aggregate `P=.25, C=.20, E=.20, R=.20, K=.15`. Scores guide compaction but never override `pochemuchka` or safety rules.

## Protected Context

Always preserve: system/developer instructions and `CLAUDE.md`/project instructions; the current direct user request; explicit requirements, constraints, decisions, acceptance criteria, DoD; safety/privacy/permission/`token-check`/`loop`/`dod-completion-gate` rules; source-of-truth declarations; names, IDs, paths, commands, code, URLs, citations, timestamps, exact output formats needed by the task; tool-call/result relationships and verified evidence; credentials and private material (never expose or copy unnecessarily). Treat uncertain material as protected — never reinterpret authority based on proximity or wording.

## Eligible Material

Compact only demonstrably redundant material: exact repeated explanations/requirements; superseded progress reports already represented by a newer verified state; repeated examples adding no distinct constraint; historical context unrelated to the current request; verbose narration replaceable by an equivalent concise fact. Don't drop a detail just because it looks similar — when uncertain, keep it.

## Procedure

1. Confirm `pochemuchka` passed; if clarification is required, stop and let it respond without a scoring prefix.
2. Build an internal working brief: current outcome, scope, inputs, constraints, accepted decisions, safety boundaries, success criteria.
3. Compare the brief with supplied context; compact only proven-eligible material while preserving all protected meaning.
4. Estimate the reduction conservatively — a positive percentage only when specific redundant material was actually omitted; otherwise `0%`. Round down; never present it as provider billing savings.
5. Use the compact brief for the rest of the turn; don't modify persisted conversation history or fabricate missing context.
6. Start the final user-facing answer with exactly one line: `Промпт сокращен на X%` (replace X), then the normal answer after a newline.

## Output Rules

- The prefix is the first visible line after a successful `pochemuchka` pass, including `Промпт сокращен на 0%` when nothing was compacted.
- No prefix when asking clarification questions.
- Don't repeat the prefix after tools, retries, or intermediate reasoning.
- Don't expose the internal working brief or score table unless asked.
- Never claim the skill reduced the initial request's billed tokens.

## Fail-Safe Rules

If scoring, classification, or reduction is uncertain: preserve the original meaning, use `0%`, continue normally. If the request becomes ambiguous, return control to `pochemuchka` and ask one consolidated question list without the prefix.

## Verification

A working installation passes three fresh-session checks: (1) an ambiguous request produces `pochemuchka` questions and no scoring prefix; (2) a complete concise request starts with `Промпт сокращен на 0%`; (3) a complete request with obvious repeated context starts with a positive conservative percentage while retaining every distinct requirement.
