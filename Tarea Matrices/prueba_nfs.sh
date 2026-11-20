#!/usr/bin/env bash
set -euo pipefail

if [[ ${MATRICES_NFS_DIR:-} != /* || ! -d ${MATRICES_NFS_DIR:-} ]]; then
    printf 'Define MATRICES_NFS_DIR con una ruta absoluta compartida por todos los nodos.\n' >&2
    exit 1
fi

carpeta=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
source "$carpeta/prueba_mpi.sh"
ejecutar_prueba matrices_nfs.c 'MPI + NFS' "$@"
