all: task3.o LineParser.o job_control.o
	gcc -g -Wall -o myshell LineParser.o task3.o job_control.o
LineParser.o: LineParser.c LineParser.h
	gcc -g -Wall -c -o LineParser.o LineParser.c
task3.o: task3.c LineParser.h
	gcc -g -Wall -c -o task3.o task3.c
job_control.o: job_control.c job_control.h
	gcc -g -Wall -c -o job_control.o job_control.c
.PHONY: clean
clean:
	rm -f *.o myshell