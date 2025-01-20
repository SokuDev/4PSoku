#!/bin/sh
type shady-cli || exit
rm -rf output prepare
cp -r assets prepare
for file in `find "$(pwd)/prepare" -name "*.png" -o -name "*.xml" -o -name "*wav" -o -name "*.csv"`; do
	cd $(dirname $file)
	shady-cli convert $file
	rm $file
	cd - >/dev/null
done
mv prepare output
shady-cli pack -o assets.dat -m data output
