---
name: security-check
description: Security scanning before commit/push. Checks for sensitive information like API keys and tokens.
---
# Security Check Rules

Before committing or pushing code, must check for sensitive information, especially API Keys and access tokens.

## What to Check

### API Keys and Access Tokens
- API Keys starting with `sk-` (OpenAI, Anthropic, Alibaba Cloud, etc.)
- Google API Keys starting with `AIzaSy`
- Public keys starting with `pk_`
- Other common API token formats

## Before Commit

### Run Check First
Run `bash .claude/skills/security-check/scripts/security_check.sh commit` to check the staging area for sensitive information. If it does NOT output `staging area is clear`, sensitive information was found.

### If Sensitive Information Found
1. **Immediately delete or replace**: Replace real API Keys with placeholders
2. **Use environment variables**: Move sensitive info to environment variables
3. **Add to .gitignore**: Ensure files with sensitive info are not committed
4. **Must refuse the commit/push action**

## Before Push

### Run Check First
Run `bash .claude/skills/security-check/scripts/security_check.sh push` to check each commit for sensitive information. If it does NOT output `all commits are clear`, sensitive information was found. The commit hashes are written to `task/sensitive_commits.txt`.

### If Sensitive Information Found
1. **Immediately delete or replace**: Replace real API Keys with placeholders
2. **Use environment variables**: Move sensitive info to environment variables
3. **Add to .gitignore**: Ensure files with sensitive info are not committed
4. **Must use the script below to clean history**

```bash
# Reset based on results in task/sensitive_commits.txt to avoid leaking commits
bash .claude/skills/security-check/scripts/security_reset.sh
```

5. **Must refuse the commit/push action**
