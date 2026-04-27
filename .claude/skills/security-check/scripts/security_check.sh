#!/bin/bash
set -euo pipefail

SENSITIVE_PATTERNS="(sk-[a-zA-Z0-9]|AIzaSy[a-zA-Z0-9]|pk_[a-zA-Z0-9]|ghp_[a-zA-Z0-9]|gho_[a-zA-Z0-9]|ghu_[a-zA-Z0-9]|ghs_[a-zA-Z0-9]|ghr_[a-zA-Z0-9])"
MODE="${1:-}"

if [ "$MODE" != "commit" ] && [ "$MODE" != "push" ]; then
    echo "Usage: $0 [commit|push]"
    exit 2
fi

if [ "$MODE" == "commit" ]; then
    # 检查暂存区中的 API Keys
    echo "checking staging area"
    if git diff --cached --no-prefix | grep '^+' | grep -E "$SENSITIVE_PATTERNS"; then
        echo "⚠️  staging area contains SENSITIVE information"
    else
        echo "✅  staging area is clear"
    fi
elif [ "$MODE" == "push" ]; then
    # 检查所有要推送的 commit
    is_clear=true
    upstream=$(git rev-parse --abbrev-ref --symbolic-full-name @{u} 2>/dev/null) || upstream="origin/main"
    mkdir -p task
    > task/sensitive_commits.txt  # 清空文件

    while read -r commit; do
        commit_hash=$(echo "$commit" | cut -d' ' -f1)
        echo "checking commit: $commit"
        if git show "$commit_hash" --no-commit-id --unified=0 | grep '^+' | grep -E "$SENSITIVE_PATTERNS"; then
            echo "⚠️  commit $commit contains SENSITIVE information"
            echo "$commit_hash" >> task/sensitive_commits.txt
            is_clear=false
        fi
        echo "---"
    done < <(git log "${upstream}"..HEAD --oneline)
    if [ "$is_clear" = true ]; then
        echo "✅  all commits are clear"
    fi
fi