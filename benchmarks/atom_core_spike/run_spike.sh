#!/usr/bin/env bash
# Builds and runs the M3-T0 spike prototypes. Standalone, not part of the
# main CMake build (this is throwaway spike code, see
# docs/atom-representation-spike.md).
set -euo pipefail
cd "$(dirname "$0")"
for cxx in clang++ g++; do
  if command -v "$cxx" >/dev/null 2>&1; then
    echo "=== $cxx ==="
    "$cxx" -std=c++20 -O2 -Wall -Wextra -o /tmp/spike_bench_"$cxx" spike_bench.cc
    /tmp/spike_bench_"$cxx"
  fi
done
