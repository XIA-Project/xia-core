#!/bin/bash

host=$(hostname | cut -f1 -d".")
./bin/xcidrouted -h $host -t 4