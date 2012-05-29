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

class Curshape:
    pass

class Shape:
    pass

class Rotation:
    pass


class Powerup:
    pass

MAX_SHAPES = 7;
XMAX = 12;
YMAX = 31;

IMGX = 15
IMGY = 15

POWERUP_MIN = 8
POWERUP_MAX = 19

FALLING = -1

POWERUP_PROB = 80    # every how many 100ms cycles a power may occur, approx.
POWERUP_FLASHING = 25 # time (in 100ms cycles) the powerup flashes before gone

# z-left

zleft = Shape();

zleft.rot0 = Rotation();
zleft.rot1 = Rotation();

zleft.rots = [zleft.rot0, zleft.rot1];

zleft.rot = 2;
zleft.rots[0].height = 2;
zleft.rots[0].width = 3;
zleft.rots[0].rotmat =[[1,1,0,0],
                       [0,1,1,0],
                       [0,0,0,0],
                       [0,0,0,0]];

zleft.rots[1].height = 3;
zleft.rots[1].width = 2;
zleft.rots[1].rotmat =[[0,1,0,0],
                       [1,1,0,0],
                       [1,0,0,0],
                       [0,0,0,0]];

# stick

stick = Shape();

stick.rot0 = Rotation();
stick.rot1 = Rotation();

stick.rots = [stick.rot0, stick.rot1];

stick.rot = 2;
stick.rots[0].height = 1;
stick.rots[0].width = 4;
stick.rots[0].rotmat =[[7,7,7,7],
                       [0,0,0,0],
                       [0,0,0,0],
                       [0,0,0,0]];

stick.rots[1].height = 4;
stick.rots[1].width = 2;
stick.rots[1].rotmat =[[0,7,0,0],
                       [0,7,0,0],
                       [0,7,0,0],
                       [0,7,0,0]];




# square


square = Shape();

square.rot0 = Rotation();

square.rots = [square.rot0];

square.rot = 1;
square.rots[0].height = 2;
square.rots[0].width = 2;
square.rots[0].rotmat =[[2,2,0,0],
                        [2,2,0,0],
                        [0,0,0,0],
                        [0,0,0,0]];


# taf

taf = Shape();

taf.rot0 = Rotation();
taf.rot1 = Rotation();
taf.rot2 = Rotation();
taf.rot3 = Rotation();

taf.rots = [taf.rot0, taf.rot1, taf.rot2, taf.rot3];

taf.rot = 4;
taf.rots[0].height = 2;
taf.rots[0].width = 3;
taf.rots[0].rotmat =[[3,3,3,0],
                     [0,3,0,0],
                     [0,0,0,0],
                     [0,0,0,0]];

taf.rots[1].height = 3;
taf.rots[1].width = 2;
taf.rots[1].rotmat =[[3,0,0,0],
                     [3,3,0,0],
                     [3,0,0,0],
                     [0,0,0,0]];

taf.rots[2].height = 2;
taf.rots[2].width = 3;
taf.rots[2].rotmat =[[0,3,0,0],
                     [3,3,3,0],
                     [0,0,0,0],
                     [0,0,0,0]];

taf.rots[3].height = 3;
taf.rots[3].width = 2;
taf.rots[3].rotmat =[[0,3,0,0],
                     [3,3,0,0],
                     [0,3,0,0],
                     [0,0,0,0]];


# gamma-left

gleft = Shape();

gleft.rot0 = Rotation();
gleft.rot1 = Rotation();
gleft.rot2 = Rotation();
gleft.rot3 = Rotation();

gleft.rots = [gleft.rot0, gleft.rot1, gleft.rot2, gleft.rot3];

gleft.rot = 4;
gleft.rots[0].height = 2;
gleft.rots[0].width = 3;
gleft.rots[0].rotmat =[[4,4,4,0],
                       [0,0,4,0],
                       [0,0,0,0],
                       [0,0,0,0]];

