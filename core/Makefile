TOPDIR  := $(shell cd .; pwd)
include $(TOPDIR)/Rules.make

APP = ../simple

all: $(APP)

$(APP): main.o comutils.o engine.o parser.o tcppes.o cser.o asmjs.o sqlit.o
	$(GPP) main.o comutils.o engine.o parser.o tcppes.o cser.o asmjs.o sqlit.o -o $(APP) $(CFLAGS) -lsqlite3 -lrt
	#rm *.o

main.o: main.cpp
	$(GPP) main.cpp -c $(CXFLAGS) 

comutils.o: comutils.c	
	$(CC) comutils.c -c $(CXFLAGS) 

engine.o: engine.c
	$(CC) engine.c -c $(CXFLAGS)

parser.o: parser.c
	$(CC) parser.c -c $(CXFLAGS)

tcppes.o: tcppes.c
	$(CC) tcppes.c -c $(CXFLAGS)
	
cser.o:   cser.cpp
	$(GPP) cser.cpp -c $(CXFLAGS)

asmjs.o:  asmjs.c
	$(CC) asmjs.c  -c $(CXFLAGS) -linitparser -L./iniparser -I./iniparser

sqlit.o:  sqlit.c
	$(CC) sqlit.c -c $(CXFLAGS) 

clean:
	rm -f *.o ; rm $(APP)
TARGETTYPE := APP
TARGETNAME := /home/pi/lubeman
