#!/usr/bin/env bash

# La version NFS carga esta funcion para usar el mismo protocolo de prueba.
ejecutar_prueba() (
    set -euo pipefail
    export LC_ALL=C

    local fuente=$1 version=$2
    shift 2
    if (( $# == 0 )); then
        set -- mpirun -np "${NP:-4}"
    fi

    local carpeta compilacion ejecutable
    carpeta=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
    compilacion=$(mktemp -d "$carpeta/.prueba_matrices.XXXXXX")
    ejecutable=$compilacion/matrices
    trap 'rm -f -- "$ejecutable"; rmdir -- "$compilacion"' EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM

    "${MPICC:-mpicc}" -std=c11 -O3 -Wall -Wextra -Wpedantic \
        "$carpeta/$fuente" -o "$ejecutable"

    local orden repeticion salida fila
    local -a filas=()
    for orden in 500 1000 2000; do
        printf '%s | orden %d | 10 ejecuciones\n' "$version" "$orden" >&2
        for (( repeticion = 1; repeticion <= 10; ++repeticion )); do
            if ! salida=$("$@" "$ejecutable" "$orden" 500); then
                printf '%s | fallo con orden %d, ejecucion %d\n%s\n' \
                    "$version" "$orden" "$repeticion" "$salida" >&2
                return 1
            fi

            if ! fila=$(awk -v orden="$orden" -v rep="$repeticion" '
                /^Procesos utilizados: / { procesos = $3 }
                /^Semilla: / { semilla = $2 }
                /^Tiempo de multiplicacion paralela: / { calculo = $(NF - 1) }
                /^Tiempo de distribucion, calculo y recoleccion: / { total = $(NF - 1) }
                /^Huella de C: / { huella = $4 }
                END {
                    if (procesos !~ /^[1-9][0-9]*$/ || semilla !~ /^[0-9]+$/ ||
                        calculo !~ /^[0-9]+\.[0-9]+$/ || total !~ /^[0-9]+\.[0-9]+$/ ||
                        length(huella) != 16 || huella !~ /^[0-9a-f]+$/) exit 1
                    printf "%s,%s,%s,%s,%s,%s,%s\n", orden, rep, procesos, semilla,
                           calculo, total, huella
                }
            ' <<< "$salida"); then
                printf '%s | salida invalida con orden %d, ejecucion %d\n%s\n' \
                    "$version" "$orden" "$repeticion" "$salida" >&2
                return 1
            fi
            filas+=("$fila")
        done
    done

    printf '\n%s | valores de 1 a 500 | semilla automatica\n' "$version"
    printf 'orden,repeticion,procesos,semilla,calculo_s,total_s,huella_c\n'
    printf '%s\n' "${filas[@]}"
    printf '\nResumen %s\n' "$version"
    printf 'orden,repeticiones,calculo_promedio_s,total_promedio_s,total_min_s,total_max_s\n'
    printf '%s\n' "${filas[@]}" | awk -F, '
        {
            orden = $1
            total = $6 + 0
            if (++cuenta[orden] == 1) {
                ordenes[++cantidad] = orden
                minimo[orden] = maximo[orden] = total
            }
            if (total < minimo[orden]) minimo[orden] = total
            if (total > maximo[orden]) maximo[orden] = total
            suma_calculo[orden] += $5
            suma_total[orden] += total
        }
        END {
            for (i = 1; i <= cantidad; ++i) {
                orden = ordenes[i]
                n = cuenta[orden]
                printf "%s,%d,%.6f,%.6f,%.6f,%.6f\n", orden, n, suma_calculo[orden] / n,
                       suma_total[orden] / n, minimo[orden], maximo[orden]
            }
        }
    '
)

if [[ ${BASH_SOURCE[0]} == "$0" ]]; then
    ejecutar_prueba matrices.c MPI "$@"
fi
