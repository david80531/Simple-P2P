all:connect

connect:ser cli

ser:
	gcc ser.c -o ser

cli:
	gcc client.c -o cli

clean:
	rm -rf *o ser cli
