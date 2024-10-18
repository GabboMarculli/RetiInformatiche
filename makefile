# make rule primaria con dummy target ‘all’--> non crea alcun file all ma fa un complete build
# che dipende dai target client e server scritti sotto
all: dev server
	mkdir -p rubrica
	echo "user2\nuser3" > rubrica/user1.txt
	echo "user1\nuser3" > rubrica/user2.txt
	echo "user2\nuser1" > rubrica/user3.txt
	echo "user1 user1\nuser2 user2\nuser3 user3" > users.txt

# make rule per il client
client: dev.o
	gcc -Wall dev.c -o dev 

# make rule per il server
server: server.o
	gcc -Wall server.c -o server

# pulizia dei file della compilazione (eseguito con ‘make clean’ da terminale)
clean:
	rm *o dev server






