#!/bin/sh

# collect traces from auto-pts run (exclude chipset/zephyr)
lcov --capture --rc lcov_branch_coverage=1 --directory . --exclude "/Applications/*" --exclude "/Library/*" --exclude "/usr/*" --exclude "*/test/*"  --exclude "*/chipset/*" --output-file coverage-pts.info

# optional: download unit tests, merge and filter results like in ../Makefile

# generate html output
genhtml coverage-pts.info  --branch-coverage --output-directory coverage-pts
