"""Benchmark repetible, ejecucion secuencial de procesos y orden aleatorio."""
import argparse
import csv
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import platform
import random
import statistics
import subprocess
import time

from verify import run, ENV

ROOT = Path(__file__).resolve().parent


def cpu_name():
    if os.name == "nt":
        import winreg
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                            r"HARDWARE\DESCRIPTION\System\CentralProcessor\0") as key:
            return winreg.QueryValueEx(key, "ProcessorNameString")[0].strip()
    return platform.processor()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sizes", nargs="+", type=int, default=[100000, 1000000, 10000000])
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--threads", nargs="+", type=int, default=[1, 2, 4, 6, 12])
    args = parser.parse_args()
    if args.repeats < 1 or min(args.sizes + args.threads) < 1:
        parser.error("Se requieren tamanos, hilos y repeticiones positivos")
    out = ROOT / "resultados"
    out.mkdir(exist_ok=True)
    threads = sorted(set(p for p in args.threads if p <= (os.cpu_count() or 1)))
    if not threads:
        parser.error("Ningun numero de hilos solicitado cabe en los procesadores logicos disponibles")
    configurations = []
    for variant in ("O2", "O3", "native"):
        for algo in ("dartboard", "needles"):
            for n in args.sizes:
                configs = [("serial", 1, "static"), ("buffered", 1, "static")]
                configs += [("omp", p, "static") for p in threads]
                configs += [("simd", p, "static") for p in sorted({1, max(threads)})]
                # Scheduling: contraste con mismo binario, N y equipo.
                configs += [("omp", max(threads), s) for s in ("dynamic", "guided")]
                for mode, p, schedule in configs:
                    configurations.append((variant, algo, mode, n, p, schedule))
    jobs = [(config, rep) for rep in range(args.repeats) for config in configurations]
    random.Random(2026).shuffle(jobs)
    # Un calentamiento por binario/algoritmo (procesos nuevos en cada medicion).
    for variant in ("O2", "O3", "native"):
        for algo in ("dartboard", "needles"):
            run(variant, algo, "omp", 1000000, max(threads))
    rows = []
    expected_hits = {}
    started = time.perf_counter()
    with (out / "mediciones.csv").open("w", newline="", encoding="utf-8") as f:
        writer = None
        for index, ((variant, algo, mode, n, p, schedule), rep) in enumerate(jobs, 1):
            begin = time.perf_counter()
            row = run(variant, algo, mode, n, p, 42, schedule)
            process_wall = time.perf_counter() - begin
            key = algo, n
            expected_hits.setdefault(key, row["hits"])
            if row["hits"] != expected_hits[key]:
                raise RuntimeError(f"Conteo diferente: {variant}, {row}")
            row = {"variant": variant, "repeat": rep + 1, **row,
                   "process_wall_s": f"{process_wall:.9f}"}
            rows.append(row)
            if writer is None:
                writer = csv.DictWriter(f, fieldnames=list(row))
                writer.writeheader()
            writer.writerow(row)
            f.flush()
            if index % 50 == 0 or index == len(jobs):
                print(f"{index}/{len(jobs)} mediciones ({time.perf_counter()-started:.1f} s)", flush=True)
    groups = {}
    for row in rows:
        key = tuple(row[k] for k in ("variant", "algorithm", "mode", "n", "threads_actual", "schedule"))
        groups.setdefault(key, []).append(row)
    summary = []
    for key, values in sorted(groups.items()):
        variant, algo, mode, n, p, schedule = key
        times = [float(v["wall_s"]) for v in values]
        median = statistics.median(times)
        if median <= 0:
            raise RuntimeError("Tiempo no medible: aumente N o use un reloj de mayor resolucion")
        ref = groups[(variant, algo, "serial", n, "1", "static")]
        serial = statistics.median(float(v["wall_s"]) for v in ref)
        summary.append(dict(variant=variant, algorithm=algo, mode=mode, n=n,
            threads_actual=p, schedule=schedule, repeats=len(values),
            median_s=median, min_s=min(times), max_s=max(times),
            stdev_s=statistics.stdev(times) if len(times) > 1 else 0,
            speedup=serial / median, efficiency=serial / median / int(p),
            samples_per_s=int(n) / median,
            median_cpu_s=statistics.median(float(v["cpu_s"]) for v in values),
            median_process_wall_s=statistics.median(float(v["process_wall_s"]) for v in values),
            median_peak_rss_kib=statistics.median(int(v["peak_rss_kib"]) for v in values),
            buffer_bytes=values[0]["buffer_bytes"],
            median_generation_s=statistics.median(float(v["generation_s"]) for v in values),
            median_evaluation_s=statistics.median(float(v["evaluation_s"]) for v in values)))
    with (out / "resumen.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(summary[0]))
        writer.writeheader()
        writer.writerows(summary)
    metadata = dict(recorded_at_utc=datetime.now(timezone.utc).isoformat(),
        platform=platform.platform(), cpu=cpu_name(), logical_cpus=os.cpu_count(),
        python=platform.python_version(), gcc=subprocess.check_output(["gcc", "--version"], text=True).splitlines()[0],
        parameters=vars(args), environment={k: v for k, v in ENV.items() if k.startswith("OMP_")},
        run_order_seed=2026, sample_seed=42, measurements=len(rows),
        notes="Tiempo kernel tras inicializar equipo OpenMP; process_wall_s incluye proceso y runtime. RSS es pico de todo el proceso. CPU de Windows tiene resolucion limitada.")
    (out / "entorno.json").write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")
    print("Resultados guardados en resultados/mediciones.csv y resultados/resumen.csv")


if __name__ == "__main__":
    main()
