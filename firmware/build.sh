#!/usr/bin/env bash

rm firmware.bin
particle compile photon . --saveTo firmware.bin
