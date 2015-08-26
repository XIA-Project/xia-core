#!/bin/bash

echo "Replacing $1 with $2"

for dir in $(ls); do
	if [ -d ${dir} ]; then
		cd ${dir}
		echo "(sed -i 's/$1/$2/g' *)"
		cd -
	fi
done
