#!/usr/bin/env python
import math
import sys

SAMPLES = 100
VALUES_PER_LINE = 10

print ('''
// input signal: pre-computed sine wave, 160 Hz at 16000 kHz
static const int16_t sine_int16[] = {''')
items = 0
for sample in range(SAMPLES):
    angle = (sample * 360.0) / SAMPLES
    sine = math.sin(math.radians(angle))
    rescaled = int(round(sine * 32767))
    print ("%6d, " % rescaled),
    items += 1
    if items == VALUES_PER_LINE:
        items = 0
        print
print( "};")
