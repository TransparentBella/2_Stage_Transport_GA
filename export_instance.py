from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "GA _for_SCFLP" / "src"
if str(SRC) not in sys.path:
    sys.path.insert(0, str(SRC))

from ga_scflp import GAConfig, GASolver, load_instance_from_excel


def write_vector(path: Path, rows: list[list[object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as fh:
        writer = csv.writer(fh)
        writer.writerows(rows)


def write_matrix(path: Path, matrix) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as fh:
        writer = csv.writer(fh)
        for row in matrix:
            writer.writerow([f"{float(v):.10f}" for v in row])


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Export a TSCFLP Excel instance for the C++ paper GA.")
    parser.add_argument("--data", required=True)
    parser.add_argument("--out-dir", required=True)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    instance = load_instance_from_excel(args.data)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    write_vector(
        out_dir / "plants.csv",
        [
            [instance.plant_ids[i], f"{instance.plant_capacity[i]:.10f}", f"{instance.plant_fixed_cost[i]:.10f}"]
            for i in range(instance.n_plants)
        ],
    )
    write_vector(
        out_dir / "depots.csv",
        [
            [instance.depot_ids[j], f"{instance.depot_capacity[j]:.10f}", f"{instance.depot_fixed_cost[j]:.10f}"]
            for j in range(instance.n_depots)
        ],
    )
    write_vector(
        out_dir / "customers.csv",
        [[instance.customer_ids[k], f"{instance.demand[k]:.10f}"] for k in range(instance.n_customers)],
    )
    write_matrix(out_dir / "plant_depot_cost.csv", instance.plant_depot_cost)
    write_matrix(out_dir / "depot_customer_cost.csv", instance.depot_customer_cost)

    solver = GASolver(instance, GAConfig(population_size=1, generations=0, flow_backend="ortools_mcf"))
    ch2 = solver.ch2()
    write_vector(out_dir / "ch2.csv", [[int(v) for v in ch2.tolist()]])

    print(f"exported {instance.name} to {out_dir.resolve()}")


if __name__ == "__main__":
    main()
