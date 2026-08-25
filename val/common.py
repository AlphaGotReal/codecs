import os
import subprocess

def get_fps(video_path):
    result = subprocess.run(
        [
            "ffprobe",
            "-v", "error",
            "-select_streams", "v:0",
            "-show_entries", "stream=avg_frame_rate",
            "-of", "default=noprint_wrappers=1:nokey=1",
            video_path,
        ],
        capture_output=True,
        text=True,
        check=True,
    )

    num, den = map(float, result.stdout.strip().split("/"))
    return num / den

def get_gop(video_path):
    cmd = (
        f'ffprobe -v error '
        f'-skip_frame nokey '
        f'-select_streams v:0 '
        f'-show_entries frame=pts_time '
        f'-of csv=p=0 "{video_path}" | head -2'
    )

    result = subprocess.run(
        cmd,
        shell=True,
        capture_output=True,
        text=True,
        check=True,
    )

    lines = result.stdout.strip().splitlines()

    if len(lines) < 2:
        return None

    lines[0] = lines[0].strip(',')
    lines[1] = lines[1].strip(',')

    dt = float(lines[1]) - float(lines[0])
    return get_fps(video_path) * dt
