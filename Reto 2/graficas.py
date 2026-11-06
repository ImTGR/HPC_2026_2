"""Genera figuras estaticas del benchmark. Dependencia opcional: matplotlib."""
import csv
import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "resultados"


def main():
    with (OUT / "resumen.csv").open(encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    plt.rcParams.update({"font.size": 11, "axes.spines.top": False,
                         "axes.spines.right": False, "figure.dpi": 150})
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.3), layout="constrained")
    for ax, algo in zip(axes, ("dartboard", "needles")):
        selected = [r for r in rows if r["variant"] == "native" and
                    r["algorithm"] == algo and r["n"] == "10000000"]
        static = sorted([r for r in selected if r["mode"] == "omp" and
                         r["schedule"] == "static"], key=lambda r: int(r["threads_actual"]))
        ax.plot([int(r["threads_actual"]) for r in static],
                [float(r["speedup"]) for r in static], "o-", label="OpenMP static")
        for schedule, marker in (("dynamic", "s"), ("guided", "^")):
            r = next(r for r in selected if r["mode"] == "omp" and r["schedule"] == schedule)
            ax.scatter(int(r["threads_actual"]), float(r["speedup"]), marker=marker,
                       s=55, label=f"OpenMP {schedule}", zorder=3)
        ax.plot([1, 12], [1, 12], "--", color="#999999", label="Ideal S=p")
        ax.set(title=algo.capitalize(), xlabel="Hilos", ylabel="Speedup frente a serial")
        ax.set_xticks([1, 2, 4, 6, 12])
        ax.grid(alpha=.2)
    axes[0].legend(fontsize=9)
    fig.suptitle("10 millones de muestras · O3 -march=native · medianas de 5 ejecuciones")
    fig.savefig(OUT / "speedup.png", bbox_inches="tight")
    plt.close(fig)

    fig, axes = plt.subplots(1, 2, figsize=(11, 4.3), layout="constrained")
    labels = ["Dartboard", "Needles"]
    for mode, offset, color in (("serial", -.18, "#2874A6"), ("buffered", .18, "#D68910")):
        values = [float(next(r for r in rows if r["variant"] == "native" and
                  r["algorithm"] == a and r["n"] == "10000000" and r["mode"] == mode
                  )["median_peak_rss_kib"])/1024 for a in ("dartboard", "needles")]
        axes[0].bar([i+offset for i in range(2)], values, width=.36,
                    label="Sin arreglos" if mode == "serial" else "Dos arreglos", color=color)
    axes[0].set_xticks(range(2), labels)
    axes[0].set(ylabel="Pico RSS (MiB)", title="Memoria del proceso · N=10 millones")
    axes[0].legend(fontsize=9)
    status = json.loads((OUT / "perfil_estado.json").read_text(encoding="utf-8"))
    phases = status["instrumented_phases"]
    gen = [p["median_generation_s"] for p in phases]
    ev = [p["median_evaluation_s"] for p in phases]
    axes[1].bar(labels, gen, label="Generación y escritura", color="#2874A6")
    axes[1].bar(labels, ev, bottom=gen, label="Lectura y evaluación", color="#D68910")
    axes[1].set(ylabel="Tiempo (s)", title="Perfil de buffered · N=30 millones")
    axes[1].legend(fontsize=9)
    fig.suptitle("Memoria y profiling instrumentado · variante native, serial")
    fig.savefig(OUT / "memoria_perfil.png", bbox_inches="tight")
    plt.close(fig)
    print("Figuras guardadas en resultados/speedup.png y resultados/memoria_perfil.png")


if __name__ == "__main__":
    main()
