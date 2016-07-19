#!/bin/bash

for file in ${PWD}/*.man; do
    cat ${file}  | groff -mandoc -Thtml > /tmp/$(basename ${file}.html)
done
