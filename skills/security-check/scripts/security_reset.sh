#!/bin/bash
# 智能squash脚本 - 自动检测并清理包含敏感信息的commits
echo "🔍 开始清理包含敏感信息的commits..."

# 1. 检查task/sensitive_commits.txt文件是否存在且非空
if [ ! -f "task/sensitive_commits.txt" ] || [ ! -s "task/sensitive_commits.txt" ]; then
    echo "❌ 未找到敏感commits列表，请先运行push前检查"
    exit 1
fi

# 读取敏感commits列表
readarray -t sensitive_commits < task/sensitive_commits.txt

# 2. 如果发现敏感信息，进行智能squash
if [ ${#sensitive_commits[@]} -gt 0 ]; then
    echo "🚨 发现 ${#sensitive_commits[@]} 个包含敏感信息的commits，开始清理..."
    
    # 检查工作区是否干净
    git status --porcelain | read -r _ && {
        echo "⚠️  工作区或暂存区有未提交的更改，先进行stash..."
        git stash push -u -m "security-cleanup-backup-$(date +%Y%m%d-%H%M%S)"
        stashed=true
    } || stashed=false
    
    # 获取要reset的目标commit
    # 找到最早的敏感commit（数组最后一个），并获取其父commit
    earliest_sensitive="${sensitive_commits[${#sensitive_commits[@]}-1]}"
    parent_commit=$(git rev-parse --quiet "${earliest_sensitive}^")

    # 获取所有需要被squash的commits（从最早的敏感commit的parent到HEAD）
    if [ -n "$parent_commit" ]; then
        commits_to_squash=($(git rev-list --reverse "${parent_commit}..HEAD"))
    else
        # 如果没有parent，说明最早的敏感commit是root commit
        echo "⚠️  最早的敏感commit是仓库的第一个commit"
        commits_to_squash=($(git rev-list --reverse HEAD))
    fi
    
    if [ ${#commits_to_squash[@]} -eq 0 ]; then
        echo "❌ 无法确定要squash的commit范围"
        if [ "$stashed" = true ]; then
            git stash pop
        fi
        exit 1
    fi
    
    # 获取所有要重新提交的commits的信息
    echo "📝 提取所有commit messages..."
    all_commit_details=""
    main_subject=""
    
    for commit_hash in "${commits_to_squash[@]}"; do
        # 获取commit信息
        subject=$(git log --format=%s -n 1 "$commit_hash")
        body=$(git log --format=%b -n 1 "$commit_hash")
                
        # 主题行用第一个commit的主题
        if [ -z "$main_subject" ]; then
            main_subject="$subject"
        fi
        
        subject_marker="$subject"
        
        # 按GitHub squash格式添加commit详情
        if [ -n "$body" ]; then
            all_commit_details="${all_commit_details}* ${subject_marker}\n\n${body}\n\n"
        else
            all_commit_details="${all_commit_details}* ${subject_marker}\n\n"
        fi
    done
    
    # 创建GitHub风格的squash commit message
    new_message="${main_subject}\n\n${all_commit_details}"
    
    # 执行squash
    echo "🔄 执行squash操作..."
    if [ -n "$parent_commit" ]; then
        git reset --soft "$parent_commit"
    else
        echo "❌ 检测到最早敏感 commit 为 root commit，自动清理会涉及高风险历史重写，已中止。"
        echo "请手动执行更安全流程（例如 orphan 分支重建）后再提交。"
        if [ "$stashed" = true ]; then
            echo "⚠️  已为你恢复之前的工作区更改。"
            git stash pop
        fi
        exit 1
    fi
    
    # 显示需要手动清理的文件
    echo "📋 需要手动清理的文件："
    git status --porcelain | grep '^[AM]' | cut -c4-
    
    echo ""
    echo "✅ Squash完成！请执行以下步骤："
    echo "1. 手动清理上述文件中的敏感信息"
    echo "2. 运行: git add ."
    echo "3. 运行: git commit"
    if [ "$stashed" = true ]; then
        echo "4. 如需恢复之前的工作区更改: git stash pop"
    fi
    echo ""
    echo "📝 新的commit message预览："
    echo "────────────────────────────────────────"
    echo -e "$new_message"
    echo "────────────────────────────────────────"
else
    echo "✅ 未发现包含敏感信息的commits"
fi
