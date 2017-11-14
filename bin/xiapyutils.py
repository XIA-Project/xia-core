#!/usr/bin/env python
#
# Copyright 2017 Carnegie Mellon University
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import socket

def getxiaclickhostname():
    return ''.join(char for char in socket.gethostname().split('.')[0] if char.isalnum())

def xia_srcdir():
    return os.getcwd()[:os.getcwd().rindex('xia-core')+len('xia-core')]

def xia_bindir():
    return os.path.join(xia_srcdir(), 'bin')

def xia_etcdir():
    return os.path.join(xia_srcdir(), 'etc')
