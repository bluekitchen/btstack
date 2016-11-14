#!/bin/sh
nrfjprog --eraseall -f nrf51
nrfjprog --program outdir/nrf51_pca10028/zephyr.hex -f nrf51
nrfjprog --reset -f nrf51
