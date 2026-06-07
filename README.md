# codecs

Dependencies:

**Ubuntu/Debian:**
```bash
sudo apt install libyaml-dev libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libsdl2-dev
```

**Arch Linux:**
```bash
sudo pacman -S libyaml ffmpeg sdl2
```

Build:
```bash
mkdir build && cd build
cmake ..
make
```

### Targets

| Target | Description | Usage |
|--------|-------------|-------|
| `player` | GUI video player (SDL2) | `./gui/player <video>` |
| `random` | Synthetic encode benchmark | `./test/random <output>` |
| `h26x` | H.264/H.265 transcode test | `./test/h26x <input> <output>` |
| `vpx` | VP8/VP9 transcode test | `./test/vpx <input> <output>` |
| `av1` | AV1 transcode test | `./test/av1 <input> <output>` |
| `h264_nvenv` | NVENC H.264 transcode | `./test/h264_nvenv <input> <output>` |
| `av1_nvenv` | NVENC AV1 transcode | `./test/av1_nvenv <input> <output>` |
| `test_hash` | Hash table unit tests | `./test/test_hash` |
