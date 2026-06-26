#!/usr/bin/env bash
#
# dispatch-hook.sh — poll 自动派发时的默认处理器（可被 epic.env 的 DISPATCH_HOOK 覆盖）。
#
# 环境变量（由 epic.sh dispatch 注入）：
#   EPIC REPO EVENT_JSON COMMENT_ID KIND TARGET ACTION URL AUTHOR PREVIEW
#
# 退出码：
#   0 — 已处理（epic.sh 将自动 mark-handled）
#   1 — 处理失败（保留 pending，下轮重试）
#   2 — 需编排/执行 Agent 接手（保留 pending；默认行为）
#
set -euo pipefail

echo "[dispatch-hook] epic=#${EPIC} ${ACTION} ${KIND} #${TARGET} comment=${COMMENT_ID}"
echo "[dispatch-hook] url: ${URL}"
echo "[dispatch-hook] preview: ${PREVIEW:-}"

if [[ -n "${AGENT_CMD:-}" ]]; then
  bash -c "${AGENT_CMD}"
  exit $?
fi

echo "[dispatch-hook] 未配置 AGENT_CMD；编排 Agent 应 AGENT_TRIGGER → inbox → Task 派执行 Agent（勿在编排会话改代码）" >&2
exit 2
