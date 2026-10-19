# P2 — speed goal

Loop unit: branch `perf/save-compression-level` · base `fix/save-progress-feedback`
(stacked on PR #1059; retarget when it merges).

## Goal

The save stops being dominated by compression: measure gzip levels on the reference
map, pick a default that meets **save ≤ 8 s** without a meaningful size penalty, and
record every stage's elapsed time so the next bottleneck is visible.

## Tasks

| id | task | acceptance |
|---|---|---|
| T7 | gzip level setting `OTBM_COMPRESSION_LEVEL` (1–9) + measured default | save ≤ 8 s, size growth < 5 % |
| T8 | per-stage ms logs (tiles, finalize, houses, zones, waypoints) | one line per stage, `spdlog::info` |
| T13 | peak working set budget for the merged spawn writer | measured, budget recorded in `VERIFY.md` |

## Success criteria

1. Table `level / save time / output size / load time` for levels 1, 3, 6 on the
   reference map (53,252,872 B, 19.0M tiles), published in `VERIFY.md`.
2. Save ≤ 8 s with the chosen default, load ≤ 7 s (no regression).
3. Output size growth < 5 % vs level 6; clients can read the file (gzip is
   level-agnostic, verify by loading the saved map).
4. Stage log lines exist for tiles/finalize/houses/zones/waypoints with ms.
5. Peak working set recorded for a save; a budget is written down (V9).

## Constraints / non-goals

- No format change; only the compression level may differ.
- Level must be configurable (users may prefer size over speed).
- Non-goal: block-parallel gzip, streaming XML writer, parallel spawn scan.

## Checks

```
cmake --build build-vs2022 --config Release --target rme --parallel 12
```

Three saves per level on a fresh copy of the reference map; map-load and save
timings measured in-process and the real save wall-clock from the progress
dialog, per the `VERIFY.md` protocol. Read `PeakWorkingSet64` while the process
is alive (before exit).

## Evidence

Measured 2026-10-19 on the reference map, `build-vs2022/Release`, three saves
per level from a fresh copy; load is the map stage (`EditorFactory::LoadFromFile`).
Full protocol and controls in [VERIFY.md](../VERIFY.md#p2--gzip-level-and-stage-timings-2026-10-19).

| level | save | size | growth vs L6 | load |
|---|---|---|---|---|
| 1 | 5.46 s | 58,997,799 B | +10.79 % | 2.97 s |
| 3 | 7.45 s | 56,202,290 B | +5.54 % | 2.47 s |
| **4 (default)** | **6.22 s** | **54,553,112 B** | **+2.44 %** | **2.69 s** |
| 6 | 12.40 s (this build) | 53,252,946 B | — | 2.61 s |

Real UI save with level 4: 6.41–7.38 s. Decompressed payload and sidecar XML
are byte-identical across levels. Save peak working set 4305.2 MB, budget
4608 MB (V9).

## Outcome

- T7: `config.toml` → `[editor] otbm_compression_level = 1..9`, default **4**
  (fastest level under the 5 % size bound; levels 1 and 3 exceed it).
- T8: `Save stage:` lines with ms for tiles, finalize, spawns, houses, zones
  and waypoints; also `Save: gzip compression level N`.
- T13: peak working set measured; budget 4608 MB recorded in V9 and VERIFY.
- Side fix: cancellation polling in the XML writers is gated to one event pump
  per 100 ms, which cut the UI spawn stage from 9.9–12.0 s to ~0.6–0.7 s (cancel
  latency stays under the 250 ms budget).

## Goal line

```
/goal Fase P2 (velocidad) del pipeline guardado/carga: docs/quality/save-load/goals/P2-speed.md, SPEC.md V11 y T7/T8/T13. Rama perf/save-compression-level desde fix/save-progress-feedback. Objetivo: el save deja de estar dominado por gzip nivel 6 (11.7 s de 14.4 s). --success "tabla medida en el mapa de referencia (53.252.872 B, 19.0M tiles) con nivel 1, 3 y 6: tiempo de save, tamano de salida y tiempo de carga, publicada en VERIFY.md" --success "con el default elegido el save queda <=8 s y la carga <=7 s, sin regresion" --success "crecimiento de tamano <5% vs nivel 6 y el mapa guardado carga correctamente" --success "logs por etapa con ms para tiles, finalize, houses, zones y waypoints" --success "pico de memoria del save medido (PeakWorkingSet64) y presupuesto escrito (V9)" --constraint "no cambiar el formato; solo el nivel de compresion, configurable" --constraint "la salida debe seguir siendo legible por los clientes" --constraint "no escribir en el mapa real del usuario; copias en %TEMP%" --non-goal "gzip en bloques paralelos" --non-goal "streaming writer sin DOM" --non-goal "paralelizar el escaneo de spawns" --check "cmake --build build-vs2022 --config Release --target rme --parallel 12" --max-turns 200 --max-minutes 1440
```
