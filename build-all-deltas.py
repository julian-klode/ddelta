#!/usr/bin/python3
import apt_pkg, sys, os

apt_pkg.init()

def read_packages_file(path):
	out = {}
	with apt_pkg.TagFile(path) as tf:
		for sec in tf:
			out[(sec["Package"])] = (sec["Version"], sec["Architecture"], sec["Filename"])
	return out
		

release = read_packages_file("dists/%s/main/binary-amd64/Packages.xz" % sys.argv[1])
updates = read_packages_file("dists/%s/main/binary-amd64/Packages.xz" % sys.argv[2])

for pkg in updates:
	if pkg not in release:
		continue

	rel_ver, rel_arch, rel_path = release[pkg]
	upd_ver, upd_arch, upd_path = updates[pkg]

	if rel_ver == upd_ver:
		continue

	print("/home/jak/ddelta/build-delta.sh", rel_path, upd_path, "{}/{}_{}_{}_{}.δdeb".format(os.path.dirname(upd_path), pkg, rel_ver, upd_ver, upd_arch))
