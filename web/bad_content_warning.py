#!/usr/bin/python

import Tkinter
from tkMessageBox import showwarning


def main():
    root = Tkinter.Tk()
    root.withdraw()
    result = showwarning("Invalid Content Hash", "Firefox received content that does not match the requested content ID.")



if __name__ ==  '__main__':
    main()
