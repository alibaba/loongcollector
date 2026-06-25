#!/usr/bin/env bash
# Post GitHub issue/PR comment with epic-delivery footer.
# Usage:
#   gh-comment.sh pr <number> --from human --role maintainer --action required <<'EOF'
#   body text
#   EOF
#   gh-comment.sh issue <number> --from agent --role self-review --action none -f body.md
set -euo pipefail

REPO="${EPIC_REPO:-alibaba/loongcollector}"
TARGET=""
NUMBER=""
FROM=""
ROLE=""
ACTION=""
BODY=""
BODY_FILE=""

usage() {
  cat <<'EOF'
Usage: gh-comment.sh <pr|issue> <number> --from <agent|human> --role <role> --action <none|required|fyi> [-f file|-]
EOF
  exit 1
}

[[ $# -ge 1 ]] || usage
TARGET="$1"
shift
[[ $# -ge 1 ]] || usage
NUMBER="$1"
shift

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo) REPO="$2"; shift 2 ;;
    --from) FROM="$2"; shift 2 ;;
    --role) ROLE="$2"; shift 2 ;;
    --action) ACTION="$2"; shift 2 ;;
    -f) BODY_FILE="$2"; shift 2 ;;
    -h|--help) usage ;;
    *) echo "unknown arg: $1" >&2; usage ;;
  esac
done

[[ -n "$FROM" && -n "$ROLE" && -n "$ACTION" ]] || usage

if [[ -n "$BODY_FILE" ]]; then
  BODY="$(cat "$BODY_FILE")"
elif [[ ! -t 0 ]]; then
  BODY="$(cat)"
else
  echo "provide body via stdin or -f" >&2
  exit 1
fi

FOOTER=$'---\n`[epic-delivery]` from='"${FROM}"' role='"${ROLE}"' action='"${ACTION}"
FULL_BODY="${BODY}"$'\n\n'"${FOOTER}"

TMP="$(mktemp)"
printf '%s' "$FULL_BODY" > "$TMP"
trap 'rm -f "$TMP"' EXIT

case "$TARGET" in
  pr)
    gh pr comment "$NUMBER" --repo "$REPO" --body-file "$TMP"
    ;;
  issue)
    gh issue comment "$NUMBER" --repo "$REPO" --body-file "$TMP"
    ;;
  *)
    echo "target must be pr or issue" >&2
    exit 1
    ;;
esac

echo "Posted to $TARGET #$NUMBER ($REPO) from=$FROM action=$ACTION"
