#! /usr/bin/env python3

import os
import sys

from IPython import embed
import argparse

DINOv3_P = os.path.join(os.path.dirname(__file__)[:-2], "encoders", "dinov3")
# sys.path.append(DINOv3_P)

import torch
from dinov3.hub.backbones import Weights

def main(args):
    model = torch.hub.load('facebookresearch/dinov3', "dinov3_vits16")
    return model

if __name__ == "__main__":

    parser = argparse.ArgumentParser()
    parser.add_argument("-c", "--cont", action="store_true")
    parser.add_argument("--gvar", default="gvar")
    args = parser.parse_args()

    _gvar = args.gvar
    globals()[_gvar] = main(args)

    if args.cont:
        embed()
