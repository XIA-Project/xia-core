#!/usr/bin/python

import sys
from PyQt4.QtGui import QApplication
from main import Main

if __name__ == '__main__':
    app = QApplication(sys.argv)
    main = Main()

    main.show()
    sys.exit(app.exec_())

