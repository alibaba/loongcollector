#!/usr/bin/env bash
# Shared comment classification for epic-delivery triage.
classify_comment_body() {
  local body="$1"
  if [[ "$body" == *'[epic-delivery]'* && "$body" == *'from=agent'* ]]; then
    if [[ "$body" == *'action=required'* ]]; then
      echo "required"
    else
      echo "skip"
    fi
    return
  fi
  if [[ "$body" == *'**自检**'* ]] || [[ "$body" == *'[epic-delivery triage]'* ]]; then
    echo "skip"
    return
  fi
  echo "required"
}
