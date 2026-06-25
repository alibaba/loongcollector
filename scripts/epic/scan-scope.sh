#!/usr/bin/env bash
# Print Epic scope: sub-issues and linked PRs.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/lib/common.sh"

epic_parse_global_args "$@"
set -- ${EPIC_ARGV[@]+"${EPIC_ARGV[@]}"}

echo "Epic #${EPIC} (${REPO})"
echo "Sub-issues:"
epic_discover_sub_issues | sed 's/^/  #/'
echo "PRs:"
epic_discover_prs | sed 's/^/  #/' || echo "  (none)"
