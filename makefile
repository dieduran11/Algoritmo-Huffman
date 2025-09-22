CC=gcc
CFLAGS=-Wall -O2

all: descompresor descompresorThreads descompresorFork terminal compresor compresor-hilos compresor-fork

# Compilar versión serial
descompresor: descompresor.c
	$(CC) $(CFLAGS) descompresor.c -o descompresor

# Compilar versión con hilos
descompresorThreads: descompresorThreads.c
	$(CC) $(CFLAGS) descompresorThreads.c -o descompresorThreads

# Compilar versión con fork
descompresorFork: descompresorFork.c
	$(CC) $(CFLAGS) descompresorFork.c -o descompresorFork

# Compilar menú
terminal: terminal.c
	$(CC) $(CFLAGS) terminal.c -o terminal

# Compilar compresor serial
compresor: compresor.c
	$(CC) $(CFLAGS) compresor.c -o compresor

# Compilar compresor con hilos
compresor-hilos: compresor-hilos.c
	$(CC) $(CFLAGS) compresor-hilos.c -o compresor-hilos

# Compilar compresor con fork
compresor-fork: compresor-fork.c
	$(CC) $(CFLAGS) compresor-fork.c -o compresor-fork

clean:
	rm -f descompresor descompresorThreads descompresorFork terminal compresor compresor-hilos compresor-fork
