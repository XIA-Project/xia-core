#
#    Neitris - A multiplayer Tetris clone 
#    Copyright (C) 2006 Alexandros Kostopoulos (akostopou@gmail.com)
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
import struct


# Msg Cmds
SENDSTATE    = 1
REGPLAYER    = 2
REGPLAYERACK = 3
STARTREQ     = 4
GAMEINFO     = 5
GAMESTART    = 6
GAMEOVER     = 7
POWERUP      = 8
INCRSPEED    = 9
INCRVICTS    = 10
REGPLAYERNACK = 11

def MsgPack(cmd, data, dst, src):
    l = len(data) + 3 # len of data + 1 for cmd + 2 for addresses
    header = struct.pack("!HBBB", l, cmd, dst, src)

    return header + data

def MsgUnpack(msg):
    """ Length is the actual length of the mesg payload (returned in data),
    EXCLUDING the entire header's length(5)"""
    (length, cmd, dst, src) = struct.unpack("!HBBB", msg[:5])
    data = msg[5:]
    return (length-3, data, cmd, dst, src)
