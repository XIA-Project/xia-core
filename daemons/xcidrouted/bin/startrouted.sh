#!/bin/bash

host=$(hostname | cut -f1 -d".")
./bin/xcidrouted_ls -h $host