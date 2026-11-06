"""Pruebas de geometria, referencia independiente, OpenMP y estadistica."""
import csv
import io
import json
import math
import os
import re
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent
SUFFIX = ".exe" if os.name == "nt" else ""
ENV = {**os.environ, "OMP_DYNAMIC": "FALSE", "OMP_WAIT_POLICY": "PASSIVE"}
MASK = (1 << 64) - 1


def uniform(seed, word):
    z = (seed + 0x9E3779B97F4A7C15 * (word + 1)) & MASK
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK
    z ^= z >> 31
    return (z >> 11) * 2.0 ** -53


def reference(algo, n, seed):
    # Formula geometrica original, expresada independientemente del C nuevo.
    hits = 0
    for i in range(n):
        u, v = uniform(seed, 2*i), uniform(seed, 2*i+1)
        if algo == "needles":
            hits += u / 2 <= 0.5 * math.sin(v * math.pi / 2)
        else:
            x, y = 2*u-1, 2*v-1
            hits += x*x+y*y <= 1
    return hits


def run(variant, algo, mode, n, threads=1, seed=42, schedule="static"):
    cmd = [str(ROOT / "build" / ("mc_" + variant + SUFFIX)), algo, mode,
           str(n), str(threads), str(seed), schedule]
    result = subprocess.run(cmd, capture_output=True, text=True, check=True, env=ENV)
    return next(csv.DictReader(io.StringIO(result.stdout)))


def main():
    checks = 0
    for variant in ("O2", "O3", "native"):
        for algo in ("dartboard", "needles"):
            for n, seed in ((1, 0), (17, 42), (10003, 18446744073709551615)):
                expected = reference(algo, n, seed)
                for mode, threads in (("serial", 1), ("buffered", 1), ("omp", 1),
                                      ("omp", 2), ("omp", 4), ("simd", 4)):
                    row = run(variant, algo, mode, n, threads, seed)
                    assert int(row["hits"]) == expected, (variant, algo, mode, row, expected)
                    assert 1 <= int(row["threads_actual"]) <= threads
                    if mode in ("serial", "buffered"):
                        assert int(row["threads_actual"]) == 1
                    checks += 1
            expected = int(run(variant, algo, "serial", 100003)["hits"])
            for schedule in ("static", "dynamic", "guided"):
                for mode in ("omp", "simd"):
                    row = run(variant, algo, mode, 100003, 4, schedule=schedule)
                    assert int(row["hits"]) == expected
                    checks += 1
    exe = str(ROOT / "build" / ("mc_O2" + SUFFIX))
    for args in (("dartboard", "serial", "0", "1", "42", "static"),
                 ("needles", "omp", "10", "0", "42", "static"),
                 ("x", "serial", "10", "1", "42", "static"),
                 ("dartboard", "serial", "-1", "1", "42", "static"),
                 ("dartboard", "serial", "10", "2", "42", "static"),
                 ("needles", "omp", "10", "1", "18446744073709551616", "static"),
                 ("needles", "omp", "9223372036854775808", "1", "42", "static")):
        result = subprocess.run([exe, *args], capture_output=True, env=ENV)
        assert result.returncode == 2, args
        checks += 1
    # Varias semillas fijas, tolerancia amplia de 6 sigma, no exige error monotono.
    stats = []
    for algo in ("dartboard", "needles"):
        estimates = []
        for seed in (1, 7, 42, 2026, 987654321):
            row = run("native", algo, "serial", 1000000, seed=seed)
            pi, se = float(row["pi"]), float(row["se"])
            assert math.isfinite(pi) and abs(pi - math.pi) < 6 * se
            estimates.append({"seed": seed, "pi": pi, "se": se})
            checks += 1
        stats.append({"algorithm": algo, "samples_per_seed": 1000000, "estimates": estimates})
    originals = {}
    for name in ("Needle", "Dartboard"):
        originals[name] = subprocess.run(
            [str(ROOT / "build" / (name + "_original" + SUFFIX))],
            capture_output=True, text=True, check=True).stdout
        match = re.search(r"PI aproximado: ([0-9.]+)", originals[name])
        assert match, originals[name]
        # 6 sigma usando probabilidades teoricas; tolerancia estadistica,
        # no exige una cantidad fija de decimales a un metodo Monte Carlo.
        p = 2/math.pi if name == "Needle" else math.pi/4
        se = ((2/(p*p)) if name == "Needle" else 4) * math.sqrt(p*(1-p)/1000000)
        assert abs(float(match[1])-math.pi) < 6*se, originals[name]
        checks += 1
    output = {"checks_passed": checks, "statistics": stats, "original_programs": originals}
    (ROOT / "resultados").mkdir(exist_ok=True)
    (ROOT / "resultados" / "verificacion.json").write_text(
        json.dumps(output, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"{checks} comprobaciones correctas; evidencia en resultados/verificacion.json")


if __name__ == "__main__":
    main()
