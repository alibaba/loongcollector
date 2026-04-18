#!/usr/bin/env bash
# E2E 测试环境清理脚本
# 用法: bash .claude/skills/e2e/scripts/e2e-cleanup.sh [case_name]
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
E2E_DIR="$REPO_ROOT/test/e2e"
CASE_NAME="${1:-}"

echo "==> 停止并删除所有 Docker 容器..."
docker rm -f $(docker ps -aq) 2>/dev/null || true

echo "==> 清理 Docker 网络..."
docker network prune -f 2>/dev/null || true

echo "==> 清理运行时目录..."
rm -rf "$E2E_DIR/config" "$E2E_DIR/onetime_pipeline_config"
sudo rm -rf "$E2E_DIR/report" 2>/dev/null || rm -rf "$E2E_DIR/report" 2>/dev/null || true

if [[ -n "$CASE_NAME" ]]; then
    CASE_DIR="$E2E_DIR/test_cases/$CASE_NAME"
    if [[ -d "$CASE_DIR" ]]; then
        echo "==> 清理测试用例 $CASE_NAME..."
        rm -f "$CASE_DIR/testcase-compose.yaml"
        rm -f "$CASE_DIR/otel-export/"*.json 2>/dev/null || true
    fi
else
    echo "==> 清理所有测试用例的 testcase-compose.yaml..."
    find "$E2E_DIR/test_cases" -name "testcase-compose.yaml" -delete 2>/dev/null || true
fi

echo "==> 清理完成"
