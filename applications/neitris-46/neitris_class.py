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

from neitris_data import *
import neitris_utils
import random
import copy
import zlib
import pygame

import struct

class Matrix:

        
    def Initialize(self):
        for i in range(YMAX):
            for j in range(XMAX):
                self.matrix[i][j] = 0;

        for i in range(YMAX):
            self.matrix[i][0] = 7;
            self.matrix[i][XMAX-1] = 7;

        for j in range(XMAX):
            self.matrix[YMAX-1][j] = 7;

        self.curshape = Curshape();
        self.curshape.y = -1
        self.curshape.x = -1
        self.curshape.shape = 0;
        self.curshape.rotation = 0;
        self.curshape.nextshape = -1;

        self.powerups_matrix = {}
        self.powerups_cleared = []
        self.powerups_active = {}
        self.victim = -1
        self.get_newshape = 0
        self.speedidx = 9

    def __init__(self, sourcex, sourcey):
        self.player_list = []
        self.player_dict_active = {}
        self.pid = 0
        self.name = ""
        self.active = 0
        self.srcx = sourcex;
        self.srcy = sourcey;
        self.matrix = [];
        self.curshape = Curshape();
        self.pup_rnd_list = []
        
        coeff = 0.0
        
        for i in powerups:
            if i != DONATORRCVD and i != TETRIS:
                coeff = coeff + powerups[i].prob
        coeff = 100 / (coeff)

        for i in powerups:
            if i != DONATORRCVD and i != TETRIS:
                powerups[i].prob = int(powerups[i].prob * coeff * 10)
        #print "Normalized Powerup Probabilities:"
        for i in powerups:
            if i != DONATORRCVD and i != TETRIS:
                print i, ": ", powerups[i].prob
            
        for i in range(1000):
            self.pup_rnd_list.append(ANTIDOTE)
            
        random.shuffle(self.pup_rnd_list)

        i = 0
        for p in powerups:
            if p != DONATORRCVD and p != TETRIS:
                for j in range(powerups[p].prob):
                    self.pup_rnd_list[i] = p
                    i = i + 1

        for i in range(YMAX):
            temp = [];
            for j in range(XMAX):
                temp.append(0);
            self.matrix.append(temp);
        random.seed()
        self.Initialize();

    def NewShape(self):

        
        self.get_newshape = 0
        
        if(self.curshape.nextshape == -1):
            self.curshape.shape = random.randint(0, MAX_SHAPES-1);
            self.curshape.nextshape = random.randint(0, MAX_SHAPES-1);
        else:
            self.curshape.shape = self.curshape.nextshape;

            if ZED in self.powerups_active:
                nshape = random.randint(0, 1);
                if nshape == 0:
                    self.curshape.nextshape = 0
                else:
                    self.curshape.nextshape = 6
            else:
                self.curshape.nextshape = random.randint(0, MAX_SHAPES-1);

        self.curshape.y = 0;
        self.curshape.x = 5;
        self.curshape.rotation = 0;

        
        
        err=self.CheckMove(0, 0, self.curshape.shape, self.curshape.rotation);
        
        self.PutShape(self.curshape.y, self.curshape.x, self.curshape.shape,
                      self.curshape.rotation);

        return err;

    def MoveLeft(self):
        if REVERSEKEYS in self.powerups_active:
            self.Move(0, 1);
        else:
            self.Move(0, -1);
        
	
    def MoveRight(self):
        if REVERSEKEYS in self.powerups_active:
            self.Move(0, -1);
        else:
            self.Move(0, 1);
	
    def MoveDown(self):
        return self.Move(1,0);

    def RotateLeft(self):
        if REVERSEKEYS in self.powerups_active:
            self.Rotate(-1);
        else:
            self.Rotate(1);

    def RotateRight(self):
        if REVERSEKEYS in self.powerups_active:
            self.Rotate(1);
        else:
            self.Rotate(-1);


    def Drop(self):
        while self.MoveDown()!= 0:
            pass;

    def ClearCompleted(self):
        lines = 0;
	#print"before:"
        #self.PrintMatrix()
        for i in range(YMAX-1):
            complete = 1;
            for j in range(1,XMAX-1):
                if(self.matrix[i][j]==0):
                    complete = 0;
                    break;

            if(complete):
                #print "line ", i, "is complete"
                lines = lines + 1;
                self.ClearLine(i);
                if(i!=0): # if not the 0th (top) line
                    self.ShiftDown(i);

	#print"after:"
        #self.PrintMatrix()
        return lines;

    def RemoveShape(self, y, x, shape, rotation):
        for j in range (shapes[shape].rots[rotation].height):
            for i in range(shapes[shape].rots[rotation].width):
                if(shapes[shape].rots[rotation].rotmat[j][i]):
                    self.matrix[y+j][x+i] = 0;

    def PutShape(self, y, x, shape, rotation):
        for j in range(shapes[shape].rots[rotation].height):
            for i in range(shapes[shape].rots[rotation].width):
                if(shapes[shape].rots[rotation].rotmat[j][i]):
                    self.matrix[y+j][x+i] = FALLING			


    def PutShape_color(self, y, x, shape, rotation):
        for j in range(shapes[shape].rots[rotation].height):
            for i in range(shapes[shape].rots[rotation].width):
                if(shapes[shape].rots[rotation].rotmat[j][i]):
                    self.matrix[y+j][x+i] = shapes[shape].rots[rotation].rotmat[
                        j][i];			


    def CheckMove(self, dy, dx, shape, rotation):
        for j in range(shapes[shape].rots[rotation].height):
            for i in range(shapes[shape].rots[rotation].width):
                if(shapes[shape].rots[rotation].rotmat[j][i]):
                    if(self.matrix[self.curshape.y+dy+j][self.curshape.x+dx+i]):
                        return 0;
        return 1;

    def Move(self, dy, dx):
        self.RemoveShape(self.curshape.y, self.curshape.x, self.curshape.shape,
                         self.curshape.rotation);
        ret=self.CheckMove(dy, dx, self.curshape.shape, self.curshape.rotation);
        # check whether some powerup (e.g. an escalator) has hit on the
        # falling shape
        if self.get_newshape == 1:
            self.get_newshape = 0
            ret = 0
        elif self.get_newshape == 2:
            self.get_newshape == 0
            return 0 
        
        if(ret):
            self.curshape.x+=dx;
            self.curshape.y+=dy;
            
            self.PutShape(self.curshape.y,self.curshape.x, self.curshape.shape,
                          self.curshape.rotation);
        else:
            self.PutShape_color(self.curshape.y, self.curshape.x,
                            self.curshape.shape, self.curshape.rotation);

	return ret;


    def Rotate(self, dir):
        self.RemoveShape(self.curshape.y, self.curshape.x, self.curshape.shape,
                         self.curshape.rotation);
        newrot = dir + self.curshape.rotation;

        if(newrot<0):
            newrot = shapes[self.curshape.shape].rot - 1;
        if(newrot==shapes[self.curshape.shape].rot):
            newrot = 0;
        ret=self.CheckMove(0, 0, self.curshape.shape, newrot);

        if(ret):
            self.curshape.rotation = newrot;

        self.PutShape(self.curshape.y, self.curshape.x, self.curshape.shape,
                      self.curshape.rotation);		

    def ClearLine(self, line):
        for j in range(1, XMAX-1):
            if (line, j) in self.powerups_matrix:
                self.powerups_cleared.append(self.powerups_matrix[(line,j)])
                del self.powerups_matrix[(line,j)]
                print "Got a Powerup!!!"
            if self.matrix[line][j] != FALLING:
                self.matrix[line][j] = 0;

    def ShiftDown(self, line):
        for i in range(line, 0, -1): 
            for j in range(1, XMAX-1):
                # Move down powerups, if any
                if (i-1,j) in self.powerups_matrix:
                    pupobj = Powerup()
                    pupobj.pid = self.powerups_matrix[(i-1,j)].pid
                    pupobj.ttl = self.powerups_matrix[(i-1,j)].ttl
                    pupobj.old = self.powerups_matrix[(i-1,j)].old
                    self.powerups_matrix[(i,j)] = pupobj
                    del self. powerups_matrix[(i-1,j)]
                    #print "Moved Powerup from (%d, %d) to (%d, %d)" % \
                    #      (i-1,j,i,j)
                    
                self.matrix[i][j] = self.matrix[i-1][j];
            self.ClearLine(i-1);

    def ShiftUp(self, line):
        # First check whether there is something in line 0
        for j in range(1, XMAX-1):
            if (0,j) in self.powerups_matrix:
                del self.powerups_matrix[(0,j)]
                
        for i in range(1, line): 
            for j in range(1, XMAX-1):
                # Move up powerups, if any
                if (i,j) in self.powerups_matrix:
                    pupobj = Powerup()
                    pupobj.pid = self.powerups_matrix[(i,j)].pid
                    pupobj.ttl = self.powerups_matrix[(i,j)].ttl
                    pupobj.old = self.powerups_matrix[(i,j)].old
                    self.powerups_matrix[(i-1,j)] = pupobj
                    del self. powerups_matrix[(i,j)]
                    #print "Moved Powerup from (%d, %d) to (%d, %d)" % \
                    #      (i-1,j,i,j)
                if self.matrix[i][j] != FALLING:
                    self.matrix[i-1][j] = self.matrix[i][j];
            self.ClearLine(i);


    def PrintMatrix(self):
        print "Matrix: "
        for j in range(YMAX):
            str=""
            for i in range(XMAX):
                if self.matrix[j][i] == 0:
                    str = str + ' ';
                elif self.matrix[j][i] == -1:
                    str = str + 'X';
                else:
                    str = str + '*';
                    
            print str;

        if self.curshape.nextshape == -1:
            return

        print
        print "Next Shape is ", self.curshape.nextshape
        print "height: ", shapes[self.curshape.nextshape].rots[0].height
        print "width: ", shapes[self.curshape.nextshape].rots[0].width
        for j in range(shapes[self.curshape.nextshape].rots[0].height):
            str=""
            for i in range(shapes[self.curshape.nextshape].rots[0].width):
                if(shapes[self.curshape.nextshape].rots[0].rotmat[j][i]):
                    str = str + '*'
                else:
                    str = str + ' '
            print str

    def DrawMatrix(self, screen, bricks, dots,  players, background):
        screen.blit(background, (self.srcx, 0))            
        if(self.curshape.x != -1):
            self.PutShape_color(self.curshape.y, self.curshape.x,
                                self.curshape.shape, self.curshape.rotation);

        #print "player %d, (x,y)=(%d,%d)\n", (self.pid, self.srcx, self.srcy)
        if DOTTER in self.powerups_active:
            drawset = dots
        else:
            drawset = bricks
            
        for j in range(YMAX):
            for i in range(XMAX):
                if(self.matrix[j][i]):
                    if UPDOWN in self.powerups_active:
                        screen.blit(drawset[self.matrix[j][i]-1],
                                    (i*IMGX+self.srcx,
                                     (YMAX-1-j)*IMGY+self.srcy))
                    else:
                        screen.blit(drawset[self.matrix[j][i]-1],
                                    (i*IMGX+self.srcx,j*IMGY+self.srcy))
        
        if(self.curshape.x != -1):
            self.PutShape(self.curshape.y, self.curshape.x,
                          self.curshape.shape, self.curshape.rotation);
                    
        if self.curshape.nextshape == -1:
            return

        if CRYSTALBALL not in self.powerups_active:
            for j in range(shapes[self.curshape.nextshape].rots[0].height):
                for i in range(shapes[self.curshape.nextshape].rots[0].width):
                    if(shapes[self.curshape.nextshape].rots[0].rotmat[j][i]):
                        screen.blit(bricks[shapes[self.curshape.nextshape
                            ].rots[0].rotmat[j][i]-1], ((13+i)*IMGX+self.srcx,
                            (j+1)*IMGY+self.srcy))


        # draw powerups
        if ANTIDOTE in self.powerups_active:
            reps = self.powerups_active[ANTIDOTE].count
            if reps > 4:
                reps = 4
            for i in range(reps):
                screen.blit(bricks[ANTIDOTE-1], (i*(IMGX)+self.srcx,
                                                 500+self.srcy))
        if DONATOR in self.powerups_active:
            reps = int( self.powerups_active[DONATOR].ttl_active * 10.0 / \
                   powerups[DONATOR].ttl_active)
            
            for i in range(reps):
                screen.blit(bricks[DONATOR-1], (i*(IMGX)+self.srcx,
                                                 480+self.srcy))

        k = 0
        for i in self.powerups_active:
            if self.powerups_active[i].vis == 1:
                screen.blit(bricks[i-1], (k*(IMGX)+self.srcx, 520+self.srcy))
                k=k+1

        # Create text

        # Create player's name
        font = pygame.font.Font(None, 25)
        text = font.render("%s" % (self.name), 1, (244, 255, 29), (0,0,0))
        textpos = text.get_rect()
        textpos.centerx = 0
        textpos.centery = 0
        screen.blit(text, (self.srcx+50, self.srcy+470))
        # Create victim's name
        font = pygame.font.Font(None, 20)
        if self.victim == -1:
            str = "Victim: Nobody"
        else:
            str = "Victim: %s" % (players[self.victim].name)
        
        text = font.render(str, 1, (255, 24, 24), (0,0,0))
        textpos = text.get_rect()
        textpos.centerx = 0
        textpos.centery = 0
        screen.blit(text, (self.srcx+20, self.srcy+540))

        # Display won games and speed
        font = pygame.font.Font(None, 20)
        str = "Wins: %d    Speed: %d" % (self.victories, 10 - self.speedidx)
            
        text = font.render(str, 1, (255, 24, 24), (0,0,0))
        textpos = text.get_rect()
        textpos.centerx = 0
        textpos.centery = 0
        screen.blit(text, (self.srcx+20, self.srcy+560))

    def GetMatrixStream(self):
        self.PutShape_color(self.curshape.y, self.curshape.x,
                            self.curshape.shape, self.curshape.rotation);

        data = ''
        for j in range(YMAX-1):
            for i in range(1,XMAX-1):
                try:
                    data = data +  struct.pack("B", self.matrix[j][i])
                except:
                    print "error: matrix[%d][%d] is %d" % (j,i,self.matrix[j][i])
                    sys.exit()
        
        self.PutShape(self.curshape.y, self.curshape.x, self.curshape.shape,
                      self.curshape.rotation);

        # Send nextshape
        data = data +  struct.pack("B", self.curshape.nextshape)
        if self.victim == -1:
            data = data +  struct.pack("B", 255)
        else:
            data = data +  struct.pack("B", self.victim)

        # Send Speed and victories
        data = data + struct.pack("!H", self.victories)
        data = data + struct.pack("B", self.speedidx)

        # Send powerups
        for i in self.powerups_active:
            if self.powerups_active[i].vis == 0:
                continue
            if i == ANTIDOTE:
                data = data + struct.pack("B", ANTIDOTE)
                data = data + struct.pack("B",
                                          self.powerups_active[ANTIDOTE].count)
            elif i == DONATOR:
                data = data + struct.pack("B", DONATOR)
                data = data + struct.pack("!H",
                                 self.powerups_active[DONATOR].ttl_active)
            else:
                data = data + struct.pack("B", i)
                


        data = zlib.compress(data, 9)
        return data

    def PutMatrixStream(self, data):
        data = zlib.decompress(data)
        k=0
        for j in range(YMAX-1):
            for i in range(1, XMAX-1):
                
                self.matrix[j][i], = struct.unpack("B",data[k])
                #matrix_enemy.matrix[j][i] = int(data[k])
                k=k+1

        # get nextshape
        self.curshape.nextshape, = struct.unpack("B",data[k])
        self.victim, = struct.unpack("B",data[k+1])
        if self.victim == 255:
            self.victim = -1
            
        k = k + 2

        # get speedidx and victories
        self.victories, = struct.unpack("!H", data[k:k+2])
        self.speedidx, = struct.unpack("B", data[k+2])
        k = k + 3

        
        # Rcv Powerups
        self.powerups_active = {}

        while k < len(data):
            pup, = struct.unpack("B",data[k])
            self.powerups_active[pup] = copy.deepcopy(powerups[pup])
            if pup == ANTIDOTE:
                count, = struct.unpack("B",data[k+1])
                self.powerups_active[pup].count = count
                k = k + 2
            elif pup == DONATOR:
                count, = struct.unpack("!H",data[k+1:k+3])
                self.powerups_active[pup].ttl_active = count
                k = k + 3
            else:
                k = k + 1

        
        


            
    def GeneratePowerup(self):
        genpup = random.randint(0,POWERUP_PROB-1);

        if genpup != 0:
            return

        pupidx = random.randint(0, 999)
        #pup = random.randint(POWERUP_MIN, POWERUP_MAX);
        pup = self.pup_rnd_list[pupidx]
        #print "generating powerup: ", pup

        rows=[]
        for i in range(YMAX-1):
            rows.append(i)

        linernd = -1
        while rows!=[]:
            line = random.randint(0, len(rows)-1)
            i = rows[line]
            for j in range(1, XMAX-1):
                if self.matrix[i][j]!=0 and self.matrix[i][j]!=FALLING and \
                   (self.matrix[i][j] < POWERUP_MIN or \
                   self.matrix[i][j] > POWERUP_MAX):
                    linernd = i
                    break
            if linernd != -1:
                break
            rows.remove(i)
            
        if linernd == -1:
            #print "No bricks available for powerup"
            return

        cols=[]
        for j in range(1, XMAX-1):
            if self.matrix[linernd][j]!=0 and \
                   self.matrix[linernd][j]!=FALLING and \
                   (self.matrix[linernd][j] < POWERUP_MIN or \
                   self.matrix[linernd][j] > POWERUP_MAX):
                
                   cols.append(j)

        col = random.randint(0, len(cols)-1)

        x = cols[col]
        #print "powerup will be put at (%d, %d)\n" % (x, linernd)

        pupobj = Powerup()
        pupobj.pid = pup
        pupobj.ttl = powerups[pup].ttl
        pupobj.old = self.matrix[linernd][x]
        

        self.powerups_matrix[(linernd, x)] = pupobj
        self.matrix[linernd][x] = pup
        

    def MonitorPowerups(self):
        d=[]
        ret = 0
        for p in self.powerups_matrix:
            self.powerups_matrix[p].ttl = self.powerups_matrix[p].ttl - 1
            (y,x) = p
            if self.powerups_matrix[p].ttl == 0:
                self.matrix[y][x] = self.powerups_matrix[p].old
                d.append(p)
                ret = 1
                #print "deleting powerup in ", p
            elif self.powerups_matrix[p].ttl < POWERUP_FLASHING:

                if self.matrix[y][x] == self.powerups_matrix[p].old:
                    self.matrix[y][x] = self.powerups_matrix[p].pid
                else:
                    self.matrix[y][x] = self.powerups_matrix[p].old
                ret = 1 

        for p in d:
            del self.powerups_matrix[p]
            
        return ret
                
    def ProcessClearedPowerups(self, wqueue):
        while self.powerups_cleared != []:
            pup = self.powerups_cleared.pop(0)

            # Antidote
            if pup.pid == ANTIDOTE:
                
                if ANTIDOTE in self.powerups_active:
                    self.powerups_active[ANTIDOTE].count = \
                          self.powerups_active[ANTIDOTE].count + 1
                else:
                    self.powerups_active[ANTIDOTE] = \
                           copy.deepcopy(powerups[ANTIDOTE])
                    self.powerups_active[ANTIDOTE].count = 1
                    
           
            elif pup.pid == ZED or pup.pid == REVERSEKEYS or \
                 pup.pid == ESCALATOR or pup.pid == RABBIT or \
                 pup.pid == CRYSTALBALL or pup.pid == UPDOWN or \
                 pup.pid == DOTTER: 
                data = struct.pack("B", pup.pid)
                msg = neitris_utils.MsgPack(
                    neitris_utils.POWERUP, data, self.victim, self.pid)
                if self.victim != -1:
                    wqueue.put_nowait(msg)
                    print "Sent", pup.pid," to player ", self.victim

            elif pup.pid == SWAPSCR:
                data = self.GetSwapData()
                data = struct.pack("BB", SWAPSCR,0) + data
                
                msg = neitris_utils.MsgPack(
                    neitris_utils.POWERUP, data, self.victim, self.pid)
                if self.victim != -1:
                    wqueue.put_nowait(msg)

            elif pup.pid == TURTLE:
                self.speedidx = self.speedidx + 2
                if self.speedidx > 9:
                    self.speedidx = 9

            elif pup.pid == CLEARSCR:
                for j in range(YMAX-1):
                    for i in range(1,XMAX-1):
                        self.matrix[j][i] = 0
                self.powerups_matrix = {}
                #self.get_newshape = 1

            elif pup.pid == DONATOR:
                
                if DONATOR in self.powerups_active:
                    self.powerups_active[DONATOR].ttl_active = \
                          powerups[DONATOR].ttl_active
                else:
                    self.powerups_active[DONATOR] = \
                           copy.deepcopy(powerups[DONATOR])
                

    def GetSwapData(self):
        data = ""
        for j in range(YMAX-1):
            for i in range(1,XMAX-1):
                if self.matrix[j][i] == FALLING:
                    data = data + struct.pack("B", 0)
                else:
                    data = data + struct.pack("B", self.matrix[j][i])
                    if self.matrix[j][i] > POWERUP_MIN and \
                           self.matrix[j][i] < POWERUP_MAX:
                        data = data + struct.pack("B",
                                 self.powerups_matrix[(j,i)].ttl)
                        data = data + struct.pack("B",
                                 self.powerups_matrix[(j,i)].old)


        data = zlib.compress(data)
        return data

    def PutSwapData(self, data):
        self.powerups_matrix = {}
        print "In putswapdata"
        data = zlib.decompress(data)
        k=0
        for j in range(YMAX-1):
            for i in range(1,XMAX-1):
                self.matrix[j][i], = struct.unpack("B", data[k])
                p = self.matrix[j][i]
                if p > POWERUP_MIN and \
                   p < POWERUP_MAX:
                    ttl, = struct.unpack("B", data[k+1])
                    old, = struct.unpack("B", data[k+2])
                    pupobj = Powerup()
                    pupobj.pid = p
                    print "took powerup ", pupobj.pid, "from opponent"
                    pupobj.ttl = ttl
                    pupobj.old = old
                    self.powerups_matrix[(j,i)] = pupobj
                    k = k + 2
                k = k + 1
        self.get_newshape = 2
        for i in self.powerups_matrix:
            print "powerup in ", i, " is ", self.powerups_matrix[i].pid
        
    def UseAntidote(self):
        if ANTIDOTE not in self.powerups_active:
            print "You don't have an antidote...Sorry..."
            return

        # first decrease antidote count
        self.powerups_active[ANTIDOTE].count = \
                    self.powerups_active[ANTIDOTE].count - 1
        if self.powerups_active[ANTIDOTE].count == 0:
            del self.powerups_active[ANTIDOTE]

        # then undo all bad powerups
        if ZED in self.powerups_active:
            del self.powerups_active[ZED]

        if DOTTER in self.powerups_active:
            del self.powerups_active[DOTTER]

        if REVERSEKEYS in self.powerups_active:
            del self.powerups_active[REVERSEKEYS]
            
        if CRYSTALBALL in self.powerups_active:
            del self.powerups_active[CRYSTALBALL]

        if UPDOWN in self.powerups_active:
            del self.powerups_active[UPDOWN]
            
    # Process all powerups that need animation or constant processing
    def ProcessActivePowerups(self, donatorlines, wqueue):
             
        if RABBIT in self.powerups_active:
            self.speedidx = self.speedidx - 2
            if self.speedidx < 0:
                self.speedidx = 0
            del self.powerups_active[RABBIT]
            
        # immediately applied code-controls sending of lines to opponent       
        if DONATOR in self.powerups_active:
            if donatorlines > 0:
                data = struct.pack("BB", DONATORRCVD, donatorlines )
                msg = neitris_utils.MsgPack(
                    neitris_utils.POWERUP, data, self.victim, self.pid)
                if self.victim != -1:
                    wqueue.put_nowait(msg)
                    print "Sent ", donatorlines, " by donator to player ", \
                          self.victim

        if DONATORRCVD in self.powerups_active:
            print "received ", self.powerups_active[DONATORRCVD].lines,\
                  "lines from donator"
            print "Matrix Before donator"
            self.PrintMatrix()
            for i in range(self.powerups_active[DONATORRCVD].lines):
                self.ShiftUp(YMAX-1)

                emptyspot = random.randint(1, XMAX-2)
                linecolor = random.randint(1,7)
                 
                for j in range(1, XMAX-1):
                    if j != emptyspot:
                        self.matrix[YMAX-2][j] = linecolor
                
            del self.powerups_active[DONATORRCVD]
            print "Matrix After donator"
            self.PrintMatrix()

        if SWAPSCR in self.powerups_active:
            del self.powerups_active[SWAPSCR]

        # Check whether a tetris has been done
        if donatorlines >= 4:
            data = struct.pack("B", TETRIS)
            msg = neitris_utils.MsgPack(
                neitris_utils.POWERUP, data, self.victim, self.pid)
            if self.victim != -1:
                wqueue.put_nowait(msg)
                print "Sent TETRIS to player", self.victim

        # Check whether a tetris has been received
        if TETRIS in self.powerups_active:

            # find the first empty line
            k=YMAX-2
            for i in range(YMAX-2, 0, -1):
                found = 1
                for j in range(1, XMAX-1):
                    if self.matrix[i][j]!=0 and self.matrix[i][j]!=FALLING:
                        found = 0
                        break
                if found:
                    break
                k = k-1
            print "empty line found at ", k
            # now put two lines there
            self.PutRandomLines(k-1, 2)
            del self.powerups_active[TETRIS]
            
            
                
         
    def PutRandomLines(self, pos, lines):
        for i in range(pos, pos + lines):
            emptyspot = random.randint(1, XMAX-2)
            linecolor = random.randint(1,7)
                           
            for j in range(1, XMAX-1):
                if self.matrix[i][j] == FALLING:
                    self.get_newshape = 1
                if j != emptyspot:
                    self.matrix[i][j] = linecolor
                        

            
        
                  
            
    def ProcessActivePowerupsTimed(self):
        
        if ESCALATOR in self.powerups_active:
            self.powerups_active[ESCALATOR].ttl_draw = \
                  self.powerups_active[ESCALATOR].ttl_draw - 1
            
            ttl = self.powerups_active[ESCALATOR].ttl_draw

            # check whether the escalator hit the falling piece
            if self.matrix[YMAX+1-XMAX+ttl][XMAX-2-ttl] == FALLING:
                self.get_newshape = 1
           
            # check whether the escalator hit a powerup
            if self.matrix[YMAX+1-XMAX+ttl][XMAX-2-ttl] > POWERUP_MIN and \
               self.matrix[YMAX+1-XMAX+ttl][XMAX-2-ttl] < POWERUP_MAX:
                del self.powerups_matrix[(YMAX+1-XMAX+ttl,XMAX-2-ttl)]
                
            self.matrix[YMAX+1-XMAX+ttl][XMAX-2-ttl] = 7
            
            if self.powerups_active[ESCALATOR].ttl_draw == 0:
                del self.powerups_active[ESCALATOR]
    
        # regurarly timed donator code - controls donator's duration
        if DONATOR in self.powerups_active:
            
            self.powerups_active[DONATOR].ttl_active = \
                self.powerups_active[DONATOR].ttl_active - 1

            if self.powerups_active[DONATOR].ttl_active < 0:
                del self.powerups_active[DONATOR]

     

    def ChangeVictim(self, action):

        # Set victim to the player next to the current player
        if action == "init":
            # first find the players position in victim list
            k = 0
            for p in range(len(self.player_list)):
                print "playerlist ", p, "=", self.player_list[p]
                if self.player_list[p] == self.pid:
                    break
                k = k + 1;
            # find the next victim
            print "k=", k, " len=", len(self.player_list)
            if k == len(self.player_list) - 1:
                self.victim = self.player_list[0]
                k = 0
            else:
                self.victim = self.player_list[k+1]
                k = k+1

            # if next victim is Nobody and not playing alone, get the victim
            # after that
            if self.victim == -1 and len(self.player_list) > 2:
                if k == len(self.player_list) - 1:
                    self.victim = self.player_list[0]
                else:
                    self.victim = self.player_list[k+1]
                 

            

        else:
            # find the current victims position in list
            k = 0
            for p in range(len(self.player_list)):
                if self.player_list[p] == self.victim:
                    break
                k = k + 1
            
            # find the next victim's position
            if k == len(self.player_list) - 1:
                k = 0
            else:
                k = k+1

            # Get number of active victim's in the list
            actnr = 0
            for p in range(len(self.player_list)):
                if self.player_dict_active[self.player_list[p]] == 1:
                    actnr = actnr + 1

            self.victim = self.player_list[k];
            # if next victim is the player or (Nobody and the previous victim
            # just died) or dead,  AND at least one opponent is active,
            # then get the victim after that
            if self.victim == self.pid or \
               (self.victim == -1 and action=="victimdied" and actnr>1) or \
               self.player_dict_active[self.victim] == 0:

                self.ChangeVictim(action)
            else:
                print "new victim: ", self.victim


