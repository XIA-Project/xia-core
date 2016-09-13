#!/bin/bash

host=$(hostname | cut -f1 -d".")
./bin/xcidrouted_dv -h $host -t 2