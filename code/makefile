all: dev serv

#make rule per il client
dev: dev.o
	gcc -Wall dev.o -o dev

#make rule per il server
serv: serv.o
	gcc -Wall serv.o -o serv

#pulizia dei file della compilazione	
clean:
	rm *o dev serv

