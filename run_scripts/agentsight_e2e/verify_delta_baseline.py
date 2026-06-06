#!/usr/bin/env python3
"""
Verify baseline-only AgentSight delta formula against SLS logs.

Formula (per row after chronological pairing within gen_ai.session.id):
  canon(omit_system(prev_in) + prev_out + delta) == canon(omit_system(cur))

Where prev_in/prev_out come from the previous HTTP row in the same session,
delta/cur from the current row. First row: delta == omit_system(cur).

Only baseline rows are checked (merged log: no event.name). See E2E_VERIFY.md.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import time
from typing import Any

from aliyun.log import LogClient

ENDPOINT = "cn-hangzhou.log.aliyuncs.com"
PROJECT = "xiaotian-config"
LOGSTORE = "xiaotian-logstore"

DEFAULT_CFG_CANDIDATES = [
    os.environ.get("LOONGCOLLECTOR_CONFIG", ""),
    os.path.join(
        os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
        ".vscode",
        "loongcollector_config.json",
    ),
    "/usr/local/loongcollector/conf/instance_config/local/loongcollector_config.json",
]


def resolve_config_path(explicit: str | None) -> str:
    if explicit:
        if not os.path.isfile(explicit):
            raise FileNotFoundError(f"config not found: {explicit}")
        return explicit
    for path in DEFAULT_CFG_CANDIDATES:
        if path and os.path.isfile(path):
            return path
    raise FileNotFoundError(
        "no loongcollector_config.json; set LOONGCOLLECTOR_CONFIG or use --config"
    )


def load_client(cfg_path: str) -> LogClient:
    with open(cfg_path, encoding="utf-8") as f:
        cfg = json.load(f)
    return LogClient(ENDPOINT, cfg["default_access_key_id"], cfg["default_access_key"])


def parse_messages(raw: str | None) -> list[Any] | None:
    if raw is None:
        return []
    text = str(raw).strip()
    if not text:
        return []
    try:
        val = json.loads(text)
    except json.JSONDecodeError:
        return None
    if not isinstance(val, list):
        return None
    return val


def omit_system(msgs: list[Any]) -> list[Any]:
    out: list[Any] = []
    for msg in msgs:
        if isinstance(msg, dict) and msg.get("role") == "system":
            continue
        out.append(msg)
    return out


def canon_msg(msg: Any) -> Any:
    if not isinstance(msg, dict):
        return msg
    out: dict[str, Any] = {}
    if "role" in msg:
        out["role"] = msg["role"]
    if "parts" in msg:
        out["parts"] = msg["parts"]
    return out


def canon_array(msgs: list[Any]) -> list[Any]:
    return [canon_msg(m) for m in msgs]


def stable_json(obj: Any) -> str:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"), ensure_ascii=False)


def is_baseline_row(contents: dict[str, str], *, agent_type: str) -> bool:
    event_name = contents.get("event.name", "").strip()
    if event_name:
        return False
    if contents.get("gen_ai.agent.type", "").strip() != agent_type:
        return False
    return bool(contents.get("gen_ai.output.messages", "").strip())


def dedupe_key(contents: dict[str, str]) -> str:
    rid = contents.get("gen_ai.response.id", "").strip()
    if rid:
        return f"rid:{rid}"
    sid = contents.get("gen_ai.session.id", "").strip()
    tid = contents.get("gen_ai.turn.id", "").strip()
    ts = contents.get("__time__", contents.get("_time_", "")).strip()
    return f"fallback:{sid}:{tid}:{ts}"


def row_signature(contents: dict[str, str]) -> str:
    """Dedup signature ignoring gen_ai.step.id (baseline has none; split mirrors may differ)."""
    skip = {"gen_ai.step.id", "__time__", "_time_", "__source__", "__topic__"}
    parts = []
    for k in sorted(contents):
        if k in skip:
            continue
        parts.append(f"{k}={contents[k]}")
    return "|".join(parts)


def dedupe_rows(rows: list[tuple[int, dict[str, str]]]) -> list[tuple[int, dict[str, str]]]:
    by_key: dict[str, list[tuple[int, dict[str, str]]]] = {}
    for ts, contents in rows:
        by_key.setdefault(dedupe_key(contents), []).append((ts, contents))

    out: list[tuple[int, dict[str, str]]] = []
    for group in by_key.values():
        if len(group) == 1:
            out.append(group[0])
            continue
        sigs: dict[str, list[tuple[int, dict[str, str]]]] = {}
        for item in group:
            sigs.setdefault(row_signature(item[1]), []).append(item)
        if len(sigs) == 1:
            out.append(min(group, key=lambda x: x[0]))
            continue
        # Same response.id but materially different payloads: keep earliest per signature.
        for sig_group in sigs.values():
            out.append(min(sig_group, key=lambda x: x[0]))
    out.sort(key=lambda x: x[0])
    return out


def scenario_label(
    index: int,
    cur: dict[str, str],
    prev: dict[str, str] | None,
    turn_index: int,
) -> str:
    if index == 0:
        return "first_in_session"
    if prev is None:
        return "first_in_session"
    cur_turn = cur.get("gen_ai.turn.id", "")
    prev_turn = prev.get("gen_ai.turn.id", "")
    if cur_turn and prev_turn and cur_turn != prev_turn:
        return "cross_turn_step_1"
    if turn_index >= 2:
        return f"same_turn_step_{turn_index}"
    return "cross_turn_or_step_2"


def verify_formula(
    prev_in: list[Any] | None,
    prev_out: list[Any] | None,
    delta: list[Any] | None,
    cur_in: list[Any] | None,
    *,
    first_in_session: bool,
) -> tuple[bool, str, str, str]:
    if cur_in is None:
        return False, "", "", "invalid gen_ai.input.messages JSON"
    if delta is None:
        return False, "", "", "invalid gen_ai.input.messages.delta JSON"
    if not first_in_session:
        if prev_in is None:
            return False, "", "", "invalid prev gen_ai.input.messages JSON"
        if prev_out is None:
            return False, "", "", "invalid prev gen_ai.output.messages JSON"

    if first_in_session:
        left = canon_array(delta)
        right = canon_array(omit_system(cur_in))
    else:
        left = canon_array(concat(omit_system(prev_in or []), prev_out or [], delta))
        right = canon_array(omit_system(cur_in))

    if stable_json(left) == stable_json(right):
        return True, stable_json(left), stable_json(right), ""
    return False, stable_json(left), stable_json(right), "canon mismatch"


def concat(*arrays: list[Any]) -> list[Any]:
    out: list[Any] = []
    for arr in arrays:
        out.extend(arr)
    return out


def build_sls_query(
    *,
    agent_type: str,
    session_id: str | None,
    marker: str | None,
) -> str:
    clauses = [f"gen_ai.agent.type: {agent_type}"]
    if session_id:
        clauses.append(f'gen_ai.session.id: "{session_id}"')
    if marker:
        # Quote tokens with spaces; bare tokens (e.g. E2E_HERMES_*) work unquoted.
        token = marker if " " not in marker else f'"{marker}"'
        clauses.append(token)
    return " and ".join(clauses)


def resolve_session_from_marker(
    client: LogClient,
    *,
    agent_type: str,
    marker: str,
    from_time: int,
    to_time: int,
) -> str | None:
    """Resolve gen_ai.session.id from SLS rows containing marker text."""
    query = build_sls_query(agent_type=agent_type, session_id=None, marker=marker)
    res = client.get_log(
        PROJECT,
        LOGSTORE,
        from_time,
        to_time,
        query=query,
        offset=0,
        size=50,
    )
    counts: dict[str, int] = {}
    for log in res.get_logs():
        contents = dict(log.get_contents())
        if not is_baseline_row(contents, agent_type=agent_type):
            continue
        sid = contents.get("gen_ai.session.id", "").strip()
        if sid:
            counts[sid] = counts.get(sid, 0) + 1
    if not counts:
        return None
    return max(counts, key=lambda k: counts[k])


def fetch_rows(
    client: LogClient,
    *,
    agent_type: str,
    session_id: str | None,
    marker: str | None,
    from_time: int,
    to_time: int,
    limit: int,
) -> list[tuple[int, dict[str, str]]]:
    query = build_sls_query(
        agent_type=agent_type,
        session_id=session_id,
        marker=marker,
    )

    rows: list[tuple[int, dict[str, str]]] = []
    offset = 0
    page = min(limit, 100)
    while len(rows) < limit:
        res = client.get_log(
            PROJECT,
            LOGSTORE,
            from_time,
            to_time,
            query=query,
            offset=offset,
            size=page,
        )
        logs = res.get_logs()
        if not logs:
            break
        for log in logs:
            contents = dict(log.get_contents())
            contents["__time__"] = str(log.get_time())
            if not is_baseline_row(contents, agent_type=agent_type):
                continue
            rows.append((log.get_time(), contents))
            if len(rows) >= limit:
                break
        if len(logs) < page:
            break
        offset += len(logs)
    rows.sort(key=lambda x: x[0])
    return rows


def run_checks(rows: list[tuple[int, dict[str, str]]]) -> tuple[list[dict[str, Any]], int, int]:
    by_session: dict[str, list[tuple[int, dict[str, str]]]] = {}
    for ts, contents in rows:
        sid = contents.get("gen_ai.session.id", "").strip() or "__unknown__"
        by_session.setdefault(sid, []).append((ts, contents))

    results: list[dict[str, Any]] = []
    passed = 0
    total = 0

    for sid in sorted(by_session):
        session_results, session_passed, session_total = run_checks_for_session(by_session[sid])
        for r in session_results:
            r["session_id"] = sid
        results.extend(session_results)
        passed += session_passed
        total += session_total
    return results, passed, total


def run_checks_for_session(rows: list[tuple[int, dict[str, str]]]) -> tuple[list[dict[str, Any]], int, int]:
    deduped = dedupe_rows(rows)
    results: list[dict[str, Any]] = []
    passed = 0
    turn_counts: dict[str, int] = {}

    for i, (ts, cur) in enumerate(deduped):
        prev = deduped[i - 1][1] if i > 0 else None
        turn_id = cur.get("gen_ai.turn.id", "")
        turn_counts[turn_id] = turn_counts.get(turn_id, 0) + 1
        turn_idx = turn_counts[turn_id]

        prev_in = parse_messages(prev.get("gen_ai.input.messages")) if prev else None
        prev_out = parse_messages(prev.get("gen_ai.output.messages")) if prev else None
        delta = parse_messages(cur.get("gen_ai.input.messages.delta"))
        cur_in = parse_messages(cur.get("gen_ai.input.messages"))

        ok, left, right, err = verify_formula(
            prev_in,
            prev_out,
            delta,
            cur_in,
            first_in_session=(i == 0),
        )
        label = scenario_label(i, cur, prev, turn_idx)
        entry = {
            "index": i,
            "scenario": label,
            "time": ts,
            "session_id": cur.get("gen_ai.session.id", ""),
            "turn_id": turn_id,
            "response_id": cur.get("gen_ai.response.id", ""),
            "passed": ok,
            "error": err,
            "left": left,
            "right": right,
        }
        results.append(entry)
        if ok:
            passed += 1
    return results, passed, len(deduped)


def main() -> int:
    default_agent = os.environ.get("AGENT_TYPE", "openclaw").strip() or "openclaw"
    parser = argparse.ArgumentParser(description="Verify baseline AgentSight delta formula in SLS")
    parser.add_argument("--session-id", help="gen_ai.session.id to filter (optional if --marker set)")
    parser.add_argument(
        "--marker",
        help='SLS search token, e.g. E2E_HERMES_* or TOOL_OK or "E2E baseline"',
    )
    parser.add_argument(
        "--agent-type",
        default=default_agent,
        help=f"gen_ai.agent.type filter (default: {default_agent!r} or AGENT_TYPE env)",
    )
    parser.add_argument(
        "--resolve-session",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="When --marker is set, resolve gen_ai.session.id from SLS (default: on)",
    )
    parser.add_argument("--minutes", type=int, default=15, help="Look-back window (default 15)")
    parser.add_argument("--from-time", type=int, help="Unix start (overrides --minutes)")
    parser.add_argument("--to-time", type=int, help="Unix end (default now)")
    parser.add_argument("--limit", type=int, default=200, help="Max SLS rows to fetch")
    parser.add_argument("--config", help="Path to loongcollector_config.json")
    parser.add_argument("--json", action="store_true", help="Emit machine-readable JSON")
    args = parser.parse_args()

    if not args.session_id and not args.marker:
        print("error: provide --session-id and/or --marker", file=sys.stderr)
        return 2

    agent_type = args.agent_type.strip()
    to_time = args.to_time or int(time.time())
    from_time = args.from_time if args.from_time is not None else to_time - args.minutes * 60

    cfg_path = resolve_config_path(args.config)
    client = load_client(cfg_path)

    session_id = args.session_id
    cli_session_id = args.session_id
    resolved_session_id: str | None = None
    if args.marker and args.resolve_session:
        resolved_session_id = resolve_session_from_marker(
            client,
            agent_type=agent_type,
            marker=args.marker,
            from_time=from_time,
            to_time=to_time,
        )
        if resolved_session_id:
            session_id = resolved_session_id
            if cli_session_id and cli_session_id != resolved_session_id and not args.json:
                print(
                    f"Note: CLI --session-id {cli_session_id!r} differs from SLS "
                    f"gen_ai.session.id {resolved_session_id!r}; using SLS value.",
                    file=sys.stderr,
                )

    rows = fetch_rows(
        client,
        agent_type=agent_type,
        session_id=session_id,
        marker=args.marker,
        from_time=from_time,
        to_time=to_time,
        limit=args.limit,
    )

    if not rows:
        msg = (
            f"No baseline {agent_type} rows in {LOGSTORE} "
            f"(session={session_id!r} marker={args.marker!r} window={args.minutes}m)"
        )
        if args.json:
            print(json.dumps({"passed": False, "error": msg, "checks": []}, ensure_ascii=False))
        else:
            print(f"FAIL: {msg}", file=sys.stderr)
        return 1

    results, passed, total = run_checks(rows)

    if args.json:
        print(
            json.dumps(
                {
                    "passed": passed == total and total > 0,
                    "agent_type": agent_type,
                    "cli_session_id": cli_session_id,
                    "resolved_session_id": resolved_session_id,
                    "fetched": len(rows),
                    "deduped": total,
                    "passed_count": passed,
                    "total_checks": total,
                    "checks": results,
                },
                ensure_ascii=False,
                indent=2,
            )
        )
    else:
        sid = session_id or (results[0].get("session_id", "?") if results else "?")
        sessions = sorted({r.get("session_id", "") for r in results})
        print(f"AgentSight baseline delta verify — agent={agent_type} session={sid}")
        if len(sessions) > 1:
            print(f"  ({len(sessions)} sessions matched; prefer --session-id for a single run)")
        print(
            f"Config: {cfg_path} | window: {from_time}..{to_time} "
            f"| fetched={len(rows)} deduped={total}"
        )
        for r in results:
            status = "PASS" if r["passed"] else "FAIL"
            sess_note = ""
            if len(sessions) > 1:
                sess_note = f" session={r.get('session_id', '')[:12]}…"
            line = (
                f"  {status} [{r['scenario']}] turn={r['turn_id']} "
                f"response.id={r['response_id']}{sess_note}"
            )
            print(line)
            if not r["passed"]:
                print(f"         error: {r['error']}", file=sys.stderr)
                if r["left"] and r["right"]:
                    print(f"         left:  {r['left'][:240]}...", file=sys.stderr)
                    print(f"         right: {r['right'][:240]}...", file=sys.stderr)
        print(f"Summary: {passed}/{total} checks passed")
        if passed != total:
            print("RESULT: FAIL", file=sys.stderr)
        else:
            print("RESULT: PASS")

    return 0 if passed == total and total > 0 else 1


if __name__ == "__main__":
    sys.exit(main())
