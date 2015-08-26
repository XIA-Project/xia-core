#!/bin/bash

read command
echo "$command"

## ports=(30522 30523 30524 30525 30526)

## ports=(30523 30522 30524 30525 30526)
ports=(30524 30525 30526)
for i in ${ports[@]}; do
	echo "$ [${i}]  ${command}"
	if [ "$1" = "t" ]; then
		ssh -p ${i} hshirwad@pc3.utah.geniracks.net "tmux send -t xcache '${command}' ENTER"
	else
		ssh -p ${i} hshirwad@pc3.utah.geniracks.net ${command}
	fi
	echo ""
done
