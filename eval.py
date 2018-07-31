#!/usr/bin/python3

import sys, apt_pkg

ignored=0
deltas=0
delta_sum=0
deb_sum=0

delta_sum2=0
deb_sum2=0

all_deb_sum = 0

delta_sizes = {}

with open(sys.argv[1]) as results:
	for line in results:
		name, deb, delta = line.split()
		deb, delta = int(deb), int(delta)

		try:
			delta_sizes[100 * delta // deb // 5] += 1
		except KeyError:
			delta_sizes[100 * delta // deb // 5] = 1

		if delta > 0.5 * deb:
			ignored += 1
			delta_sum2 += deb
			deb_sum2 += deb
			print("Reject", name, deb, delta, 100.0 * delta / deb)
		else:
			deltas += 1
			delta_sum += delta
			deb_sum += deb

			print("Accept", name, deb, delta, 100.0 * delta / deb)
		

print()
print("Delta size", apt_pkg.size_to_str(delta_sum))
print("Deb size", apt_pkg.size_to_str(deb_sum))
print("Upgrade size delta", apt_pkg.size_to_str(delta_sum+delta_sum2))
print("Upgrade size deb", apt_pkg.size_to_str(deb_sum+deb_sum2))

print("Average deltas", 100.0 * delta_sum/deb_sum)
print("Average total", 100.0 * (delta_sum+delta_sum2)/(deb_sum+deb_sum2))
print("Reject", ignored)
print("Accepted", deltas)

for percentage, count in sorted(delta_sizes.items()):
	print("%s to %s,%d" % (percentage*5, percentage*5+5, count))
