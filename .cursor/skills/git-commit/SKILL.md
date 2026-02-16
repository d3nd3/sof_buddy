---
name: git-commit
description: Stages changes and commits with an auto-generated message derived from the diff. Use when the user wants to commit with a smart message, "git commit with message from changes", or to stage and commit in one go without typing the message.
---

# Git Commit (auto message)

## Workflow

1. **Stage** — `git add -A` (or stage specific paths if the user specified).
2. **Inspect** — Run `git status -sb` and `git diff --cached` (and `git diff --cached --stat` if helpful). If nothing staged, stop and report.
3. **Compose** — From the staged diff, write a single short commit message: concise, imperative, summarizing the change (e.g. "Add raw mouse cursor clip on focus loss", "Fix cbuf overflow logging in debug build"). Prefer conventional style only when it fits (e.g. `fix(menus): ...`).
4. **Commit** — `git commit -m "message"`.

Do not bump version or push unless the user asks.

## Optional: stage and show in one go

From repo root, to stage everything and print the diff for message composition:

```bash
./.cursor/skills/git-commit/scripts/stage_and_show.sh
```

Then compose the message from the output and run `git commit -m "..."`.

## Scope

- Default: stage all changes (`git add -A`). If the user names paths or says "only …", stage only those.
- Untracked files are included with `-A`; ensure `.gitignore` is correct.
