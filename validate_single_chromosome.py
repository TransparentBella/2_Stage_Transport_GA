from __future__ import annotations

import argparse
import csv
import subprocess
import sys
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
GA_SRC = ROOT / "GA _for_SCFLP" / "src"


def run_cpp(exe: Path, instance_dir: Path, chromosome: Path) -> float:
    if not exe.exists():
        raise FileNotFoundError(f"missing executable: {exe}")
    completed = subprocess.run(
        [
            str(exe),
            "--instance-dir",
            str(instance_dir),
            "--eval-chromosome",
            str(chromosome),
        ],
        check=True,
        text=True,
        capture_output=True,
    )
    return float(completed.stdout.strip().splitlines()[-1])


def run_ortools(instance_xlsx: Path, chromosome: Path) -> tuple[bool, float, int, int, int, int]:
    sys.path.insert(0, str(GA_SRC))
    from ga_scflp.data import load_instance_from_excel
    from ga_scflp.models import build_flow_evaluator

    instance = load_instance_from_excel(instance_xlsx)
    chrom = np.loadtxt(chromosome, delimiter=",", dtype=int)
    y = chrom[: instance.n_plants]
    z = chrom[instance.n_plants :]
    evaluator = build_flow_evaluator(instance, backend="ortools_mcf")
    feasible, objective = evaluator.evaluate(y, z)
    return feasible, objective, chrom.size, int(y.sum()), int(z.sum()), instance.n_plants + instance.n_depots


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate one chromosome on OR-Tools, C++ SSP, and Goldberg-style backends.")
    parser.add_argument("--instance-xlsx", type=Path, default=ROOT / "data_100200400.xlsx")
    parser.add_argument("--chromosome", type=Path, default=ROOT / "paper_cpp_ga_run" / "instances" / "data_100200400" / "ch2.csv")
    parser.add_argument("--ssp-instance-dir", type=Path, default=ROOT / "paper_cpp_ga_run" / "instances" / "data_100200400")
    parser.add_argument("--goldberg-instance-dir", type=Path, default=ROOT / "paper_cpp_goldberg_ga" / "instances" / "data_100200400")
    parser.add_argument("--ssp-exe", type=Path, default=ROOT / "GA _for_SCFLP" / "paper_cpp_ga" / "paper_ga_eval.exe")
    parser.add_argument("--goldberg-exe", type=Path, default=ROOT / "paper_cpp_goldberg_ga" / "paper_ga_goldberg.exe")
    parser.add_argument("--out", type=Path, default=ROOT / "paper_cpp_goldberg_ga" / "validation" / "single_chromosome_validation.csv")
    parser.add_argument("--tolerance", type=float, default=1e-6)
    args = parser.parse_args()

    feasible, ortools_obj, chrom_len, open_plants, open_depots, expected_len = run_ortools(args.instance_xlsx, args.chromosome)
    ssp_obj = run_cpp(args.ssp_exe, args.ssp_instance_dir, args.chromosome)
    goldberg_obj = run_cpp(args.goldberg_exe, args.goldberg_instance_dir, args.chromosome)

    rows = [
        {"backend": "ortools_mcf", "feasible": feasible, "objective": f"{ortools_obj:.6f}", "delta_vs_ortools": f"{0.0:.6f}"},
        {"backend": "cpp_ssp", "feasible": True, "objective": f"{ssp_obj:.6f}", "delta_vs_ortools": f"{ssp_obj - ortools_obj:.6f}"},
        {"backend": "goldberg_style", "feasible": True, "objective": f"{goldberg_obj:.6f}", "delta_vs_ortools": f"{goldberg_obj - ortools_obj:.6f}"},
    ]

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=["backend", "feasible", "objective", "delta_vs_ortools"])
        writer.writeheader()
        writer.writerows(rows)

    passed = feasible and chrom_len == expected_len and all(abs(float(row["delta_vs_ortools"])) <= args.tolerance for row in rows)
    print(f"chromosome={args.chromosome}")
    print(f"chrom_len={chrom_len} expected_len={expected_len} open_plants={open_plants} open_depots={open_depots}")
    for row in rows:
        print(f"{row['backend']}: objective={row['objective']} delta_vs_ortools={row['delta_vs_ortools']}")
    print(f"csv={args.out}")
    return 0 if passed else 2


if __name__ == "__main__":
    raise SystemExit(main())