gleft.rots[1].height = 3;
gleft.rots[1].width = 2;
gleft.rots[1].rotmat =[[4,4,0,0],
                       [4,0,0,0],
                       [4,0,0,0],
                       [0,0,0,0]];

gleft.rots[2].height = 2;
gleft.rots[2].width = 3;
gleft.rots[2].rotmat =[[4,0,0,0],
                       [4,4,4,0],
                       [0,0,0,0],
                       [0,0,0,0]];

gleft.rots[3].height = 3;
gleft.rots[3].width = 2;
gleft.rots[3].rotmat =[[0,4,0,0],
                       [0,4,0,0],
                       [4,4,0,0],
                       [0,0,0,0]];

# gamme-right

gright = Shape();

gright.rot0 = Rotation();
gright.rot1 = Rotation();
gright.rot2 = Rotation();
gright.rot3 = Rotation();

gright.rots = [gright.rot0, gright.rot1, gright.rot2, gright.rot3];

gright.rot = 4;
gright.rots[0].height = 2;
gright.rots[0].width = 3;
gright.rots[0].rotmat =[[5,5,5,0],
                        [5,0,0,0],
                        [0,0,0,0],
                        [0,0,0,0]];

gright.rots[1].height = 3;
gright.rots[1].width = 2;
gright.rots[1].rotmat =[[5,0,0,0],
                        [5,0,0,0],
                        [5,5,0,0],
                        [0,0,0,0]];

gright.rots[2].height = 2;
gright.rots[2].width = 3;
gright.rots[2].rotmat =[[0,0,5,0],
                        [5,5,5,0],
                        [0,0,0,0],
                        [0,0,0,0]];

gright.rots[3].height = 3;
gright.rots[3].width = 2;
gright.rots[3].rotmat =[[5,5,0,0],
                        [0,5,0,0],
                        [0,5,0,0],
                        [0,0,0,0]];


# z-right

zright = Shape();

zright.rot0 = Rotation();
zright.rot1 = Rotation();

zright.rots = [zright.rot0, zright.rot1];

zright.rot = 2;
zright.rots[0].height = 2;
zright.rots[0].width = 3;
zright.rots[0].rotmat =[[0,6,6,0],
                        [6,6,0,0],
                        [0,0,0,0],
                        [0,0,0,0]];

zright.rots[1].height = 3;
zright.rots[1].width = 2;
zright.rots[1].rotmat =[[6,0,0,0],
                        [6,6,0,0],
                        [0,6,0,0],
                        [0,0,0,0]];


shapes = [zleft, stick, square, taf, gleft, gright, zright];

antidote_pup = Powerup()
antidote_pup.pid = POWERUP_MIN
antidote_pup.prob = 50.0
antidote_pup.ttl = 100
antidote_pup.old = 0
antidote_pup.count = 0
antidote_pup.name = "antidote"
antidote_pup.vis = 2
ANTIDOTE = antidote_pup.pid

escalator_pup = Powerup()
escalator_pup.pid = POWERUP_MIN + 1
escalator_pup.prob = 30.0
escalator_pup.ttl = 100
escalator_pup.ttl_draw = XMAX - 2
escalator_pup.old = 0
escalator_pup.count = 0
escalator_pup.name = "escalator"
escalator_pup.vis = 0
ESCALATOR = escalator_pup.pid


zed_pup = Powerup()
zed_pup.pid = POWERUP_MIN + 2
zed_pup.prob = 15.0
zed_pup.ttl = 100
zed_pup.old = 0
zed_pup.count = 0
zed_pup.name = "zed"
zed_pup.vis = 1
ZED = zed_pup.pid


reversekeys_pup = Powerup()
reversekeys_pup.pid = POWERUP_MIN + 3
reversekeys_pup.prob = 30.0
reversekeys_pup.ttl = 100
reversekeys_pup.old = 0
reversekeys_pup.count = 0
reversekeys_pup.name = "reversekeys"
reversekeys_pup.vis = 1
REVERSEKEYS = reversekeys_pup.pid

