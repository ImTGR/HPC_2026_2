"""Compila con GCC/MinGW o GCC en Linux. Solo biblioteca estandar de Python."""
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent
BUILD = ROOT / "build"
EXE = ".exe" if os.name == "nt" else ""


def main():
    BUILD.mkdir(exist_ok=True)
    compiler = shutil.which("gcc")
    if not compiler:
        raise SystemExit("Se requiere GCC con soporte OpenMP (MinGW-w64 en Windows).")
    common = [compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
              "-fopenmp", "-ffp-contract=off", "-fno-math-errno"]
    variants = {"O2": ["-O2"], "O3": ["-O3"],
                "native": ["-O3", "-march=native"],
                "profile": ["-O2", "-g", "-pg", "-fno-inline"]}
    commands = []
    for name, flags in variants.items():
        cmd = common + flags
        if name in ("O2", "O3", "native"):
            cmd += [f"-fopt-info-vec-all={BUILD / ('vector_' + name + '.txt')}"]
            (BUILD / ("vector_" + name + ".txt")).unlink(missing_ok=True)
        cmd += [str(ROOT / "montecarlo.c"), "-o", str(BUILD / ("mc_" + name + EXE)), "-lm"]
        if os.name == "nt":
            cmd += ["-lpsapi"]
        subprocess.run(cmd, cwd=BUILD, check=True)
        commands.append(subprocess.list2cmdline(cmd))
    for source in ("Needle", "Dartboard"):
        cmd = [compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Wpedantic",
               str(ROOT / (source + ".c")), "-o", str(BUILD / (source + "_original" + EXE)), "-lm"]
        subprocess.run(cmd, check=True)
        commands.append(subprocess.list2cmdline(cmd))
    (BUILD / "comandos.txt").write_text("\n".join(commands) + "\n", encoding="utf-8")
    print("Compilacion terminada. Comandos y diagnosticos SIMD en build/.")


if __name__ == "__main__":
    main()
