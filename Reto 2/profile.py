"""gprof serial: compilar primero con build.py. Conserva perfiles crudos."""
import argparse
import csv
import json
import os
from pathlib import Path
import shutil
import subprocess
import statistics

from verify import run

ROOT = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--samples", type=int, default=30000000)
    args = parser.parse_args()
    gprof = shutil.which("gprof")
    exe = ROOT / "build" / ("mc_profile.exe" if os.name == "nt" else "mc_profile")
    status = {}
    for algo in ("dartboard", "needles"):
        directory = ROOT / "resultados" / ("perfil_" + algo)
        directory.mkdir(exist_ok=True)
        (directory / "gmon.out").unlink(missing_ok=True)
        if not gprof:
            status[algo] = "gprof no disponible"
            continue
        result = subprocess.run([str(exe), algo, "serial", str(args.samples), "1", "42", "static"],
                                cwd=directory, capture_output=True, text=True, check=True)
        (directory / "ejecucion.csv").write_text(result.stdout, encoding="utf-8")
        if not (directory / "gmon.out").exists():
            status[algo] = "runtime no genero gmon.out"
            continue
        profile = subprocess.check_output([gprof, str(exe), "gmon.out"], cwd=directory, text=True)
        (directory / "gprof.txt").write_text(profile, encoding="utf-8")
        # Algunos runtimes MinGW generan un gmon sin muestras ni funciones.
        status[algo] = ("sin datos utilizables" if "no time propagated" in profile
                        else "perfil generado; revisar cobertura y resolucion")
        print(f"gprof {algo}: {status[algo]}")
    # Alternativa real y portable: instrumentacion de fases. Una repeticion
    # inicial se descarta; despues se registran 5. No se usa -pg aqui.
    rows = []
    summary = []
    for algo in ("dartboard", "needles"):
        run("native", algo, "buffered", args.samples)
        phases = []
        for rep in range(5):
            row = run("native", algo, "buffered", args.samples)
            rows.append({"repeat": rep + 1, **row})
            phases.append(row)
        gen = statistics.median(float(r["generation_s"]) for r in phases)
        ev = statistics.median(float(r["evaluation_s"]) for r in phases)
        wall = statistics.median(float(r["wall_s"]) for r in phases)
        summary.append(dict(algorithm=algo, n=args.samples, median_generation_s=gen,
                            median_evaluation_s=ev, median_wall_s=wall,
                            evaluation_fraction_of_phases=ev/(gen+ev)))
    with (ROOT / "resultados" / "perfil_fases.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (ROOT / "resultados" / "perfil_estado.json").write_text(
        json.dumps({"gprof": status, "instrumented_phases": summary}, indent=2), encoding="utf-8")
    print("Perfil instrumentado guardado en resultados/perfil_fases.csv")


if __name__ == "__main__":
    main()
