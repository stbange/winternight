# Performance

The game must run smoothly on an average phone (design D14): **60 fps** with
the 95th-percentile frame ≤ 16.7 ms and the 99th percentile ≤ 33 ms.

## Reference devices

| Target | Device | Browser / OS |
|--------|--------|--------------|
| Android phone | Samsung Galaxy A54 | Chrome |
| iPhone | iPhone 13 | Safari |
| Desktop | RTX 3060 class GPU, 6-core CPU, 16 GB RAM, 1080p | Windows 11 + Chrome, Linux |
| Low-end desktop | Intel Iris Xe laptop | Windows 11 + Chrome |
| XR headset | Meta Quest 3 | Meta Quest Browser |

**Not measured yet:** none of the reference devices were available while
building milestone 1. Task 11.2 stays open until they are measured.

## Mobile preset budget per frame (D14)

| Budget | Limit | Prologue start, Mobile preset |
|--------|-------|-------------------------------|
| Draw calls | ≤ 150 | **90** |
| Triangles | ≤ 250k | **139k** |
| View radius | 5 chunk rings (16 m) + far scenery | 5 |
| Shadow map | 512² | 512², nearest ring casts |
| Particles | ≤ 2000 | 2000 cosmetic + gameplay pool |
| Internal resolution | 480×270 (auto-scales to 320×180) | 480×270 |
| Download | ≤ 15 MB compressed | about 1.2 MB (wasm 0.9 MB, js 0.2 MB, data 9 KB) |

Draw calls and triangles come from `winternight --benchmark --preset mobile`
(maximum over 900 frames at the start of the Prologue).

## Measurements on the build machine

The development container has no GPU. It renders with **Mesa llvmpipe**
(software rendering on the CPU), so its frame times say nothing about a real
GPU. They are only useful to compare presets with each other:

| Preset | p50 | p95 | p99 | Max draw calls | Max triangles |
|--------|-----|-----|-----|----------------|---------------|
| Mobile | 24.1 ms | 29.1 ms | 33.3 ms | 90 | 139k |
| Standard | 86.4 ms | 100.7 ms | 114.4 ms | 166 | 710k |

CPU-side costs measured natively (x86-64, -O2, single thread) for the whole
start area (11 × 11 columns of 16 chunks):

| Work | Time |
|------|------|
| Generating 1,936 chunks | 1.7 s (≈ 0.9 ms per chunk) |
| Lighting 121 columns | 1.1 s (≈ 9 ms per column) |
| Meshing all chunks at the Mobile detail levels | ≈ 0.55 s |
| Far scenery grid + mesh (3.2 m cells) | ≈ 0.1 s |

While walking, generation is budgeted per chunk (3 chunks per frame on
Mobile) and meshing per frame (3 ms on Mobile). Lighting runs when a column is
complete. That is the largest single spike (≈ 9 ms on desktop, probably 25–30 ms
on a phone). Moving lighting and meshing to worker threads (design D3) is
the next optimisation if the phone measurements show hitches.

## How to measure

```bash
./build/winternight --benchmark --preset mobile
```

On the web, add `?preset=mobile&stats` to the URL and press F3 (or use the
always-on overlay with `stats`) to see frame time, p95, draw calls and triangles.
