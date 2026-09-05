NAME = HaikuDownloader
TYPE = APP
SRCS = Main.cpp App.cpp MainWindow.cpp
# Esta es la línea que falta:
RDEFS = HaikuDownloader.rdef
LIBS = be tracker localestub root
OPTIMIZE = FULL

include /boot/system/develop/etc/makefile-engine