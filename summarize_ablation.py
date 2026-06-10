from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path


LINE_RE = re.compile(
    r"generation=(?P<generation>\d+), runtime=(?P<runtime>[0-9.]+)s, "
    r"best_objective=(?P<objective>[0-9.]+), note=(?P<note>.+)"
)


def parse_log(path: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    if not path.exists():
        return rows
    for line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        match = LINE_RE.match(line.strip())
        if match:
            rows.append(match.groupdict())
    return rows


def summarize_run(run_dir: Path) -> dict[str, object]:
    log_rows = parse_log(run_dir / "ga_progress.log")
    result_path = run_dir / "result.json"
    result = json.loads(result_path.read_text(encoding="utf-8")) if result_path.exists() else {}
    final_runtime = float(log_rows[-1]["runtime"]) if log_rows else None
    final_objective = float(log_rows[-1]["objective"]) if log_rows else result.get("best_objective")
    elitism_rows = [row for row in log_rows if row["note"].startswith("elitism")]
    gen50_before = next((row for row in log_rows if row["generation"] == "49" and row["note"] == "checkpoint"), None)
    gen50_after = next((row for row in log_rows if row["generation"] == "50" and row["note"].startswith("elitism")), None)
    first_elitism_extra = None
    if gen50_before and gen50_after:
        first_elitism_extra = float(gen50_after["runtime"]) - float(gen50_before["runtime"])
    return {
        "run": run_dir.name,
        "best_objective": final_objective,
        "runtime_s": final_runtime,
        "elitism_events": len(elitism_rows),
        "first_elitism_extra_s": first_elitism_extra,
        "population": result.get("population"),
        "generations": result.get("generations"),
        "elitism_top_k": result.get("elitism_top_k"),
        "ls_single_pass": result.get("ls_single_pass"),
        "elitism_final_only": result.get("elitism_final_only"),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--result-root", type=Path, required=True)
    args = parser.parse_args()
    summaries = [summarize_run(path) for path in sorted(args.result_root.iterdir()) if path.is_dir()]
    out_csv = args.result_root / "summary.csv"
    out_json = args.result_root / "summary.json"
    fieldnames = [
        "run",
        "best_objective",
        "runtime_s",
        "elitism_events",
        "first_elitism_extra_s",
        "population",
        "generations",
        "elitism_top_k",
        "ls_single_pass",
        "elitism_final_only",
    ]
    with out_csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(summaries)
    out_json.write_text(json.dumps(summaries, indent=2, ensure_ascii=False), encoding="utf-8")
    print(out_csv)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
