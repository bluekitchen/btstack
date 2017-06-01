#!/usr/bin/env python

import matplotlib.pyplot as plt
import csv

timestamp = list()
storage = list()
sbc_buffer_name = 'sbc_buffer.pdf'
with open('sbc-buffer.csv', 'rb') as csvfile:
	data = csv.reader(csvfile, delimiter=' ')
	for row in data:
		i = 0
		for cell in row:
			if i == 0:
				timestamp.append(int(cell))
				i = 1
			elif i == 1:
				storage.append(int(cell))
				i = 2
	
t = len(timestamp)

plt.plot(timestamp[0:t], storage[0:t], '.')
plt.xlabel('Time [ms]')
plt.ylabel('Queued SBC frames')
#plt.show()
plt.savefig(sbc_buffer_name)

print("\nCreated figure \'%s\', audio duration %d seconds.\n" % (sbc_buffer_name, len(timestamp)/1000))