rabbit_pup = Powerup()
rabbit_pup.pid = POWERUP_MIN + 4
rabbit_pup.prob = 30.0
rabbit_pup.ttl = 100
rabbit_pup.old = 0
rabbit_pup.count = 0
rabbit_pup.name = "rabbit"
rabbit_pup.vis = 0
RABBIT = rabbit_pup.pid

turtle_pup = Powerup()
turtle_pup.pid = POWERUP_MIN + 5
turtle_pup.prob = 30.0
turtle_pup.ttl = 100
turtle_pup.old = 0
turtle_pup.count = 0
turtle_pup.name = "turtle"
turtle_pup.vis = 0
TURTLE = turtle_pup.pid

crystalball_pup = Powerup()
crystalball_pup.pid = POWERUP_MIN + 6
crystalball_pup.prob = 30.0
crystalball_pup.ttl = 100
crystalball_pup.old = 0
crystalball_pup.count = 0
crystalball_pup.name = "crystalball"
crystalball_pup.vis = 1
CRYSTALBALL = crystalball_pup.pid

clearscr_pup = Powerup()
clearscr_pup.pid = POWERUP_MIN + 7
clearscr_pup.prob = 30.0
clearscr_pup.ttl = 100
clearscr_pup.old = 0
clearscr_pup.count = 0
clearscr_pup.name = "clearscr"
clearscr_pup.vis = 0
CLEARSCR = clearscr_pup.pid

donator_pup = Powerup()
donator_pup.pid = POWERUP_MIN + 8
donator_pup.prob = 30.0
donator_pup.ttl = 100
donator_pup.ttl_active = 300
donator_pup.old = 0
donator_pup.count = 0
donator_pup.name = "donator"
donator_pup.vis = 3
DONATOR = donator_pup.pid


swapscr_pup = Powerup()
swapscr_pup.pid = POWERUP_MIN + 9
swapscr_pup.prob = 30.0
swapscr_pup.ttl = 100
swapscr_pup.old = 0
swapscr_pup.count = 0
swapscr_pup.name = "swapscr"
swapscr_pup.vis = 0
SWAPSCR = swapscr_pup.pid

updown_pup = Powerup()
updown_pup.pid = POWERUP_MIN + 10
updown_pup.prob = 30.0
updown_pup.ttl = 100
updown_pup.old = 0
updown_pup.count = 0
updown_pup.name = "updown"
updown_pup.vis = 1
UPDOWN = updown_pup.pid

dotter_pup = Powerup()
dotter_pup.pid = POWERUP_MIN + 11
dotter_pup.prob = 30.0
dotter_pup.ttl = 100
dotter_pup.old = 0
dotter_pup.count = 0
dotter_pup.name = "dotter"
dotter_pup.vis = 1
DOTTER = dotter_pup.pid


donatorrcvd_pup = Powerup()
donatorrcvd_pup.pid = 255
donatorrcvd_pup.count = 0
donatorrcvd_pup.name = "donatorrcvd"
donatorrcvd_pup.vis = 0
DONATORRCVD = donatorrcvd_pup.pid

tetris_pup = Powerup()
tetris_pup.pid = 254
tetris_pup.count = 0
tetris_pup.name = "tetris"
tetris_pup.vis = 0
TETRIS = tetris_pup.pid



powerups = {}
powerups[ANTIDOTE] = antidote_pup
powerups[ESCALATOR] = escalator_pup
powerups[ZED] = zed_pup
powerups[REVERSEKEYS] = reversekeys_pup
powerups[RABBIT] = rabbit_pup
powerups[TURTLE] = turtle_pup
powerups[CRYSTALBALL] = crystalball_pup
powerups[CLEARSCR] = clearscr_pup
powerups[SWAPSCR] = swapscr_pup
powerups[UPDOWN] = updown_pup
powerups[DONATOR] = donator_pup
powerups[DONATORRCVD] = donatorrcvd_pup
powerups[TETRIS] = tetris_pup
powerups[DOTTER] = dotter_pup

