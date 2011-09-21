#!/bin/bash

brctl delif br0 $1
ifconfig $1 0.0.0.0 down

