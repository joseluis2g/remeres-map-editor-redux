# VERIFY — save/load pipeline

Only evidence goes here. A claim without a number, hash or screenshot is not evidence.
Every row cites the method used.

## Reference setup

- Build: `build-vs2022/Release` (MSVC Release). The app reads `config.toml` **from the
  working directory**, and finds its assets relative to the executable, so run it from
  `build-vs2022/Release`.
- Reference map: `C:\Users\user\AppData\Local\Temp\opencode\mapref\world.otbm`
  (copy of the user's `world.otbm`, 53,252,872 B) + its sidecar XMLs.
  Never save into the user's server directory (C8).
- Harness: `C:\Users\user\.config\opencode\skills\redux-harness\scripts\RmeAuto.ps1`
  (`[Rme]::GetMain`, `ByTitle`, `Shot`, `Dismiss`, `PostMessage`, `SetWindowPos`).
- Menu command: `WM_COMMAND 0x0111`, `wParam = 6003` = Save
  (`MAIN_FRAME_MENU 6001 + ActionID`).

## Recipes

### 1. Load and wait until the map is ready

```powershell
$p = Start-Process rme.exe -WorkingDirectory $bd -ArgumentList "`"$map`"" -PassThru `
     -RedirectStandardOutput "$bd\run.log" -RedirectStandardError "$bd\run.err"
$main = [IntPtr]::Zero
while ((Get-Date) -lt $deadline) {
  $m = [Rme]::GetMain([uint32]$p.Id); if ($m -ne [IntPtr]::Zero) { $main = $m }
  if ($m -ne [IntPtr]::Zero -and [Rme]::Title($m) -match '\.otbm') { break }
  [void][Rme]::Dismiss([uint32]$p.Id)     # dismiss loader dialogs/warnings
  Start-Sleep -Milliseconds 250
}
```

Title contains the map name only after a successful load — use it, not a timer.

### 2. Capture dialog text (progress / stage names)

```powershell
# EnumWindows + EnumChildWindows, GetWindowText on visible children
$d = [Dlg]::ByTitle([uint32]$p.Id, 'Saving')
foreach ($t in [Dlg]::Texts($d)) { $t }
```

Poll every 20–40 ms to catch short stages. `wxGenericProgressDialog` exposes its
message as a child static text; screenshots (`[Rme]::Shot`) are the fallback.

### 3. Save and time it

```powershell
[void][Rme]::PostMessage($main, 0x0111, [IntPtr]6003, [IntPtr]::Zero)
# wait until a dialog titled 'Saving' appears, then until it disappears
```

Measure from the post to the dialog disappearing. **Do not** break early: a modal
dialog swallows `WM_COMMAND`, so posting during a load silently does nothing (this
produced an invalid byte-comparison run once).

### 4. Byte-identical comparison (determinism)

The binary must live in `build-vs2022/Release` (assets are resolved next to it).

1. keep the previous binary as `Release\rme-before.exe`;
2. fresh copy of the reference map for each run;
3. save with each binary, same input bytes;
4. `Get-FileHash -Algorithm SHA256` on every sidecar XML.

Compare multi-MB XML by hash, not by eye. A 2 KB delta in a 10 MB file is invisible
in a diff preview but is a behaviour change (see B4).

### 5. Drag test (modal input block)

```powershell
Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;
public class Mouse { [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
[DllImport("user32.dll")] public static extern void mouse_event(uint f,uint dx,uint dy,uint d,IntPtr e); }'
# caption centre -> left down -> move +320,+180 -> left up
```

Assert the frame rect (e.g. `2100,60 3700,960`) is unchanged after the drag.

### 6. Status bar / result line

Status field 0 is overwritten by the FPS tick within a frame. Screenshot immediately or
read the log line instead (`Map saved: …`, `Map loaded: …`). Crop the status strip with
`Graphics.CopyFromScreen` to keep the image small.

### 7. Peak memory

```powershell
$p = Get-Process -Id $pid; $p.PeakWorkingSet64   # after the save completes
```

Read after the save, before closing, on a freshly loaded map (same state each run).

### 8. Crash / cancel

- **Cancel**: post the save, then post `WM_COMMAND` to the dialog's Cancel button or
  send `BM_CLICK` to it; assert `Cancelling...` within 250 ms, then the canonical file
  parses and no `.tmp`/`~` remain.
- **Kill**: `Stop-Process -Id $pid -Force` mid-write; then load the canonical file and
  assert it is the previous generation. `~` + `.saving.txt` may remain (expected on
  the rename-away model, must not on the temp+replace model after T5).

## P2 — gzip level and stage timings (2026-10-19)

Branch `perf/save-compression-level`, binary `build-vs2022/Release/rme.exe`
(MSVC Release). Reference map copied to `%TEMP%\opencode\mapref\world.otbm`
(53,252,872 B, 19,899,768 tiles, 15.25 client assets). The user's map was
never written: every run loaded a fresh copy under `%TEMP%\opencode\p2`.

### Method

- One process per measurement, working directory `build-vs2022/Release`.
- `config.toml` under that directory carried `[editor] otbm_compression_level`
  (except the default run, which omitted it) and the 15.25 client path.
- A temporary harness (not committed) timed `EditorFactory::LoadFromFile`
  (`map_load_ms`, decompression + tile build) and `EditorPersistence::saveMap`
  (`save_ms`), then wrote `save_ok` and exited. Peak working set was sampled
  from `Process.PeakWorkingSet64` every 200 ms while the process was alive.
- Three save runs per level, each on a fresh copy; three load runs per level on
  the saved output. Median reported.
- `save_wall` is the real UI path measured from `WM_COMMAND 6003` to the
  `Saving` dialog disappearing.
- Size growth: the baseline is the level-6 output of the same build
  (53,252,946 B), not the input (53,252,872 B), because the header description
  carries the current build string.

### Results (median of 3 saves, 2–3 loads)

| level | save (`save_ms`) | output size | growth vs L6 | load (`map_load_ms`) | peak WS |
|---|---|---|---|---|---|
| 1 | 5.46 s | 58,997,799 B | +10.79 % | 2.97 s (n=2) | 4304.6 MB |
| 3 | 7.45 s | 56,202,290 B | +5.54 % | 2.47 s | 4304.1 MB |
| **4 (default)** | **6.22 s** | **54,553,112 B** | **+2.44 %** | **2.69 s** | **4304.1 MB** |
| 6 (baseline) | 12.40 s | 53,252,946 B | — | 2.61 s | 4305.2 MB |

Level 3 is fast enough but exceeds the 5 % size budget; level 1 exceeds it by
far. Level 4 is the fastest level that satisfies both bounds, so it is the
default. Level 5 (measured, not in the required table) takes 8.79–9.93 s, over
the 8 s target.

Real UI save with the default (level 4, `config.toml` without the key), three
runs: **6.41 s / 7.38 s / 7.35 s** from `WM_COMMAND 6003` to the dialog closing.
The same binary loading the level-4 output reaches the title in 4.44 s and logs
`Map loaded: world.otbm (18997668 tiles) in 3.8 s`, no errors.

### Format and client compatibility

- Only the gzip stream changed. Decompressed payload SHA256 is identical for
  levels 1, 4 and 6: `BA6E900F31361D12A5463F13AF8BABABAF84AB6A9B0E2822F39874BAC8B85705`.
- Sidecar XML SHA256 prefixes are identical across levels too
  (`world-house` `36044BF9…`, `world-monster` `500896A9…`, `world-npc`
  `A37125F8…`, `world-zones` `137C6D87…`).
- Every saved output loaded back in the editor (successful load runs, exit 0,
  no load errors); gzip is level-agnostic on read.

### Stage log lines (one per stage, `spdlog::info`)

Level 4, UI save: `tiles` 5527–6268 ms, `finalize` 0.6 ms, `spawns` 604–716 ms,
`houses` 30–37 ms, `zones` 1.5–1.9 ms, `waypoints (OTBM)` 0.003 ms.
Headless harness: `tiles` 5547–5619 ms, `spawns` 546–634 ms, `houses` 24–29 ms,
`zones` 2.1–2.5 ms.

The auxiliary writers used to poll the progress dialog on every 128th spawn
area; with 53,788 areas each poll pumped the UI, costing 9.9–12.0 s in the real
save (spawn stage). The poll is now gated to at most one event pump per 100 ms
(well inside the 250 ms cancel budget), which brought the same stage down to
0.60–0.72 s and houses from 273–343 ms to 30–37 ms.

### Peak memory and budget (V9)

Save peak working set, max of all runs: **4305.2 MB** (level 6); level 4:
4302.3–4304.1 MB. A load-only run also peaks at ~4304.7 MB, so the map +
client assets dominate and the spawn writer's two DOMs add no measurable peak
on this map. Recorded budget: **4608 MB (4.5 GiB)**; measured max is 302.8 MB
(7.0 %) below it. A revision that exceeds the budget needs a streaming writer.

## Evidence log

| date | metric | baseline | now | method |
|---|---|---|---|---|
| 2026-10-19 | M1 save total (L6, same build) | 14.4 s | 12.40 s | harness `save_ms`, 3 runs |
| 2026-10-19 | M1 save total (default L4) | 14.4 s | 6.22 s harness / 6.41–7.38 s UI | `save_ms` + `WM_COMMAND` → dialog gone |
| 2026-10-19 | output size (L4 vs L6) | 53,252,946 B | 54,553,112 B (+2.44 %) | `Get-Item` |
| 2026-10-19 | load (L4) | 6.8 s (baseline protocol) | 2.69 s map stage / 3.8 s `Map loaded` | harness + log line |
| 2026-10-19 | payload identity L1/L4/L6 | — | identical SHA256 | inflate + SHA256 |
| 2026-10-19 | peak WS save | unmeasured | 4305.2 MB (budget 4608 MB) | `PeakWorkingSet64` @200 ms |
| 2026-10-19 | spawn stage UI | 9.9–12.0 s (poll per 128 areas) | 0.60–0.72 s (100 ms gate) | stage log |
| 2026-10-19 | M2 spawn stage | 6.8 s frozen | 2.9–3.2 s moving | dialog text @25 ms + stage log |
| 2026-10-19 | M1 save total | 18.7 s | 14.2–14.4 s | post → dialog gone |
| 2026-10-19 | M3 load total | 6.6–7.1 s | 6.6–7.1 s | `Map loaded` log line |
| 2026-10-19 | M7 spawn XML determinism | unknown | identical | SHA256, same input, two binaries |
| 2026-10-18 | M4 closing dialog centred | off-monitor | centred, 2722,472–3078,547 | window rects |
| 2026-10-19 | V4 not draggable | yes (tool frame) | no | real mouse drag |
| 2026-10-19 | M6 partial canonical file | possible | **unmeasured** (T5) | kill mid-write |
| 2026-10-19 | M9 peak memory | **unmeasured** | 4305.2 MB save (budget 4608 MB) | `PeakWorkingSet64` (T13) |
| 2026-10-19 | M5 cancel latency | **unmeasured** | — | cancel click (T11) |

## Pitfalls seen during this work

- Modal dialog (load/save) swallows `WM_COMMAND`; posting a save during a load does
  nothing and looks like a fast save.
- Copying `rme.exe` outside `Release` breaks asset/version resolution → map load fails.
- Killing a process mid-save leaves `world.otbm~` + partial XML; the map copy must be
  discarded and re-created from the source.
- `wxGenericProgressDialog` repaints per `Update()`; a stage with one percent for
  6.8 s means the writer does not report, not that it is hung.
- Status field 0 is shared with the FPS tick; result lines are ephemeral.
- Two monitors: the editor may be on DISPLAY2 (1920..3840); centre checks must compare
  against the frame rect, not the primary screen.
