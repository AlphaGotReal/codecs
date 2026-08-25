#! /usr/bin/env python3

import os
import sys
import time

import tqdm
import random
import argparse
import av
import numpy as np

from common import get_gop, get_fps

TIMES = []

def profile(F):
    global TIMES
    def wrap(*args, **kwargs):
        global TIMES
        t0 = time.time()
        ret = F(*args, **kwargs)
        dt = time.time() - t0
        TIMES.append(dt)
        return ret
    return wrap

class PyavLoader:
    def __init__(self, video_f: str):
        self.video_f = video_f
        self.c = av.open(video_f)
        self.s = self.c.streams.video[0] # assume only video exists
        self.tb = self.s.time_base
        self.dur = float(self.s.duration * self.tb) \
            if self.s.duration is not None \
            else self.c.duration / av.time_base

    @profile
    def __getitem__(self, t: float): # sec
        tgt_pts = int(t / float(self.tb))
        self.c.seek(
            tgt_pts,
            stream=self.s,
            backward=True,
            any_frame=False,
        )

        for frame in self.c.decode(self.s):
            if frame.pts is None:
                continue
            ftime = float(frame.pts * self.tb)
            if ftime >= t:
                return frame

        return None

def main(args):

    l = PyavLoader(args.video)

    print(f"video FPS: {get_fps(args.video)}")
    print(f"video GOP: {get_gop(args.video)}")

    for r in tqdm.tqdm(range(int(args.n))):
        l[random.random() * l.dur]

    T = np.array(TIMES)
    print(f"mean random access time: {T.mean()}")
    print(f"std random access time : {T.std()}")

    return T

if __name__ == "__main__":

    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--cont", action="store_true")
    parser.add_argument("--gvar", default="gvar")
    parser.add_argument("--video")
    parser.add_argument("-n", default=10)
    args = parser.parse_args()

    _gvar = args.gvar
    globals()[_gvar] = main(args)

    if args.cont:
        embed()   
