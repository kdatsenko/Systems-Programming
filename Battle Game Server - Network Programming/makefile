PORT=30100
CFLAGS= -DPORT=\$(PORT) -g -Wall

battle: battle.o
	gcc $(CFLAGS) -o battle battle.o

%.o: %.c parser.h
	gcc  $(CFLAGS) -c -o $@ $< 

clean:
	rm -f battle *.o
