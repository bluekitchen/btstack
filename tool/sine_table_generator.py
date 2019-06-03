#!/usr/bin/env python
import math
import sys

# 1HZ == 1 sine
# sample rate at 44100Hz, sine at 441Hz -> 441 sines in 44100 samples -> 44100/441 samples/sine

sine_array = '''
// input signal: pre-computed int16 sine wave, {sine_sample_rate} Hz at {sine_frequency} Hz  
static const int16_t sine_int16[] = {{'''

VALUES_PER_LINE = 10
sine_sample_rate = 44100
sine_frequency = 441

if __name__ == "__main__":
    usage = '''
    Usage: ./sine_table.py sine_frequency sine_sample_rate
    '''

    if (len(sys.argv) < 3):
        print(usage)
        sys.exit(1)
    
    sine_frequency = int(sys.argv[1])
    sine_sample_rate = int(sys.argv[2])

    if sine_frequency <= 0:
        print(usage)
        sys.exit(1)
    
    if sine_sample_rate <= 0:
        print(usage)
        sys.exit(1)

    sine_num_samples = sine_sample_rate/sine_frequency        
    
    print(sine_array.format(sine_sample_rate=sine_sample_rate, sine_frequency=sine_frequency))
    items = 0
    for sample in range(0,sine_num_samples):
        items = items + 1
        angle = (sample * 360.0) / sine_num_samples      
        sine = math.sin(math.radians(angle))
        rescaled = int(round(sine * 32767))
        print ("%6d, " % rescaled),
        if items == VALUES_PER_LINE:
            items = 0
            print
    
    print( "};")
