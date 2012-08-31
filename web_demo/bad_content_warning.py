#!/usr/bin/python

import Tkinter
from tkMessageBox import showwarning
import os
from Tkinter import *
import tkMessageBox


root = Tk() 
root.title("Invalid Content Hash")
Label(root,text='Received content does not match the requested content ID.').pack(pady=10)
#Label(text='I am a button').pack(pady=15)
#Button( text='Button') .pack(side=BOTTOM)
screen_width = root.winfo_screenwidth()
screen_height = root.winfo_screenheight()
root.geometry("500x200+%d+%d" % (screen_width/2-275, screen_height/2-125))
root.configure(background='red')
root.lift ()
root.call('wm', 'attributes', '.', '-topmost', '1')


mainloop()



