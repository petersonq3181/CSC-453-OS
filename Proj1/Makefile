CFLAGS = -g

all : main

main : schedule.c
	gcc ${CFLAGS} schedule.c -o schedule

run : schedule
	./schedule 2000 ./two 3 : ./two 5
