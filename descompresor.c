// Implementación de un descompresor de archivos usando el algoritmo de Huffman
// Version serial
// se compila con: gcc descompresor.c -o descompresor
// se corre con: ./descompresor salida.huff descomprimidos/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
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
void descomprimir_archivo(FILE* archivoComprimido, FILE* salida, nodo* raiz, uint64_t tamano) {
    nodo* nodoActual = raiz;
    int bytes = 0;
    int c;
    // Lee byte por byte del archivo comprimido
    while (bytes < tamano && (c = fgetc(archivoComprimido)) != EOF) {
        // por cada bit en el byte
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
                if (bytes >= tamano) {
                    break;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Función principal para leer el archivo comprimido y descomprimir los archivos del directorio
// ---------------------------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Uso: %s <archivo_comprimido> <directorio_salida>\n", argv[0]);
        return 1;
    }
    const char* archivoLeido = argv[1];
    const char* directorio = argv[2];
    // Crear directorio de salida si no existe
    mkdir(directorio, 0755);
    FILE* archivoComprimido = fopen(archivoLeido, "rb");
    if (!archivoComprimido) {
        perror("No se pudo abrir el archivo comprimido");
        return 1;
    }
    // Leer la tabla de frecuencias
    int frecuencias[CHAR_ASCII];
    fread(frecuencias, sizeof(int), CHAR_ASCII, archivoComprimido);
    // Reconstruir el árbol de Huffman
    nodo* raiz = construir_arbol(frecuencias);
    // Leer el número total de archivos comprimidos
    int totalArchivos;
    fread(&totalArchivos, sizeof(int), 1, archivoComprimido);

    // Medir tiempo de descompresión
    struct timeval inicio, fin;
    gettimeofday(&inicio, NULL);

    // Procesar cada archivo
    for (int f = 0; f < totalArchivos; f++) {
        int tamanoNombre;
        fread(&tamanoNombre, sizeof(int), 1, archivoComprimido);
        char nombre[512];
        fread(nombre, 1, tamanoNombre, archivoComprimido);
        nombre[tamanoNombre] = '\0';
        uint64_t tamanoArchivo;
        fread(&tamanoArchivo, sizeof(uint64_t), 1, archivoComprimido);

        // Crear ruta de salida
        char ruta[1024];
        snprintf(ruta, sizeof(ruta), "%s/%s", directorio, nombre);
        FILE* archivoSalida = fopen(ruta, "wb");
        if (!archivoSalida) {
            perror("No se pudo crear archivo de salida");
            fclose(archivoComprimido);
            return 1;
        }

        // Descomprimir archivo
        descomprimir_archivo(archivoComprimido, archivoSalida, raiz, tamanoArchivo);
        fclose(archivoSalida);
        printf("Archivo reconstruido: %s (%lu bytes)\n", ruta, tamanoArchivo);
    }
    // Medir tiempo final
    gettimeofday(&fin, NULL);
    long ms = (fin.tv_sec - inicio.tv_sec) * 1000L + (fin.tv_usec - inicio.tv_usec) / 1000L;
    fclose(archivoComprimido);
    printf("Descompresión completada en el directorio: %s\n", directorio);
    printf("Tiempo total de descompresión: %ld ms\n", ms);
    return 0;
}