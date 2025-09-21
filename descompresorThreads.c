// Implementación de un descompresor de archivos usando el algoritmo de Huffman
// Version con pthreads
// se compila con: gcc descompresorThreads.c -o descompresorThreads
// se corre con: ./descompresorThreads salida.huff descomprimidosThreads/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#define CHAR_ASCII 256 // Número de caracteres ASCII

// ------------------------------------------------------
// Nodos de Huffman
// ------------------------------------------------------
typedef struct nodo {
    unsigned char caracter;
    unsigned frecuencia;
    struct nodo *hijoIzquierdo, *hijoDerecho;
} nodo;

// Crea los nodos de Huffman
nodo* crear_nodo_Huffman(unsigned char caracter, unsigned frecuencia) {
    nodo* nodoHuffman = (nodo*) malloc(sizeof(nodo));
    nodoHuffman->caracter = caracter;
    nodoHuffman->frecuencia = frecuencia;
    nodoHuffman->hijoIzquierdo = nodoHuffman->hijoDerecho = NULL;
    return nodoHuffman;
}

// ------------------------------------------------------
// Estructura para pasar argumentos a los hilos
// ------------------------------------------------------
typedef struct {
    char archivoEntrada[512];
    char ruta[1024];
    nodo* raiz;
    uint64_t tamanoArchivo;
    long offset;
    int indiceArchivo;
} ParametrosThreads;

// ------------------------------------------------------
// Min-Heap 
// ------------------------------------------------------
typedef struct {
    int tamaño;
    int capacidadMax;
    nodo** arrayPunteros;
} MinHeap;

// Crea el Min-Heap
MinHeap* crear_MinHeap(int capacidadMax) {
    MinHeap* minHeap = (MinHeap*) malloc(sizeof(MinHeap));
    minHeap->tamaño = 0;
    minHeap->capacidadMax = capacidadMax;
    minHeap->arrayPunteros = (nodo**) malloc(capacidadMax * sizeof(nodo*));
    return minHeap;
}

// Intercambia dos nodos de lugar en el Min-Heap
void intercambiar(nodo** primerNodo, nodo** segundoNodo) {
    nodo* temp = *primerNodo;
    *primerNodo = *segundoNodo;
    *segundoNodo = temp;
}

// Organiza el MinHeap para que el nodo con la frecuencia más baja esté en la raíz
void organizar_MinHeap(MinHeap* minHeap, int raiz) {
    int menor = raiz; // Inicializa el más pequeño como raíz
    int nodoIzquierdo = 2 * raiz + 1;
    int nodoDerecho = 2 * raiz + 2;
    // Encuentra el nodo más pequeño entre la raíz y sus hijos
    if (nodoIzquierdo < minHeap->tamaño && minHeap->arrayPunteros[nodoIzquierdo]->frecuencia < minHeap->arrayPunteros[menor]->frecuencia)
        menor = nodoIzquierdo;
    if (nodoDerecho < minHeap->tamaño && minHeap->arrayPunteros[nodoDerecho]->frecuencia < minHeap->arrayPunteros[menor]->frecuencia)
        menor = nodoDerecho;

    // Si el más pequeño no es la raíz, intercambia y continúa organizando
    if (menor != raiz) {
        intercambiar(&minHeap->arrayPunteros[menor], &minHeap->arrayPunteros[raiz]);
        organizar_MinHeap(minHeap, menor);
    }
}

// Extrae el nodo con la frecuencia más baja del Min-Heap
nodo* extraer_menor(MinHeap* minHeap) {
    nodo* menor = minHeap->arrayPunteros[0]; // La raíz es el nodo con la frecuencia más baja
    // Reemplaza la raíz con el último nodo y reduce el tamaño del heap
    minHeap->arrayPunteros[0] = minHeap->arrayPunteros[minHeap->tamaño - 1];
    minHeap->tamaño--;
    organizar_MinHeap(minHeap, 0);
    return menor;
}

