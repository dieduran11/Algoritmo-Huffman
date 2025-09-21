CC=gcc
CFLAGS=-Wall -O2

all: descompresor descompresorThreads descompresorFork descomprimir

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
descomprimir: descomprimir.c
	$(CC) $(CFLAGS) descomprimir.c -o descomprimir

clean:
	rm -f descompresor descompresorThreads descompresorFork descomprimir
