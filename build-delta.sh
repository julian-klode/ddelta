#!/bin/sh -x

# This is an approximation. It is about the same size as a real delta,
# but it does not produce a file recognized as a delta, nor does it
# handle conffiles correctly by generating a delta against /dev/null
# - but it's OK for demonstration purposes.

ddelta_dir=$(dirname $0)
ddelta_generate=$(realpath $ddelta_dir/ddelta_generate)
alpha=$(realpath $1)
beta=$(realpath $2)
output=$(realpath $3)
outputname=$(dirname $beta)/$(basename $alpha)_$(basename $beta).deb
workingdir=$(mktemp -d)

trap "{ rm -r $workingdir; }" EXIT
cd $workingdir



dpkg-deb --raw-extract $alpha alpha/
dpkg-deb --raw-extract $beta beta/
find alpha beta

cd beta; find -type f | grep -v DEBIAN | while read name; do
	if [ -e ../alpha/$name ]; then
		$ddelta_generate ../alpha/$name $name $name.delta
	else
		$ddelta_generate /dev/null $name $name.delta
	fi
	mv $name.delta $name
done

cd ..

dpkg-deb --build beta $output