// Inserta un nuevo nodo en el Min-Heap
void insertar(MinHeap* minHeap, nodo* node) {
    int i = minHeap->tamaño++;
    // Coloca el nuevo nodo al final y lo mueve hacia arriba para mantener la propiedad del heap
    while (i && node->frecuencia < minHeap->arrayPunteros[(i - 1) / 2]->frecuencia) {
        minHeap->arrayPunteros[i] = minHeap->arrayPunteros[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    minHeap->arrayPunteros[i] = node;
}

// Construye el Min-Heap a partir de la tabla de frecuencias
MinHeap* construir_minheap(int frecuencia[]) {
    MinHeap* minHeap = crear_MinHeap(CHAR_ASCII);
    // Inserta todos los caracteres con frecuencia mayor a 0 en el Min-Heap
    for (int i = 0; i < CHAR_ASCII; i++) {
        if (frecuencia[i] > 0) {
            // Crea un nuevo nodo de Huffman para el caracter y lo inserta en el min-heap
            minHeap->arrayPunteros[minHeap->tamaño] = crear_nodo_Huffman((unsigned char)i, frecuencia[i]);
            minHeap->tamaño++;
        }
    }
    // Organiza el min-heap para que el nodo con la frecuencia más baja esté en la raíz
    for (int i = (minHeap->tamaño - 2) / 2; i >= 0; i--) {
        organizar_MinHeap(minHeap, i);
    }
    return minHeap;
}

// Construye el árbol de Huffman a partir del Min-Heap
nodo* construir_arbol(int frecuencia[]) {
    MinHeap* minHeap = construir_minheap(frecuencia); // construye el min-heap
    if (minHeap->tamaño == 0) return NULL;
    
    while (minHeap->tamaño > 1) {
        nodo* nodo_izquierdo = extraer_menor(minHeap);
        nodo* nodo_derecho = extraer_menor(minHeap);
        // Crea un nuevo nodo interno con estos dos nodos como hijos
        nodo* raiz = crear_nodo_Huffman('$', nodo_izquierdo->frecuencia + nodo_derecho->frecuencia);
        raiz->hijoIzquierdo = nodo_izquierdo;
        raiz->hijoDerecho = nodo_derecho;
        insertar(minHeap, raiz);
    }
    return extraer_menor(minHeap);
}

// ------------------------------------------------------
// Descomprimir archivos
// ------------------------------------------------------

// Descomprime el archivo usando el árbol de Huffman
void* descomprimir_archivo_threads(void* arg) {
    ParametrosThreads* params = (ParametrosThreads*)arg;
    FILE* entrada = fopen(params->archivoEntrada, "rb");
    if (!entrada) {
        fprintf(stderr, "[Thread %d] No se pudo abrir archivo comprimido\n", params->indiceArchivo);
        pthread_exit(NULL);
    }
    fseek(entrada, params->offset, SEEK_SET);
    FILE* salida = fopen(params->ruta, "wb");
    if (!salida) {
        fprintf(stderr, "[Thread %d] No se pudo crear archivo de salida\n", params->indiceArchivo);
        fclose(entrada);
        pthread_exit(NULL);
    }
    nodo* raiz = params->raiz;
    nodo* nodoActual = raiz;
    uint64_t bytes = 0;
    int c;
    // Lee bits del archivo comprimido y reconstruye el archivo original
    while (bytes < params->tamanoArchivo && (c = fgetc(entrada)) != EOF) {
        for (int i = 7; i >= 0; i--) {
            int bit = (c >> i) & 1;
            // Mueve a la izquierda o derecha en el árbol según el bit
            // Si el bit es 1 mueve a la derecha y si es 0 mueve a la izquierda
            nodoActual = bit ? nodoActual->hijoDerecho : nodoActual->hijoIzquierdo;
            // Si se llega a una hoja, escribe el caracter en el archivo de salida
            if (!nodoActual->hijoIzquierdo && !nodoActual->hijoDerecho) {
                fputc(nodoActual->caracter, salida);
                bytes++;
                nodoActual = raiz;
                if (bytes >= params->tamanoArchivo) {
                    break;
                }
            }
        }
    }
    fclose(entrada);
    fclose(salida);
    printf("Archivo reconstruido: %s (%lu bytes)\n", params->ruta, params->tamanoArchivo);
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Uso: %s <archivo_comprimido> <directorio_salida>\n", argv[0]);
        return 1;
    }
    const char* archivoEntrada = argv[1];
    const char* directorio = argv[2];
    mkdir(directorio, 0755);
    FILE* entrada = fopen(archivoEntrada, "rb");
    if (!entrada) {
        perror("No se pudo abrir archivo comprimido");
        return 1;
    }
    // Lee la tabla de frecuencias del archivo comprimido
    int frecuencias[CHAR_ASCII];
    fread(frecuencias, sizeof(int), CHAR_ASCII, entrada);
    // Reconstruir el árbol de Huffman
    nodo* raiz = construir_arbol(frecuencias);
    // Lee el número de archivos comprimidos
    int totalArchivos;
    fread(&totalArchivos, sizeof(int), 1, entrada);
    // Reservar memoria para los hilos y parámetros
    pthread_t* threads = (pthread_t*)malloc(totalArchivos * sizeof(pthread_t));
    ParametrosThreads* params = (ParametrosThreads*)malloc(totalArchivos * sizeof(ParametrosThreads));
    long* offsets = (long*)malloc(totalArchivos * sizeof(long));

    //  Lee el encabezado para obtener los offsets y saber dónde empieza cada archivo
    long posicionArchivo = ftell(entrada);
    for (int f = 0; f < totalArchivos; f++) {
        // obtiene la informacion del archivo
        offsets[f] = posicionArchivo;
        int tamanoNombre;
        fread(&tamanoNombre, sizeof(int), 1, entrada);
        posicionArchivo += sizeof(int);
        char nombre[512];
        fread(nombre, 1, tamanoNombre, entrada);
        nombre[tamanoNombre] = '\0';
        posicionArchivo += tamanoNombre;
        uint64_t tamanoArchivo;
        fread(&tamanoArchivo, sizeof(uint64_t), 1, entrada);
        
        // guarda los parametros para cada hilo
        posicionArchivo += sizeof(uint64_t);
        snprintf(params[f].archivoEntrada, sizeof(params[f].archivoEntrada), "%s", archivoEntrada);
        snprintf(params[f].ruta, sizeof(params[f].ruta), "%s/%s", directorio, nombre);
        params[f].raiz = raiz;
        params[f].tamanoArchivo = tamanoArchivo;
        params[f].offset = posicionArchivo;
        params[f].indiceArchivo = f;
        // Lee donde termina el archivo para saber donde empieza el siguiente
        nodo* actual = raiz;
        uint64_t bytes = 0;
        while (bytes < tamanoArchivo) {
            int c = fgetc(entrada);
            if (c == EOF) break;
            posicionArchivo++;
            for (int i = 7; i >= 0; i--) {
                int bit = (c >> i) & 1;
                actual = bit ? actual->hijoDerecho : actual->hijoIzquierdo;
                if (!actual->hijoIzquierdo && !actual->hijoDerecho) {
                    bytes++;
                    actual = raiz;
                    if (bytes >= tamanoArchivo) break;
                }
            }
        }
    }
    // Inicia los hilos para descomprimir los archivos
    fclose(entrada);
    struct timeval start, end;
    gettimeofday(&start, NULL);
    for (int f = 0; f < totalArchivos; f++) { // crea un hilo por archivo a descomprimir
        pthread_create(&threads[f], NULL, descomprimir_archivo_threads, &params[f]);
    }
    for (int f = 0; f < totalArchivos; f++) { // espera a que terminen todos los hilos
        pthread_join(threads[f], NULL);
    }
    // calcula el tiempo de descompresión
    gettimeofday(&end, NULL);
    long ms = (end.tv_sec - start.tv_sec) * 1000L + (end.tv_usec - start.tv_usec) / 1000L;
    free(threads);
    free(params);
    free(offsets);
    printf("Descompresión completada en el directorio: %s\n", directorio);
    printf("Tiempo total de descompresión: %ld ms\n", ms);
    return 0;
}