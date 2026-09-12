# validation

this is code to validate different libraries when it comes to training utilities such as
- pyav
- decord

## Random access vs GOP — PyAV, CPU software decode (N=100)

Measured with `seek_test.sh` / `pyav_loader.py` (N=100 random seeks per video).
Decoder is PyAV software `h264` (CPU, `is_hwaccel=False`, `hwaccel=None`) —
`pyav_loader.py` uses plain `av.open()` with no hwaccel/NVDEC setup, and prints
`decoder: ... | is_hwaccel: ... | hwaccel: ...` at runtime to confirm.
Results from `out.txt` (converted to ms); GOP is the nominal value from the
filename; file size measured from `vids/test_decode_*.mp4` (MB = bytes / 1e6).

| video | GOP size | mean ± std random access (ms) | file size |
|---|---|---|---|
| `test_decode_250.mp4` | 250 | 1152.0 ± 622.4 | 187.8 MB (187766674 B) |
| `test_decode_100.mp4` | 100 | 547.0 ± 277.2 | 188.0 MB (187973340 B) |
| `test_decode_60.mp4` | 60 | 322.4 ± 149.7 | 188.2 MB (188179860 B) |
| `test_decode_50.mp4` | 50 | 276.1 ± 132.0 | 188.3 MB (188317911 B) |
| `test_decode_30.mp4` | 30 | 182.4 ± 77.3 | 188.8 MB (188800197 B) |
| `test_decode_20.mp4` | 20 | 134.0 ± 55.6 | 189.4 MB (189411043 B) |
| `test_decode_10.mp4` | 10 | 82.1 ± 25.6 | 191.4 MB (191408276 B) |
| `test_decode_3.mp4` | 3 | 49.1 ± 6.4 | 204.7 MB (204724990 B) |
