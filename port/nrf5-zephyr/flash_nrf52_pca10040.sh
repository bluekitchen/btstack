#!/bin/sh
nrfjprog --eraseall -f nrf52
nrfjprog --program outdir/nrf52_pca10040/zephyr.hex -f nrf52
nrfjprog --reset -f nrf52
