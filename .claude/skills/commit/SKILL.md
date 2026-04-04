---
name: commit
description: Write commit messages following to Conventional Commits standards.
---
# Commit Skill

Generate commit messages that follow the Conventional Commits specification.

## Format

```
type(scope): verb + object

{why is this change needed, what user/system impact it brings}

Fixes #{ISSUE_ID}
```

## Fields

- **type**: `feat | fix | docs | style | refactor | perf | test | chore | revert`
- **scope**: Optional. File/module/subsystem, e.g. `api`, `ui`, `auth`, `deps`
- **subject**: <= 50 characters, imperative mood, lowercase first letter, no period
- **body**: Each line <= 72 characters. Explain "what" and "why"
- **footer**: Optional. Link Issue / PR / Breaking Change

## Steps

1. Collect information by reading `git diff`. Skip if user already provided context.
2. Determine the commit type based on changes.
3. If changes span multiple scopes, use the core module as scope.
4. Extract added/modified/deleted functions, classes, interfaces for the subject.
5. If breaking change, add `BREAKING CHANGE:` to footer.
6. Present the complete commit message for user confirmation before executing `git commit`.

## Prohibited

- No meaningless descriptions like "update code", "fix bug", "wip"
- No subject or body lines exceeding 72 characters
- No issue links in the subject line
