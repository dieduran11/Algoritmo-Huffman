#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>  // para medir tiempo

#define NUM_CHARS 256

// =================== nodo de huffman ===================
typedef struct HuffmanNode {
    unsigned char dato;      // el caracter
    unsigned frecuencia;     // frecuencia de aparicion
    struct HuffmanNode *izquierda, *derecha;
} HuffmanNode;

// =================== min-heap ===================
typedef struct {
    int tamano;
    int capacidad;
    HuffmanNode** array;
} MinHeap;

// crear un nodo de huffman
HuffmanNode* crear_nodo(unsigned char dato, unsigned frecuencia) {
    HuffmanNode* nodo = (HuffmanNode*) malloc(sizeof(HuffmanNode));
    nodo->dato = dato;
    nodo->frecuencia = frecuencia;
    nodo->izquierda = nodo->derecha = NULL;
    return nodo;
}

// crear un minheap
MinHeap* crear_minheap(int capacidad) {
    MinHeap* minheap = (MinHeap*) malloc(sizeof(MinHeap));
    minheap->tamano = 0;
    minheap->capacidad = capacidad;
    minheap->array = (HuffmanNode**) malloc(capacidad * sizeof(HuffmanNode*));
    return minheap;
}

// intercambiar nodos
void intercambiar_nodos(HuffmanNode** a, HuffmanNode** b) {
    HuffmanNode* t = *a;
    *a = *b;
    *b = t;
}

// mantener propiedad de minheap
void minheapify(MinHeap* minheap, int idx) {
    int menor = idx;
    int izquierda = 2 * idx + 1;
    int derecha = 2 * idx + 2;

    if (izquierda < minheap->tamano && minheap->array[izquierda]->frecuencia < minheap->array[menor]->frecuencia)
        menor = izquierda;
    if (derecha < minheap->tamano && minheap->array[derecha]->frecuencia < minheap->array[menor]->frecuencia)
        menor = derecha;

    if (menor != idx) {
        intercambiar_nodos(&minheap->array[menor], &minheap->array[idx]);
        minheapify(minheap, menor);
    }
}

// extraer el nodo con menor frecuencia
HuffmanNode* extraer_min(MinHeap* minheap) {
    HuffmanNode* temp = minheap->array[0];
    minheap->array[0] = minheap->array[minheap->tamano - 1];
    minheap->tamano--;
    minheapify(minheap, 0);
    return temp;
}

// insertar un nodo en minheap
void insertar_minheap(MinHeap* minheap, HuffmanNode* nodo) {
    int i = minheap->tamano++;
    while (i && nodo->frecuencia < minheap->array[(i - 1) / 2]->frecuencia) {
        minheap->array[i] = minheap->array[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    minheap->array[i] = nodo;
}

// construir minheap a partir de frecuencias
MinHeap* construir_minheap(int frecuencias[]) {
    MinHeap* minheap = crear_minheap(NUM_CHARS);
    for (int i = 0; i < NUM_CHARS; i++) {
        if (frecuencias[i] > 0) {
            minheap->array[minheap->tamano] = crear_nodo((unsigned char)i, frecuencias[i]);
            minheap->tamano++;
        }
    }
    for (int i = (minheap->tamano - 2) / 2; i >= 0; i--) {
        minheapify(minheap, i);
    }
    return minheap;
}

// construir arbol de huffman
HuffmanNode* construir_arbol_huffman(int frecuencias[]) {
    MinHeap* minheap = construir_minheap(frecuencias);
    if (minheap->tamano == 0) return NULL;

    while (minheap->tamano > 1) {
        HuffmanNode* izquierda = extraer_min(minheap);
        HuffmanNode* derecha = extraer_min(minheap);
        HuffmanNode* top = crear_nodo('$', izquierda->frecuencia + derecha->frecuencia);
        top->izquierda = izquierda;
        top->derecha = derecha;
        insertar_minheap(minheap, top);
    }
    return extraer_min(minheap);
}

// =================== generacion de codigos ===================
void generar_codigos(HuffmanNode* raiz, char* codigo, int profundidad, char* codigos[NUM_CHARS]) {
    if (!raiz) return;

    if (!raiz->izquierda && !raiz->derecha) {
        codigo[profundidad] = '\0';
        codigos[raiz->dato] = strdup(codigo);
        return;
    }
    codigo[profundidad] = '0';
    generar_codigos(raiz->izquierda, codigo, profundidad + 1, codigos);
    codigo[profundidad] = '1';
    generar_codigos(raiz->derecha, codigo, profundidad + 1, codigos);
}

// =================== compresion ===================
void comprimir_archivo(const char* ruta_archivo, FILE* salida, char* codigos[NUM_CHARS]) {
    FILE* entrada = fopen(ruta_archivo, "rb");
    if (!entrada) {
        perror("error al abrir archivo");
        return;
    }

    unsigned char buffer = 0;
    int contador_bits = 0;
    int c;

    while ((c = fgetc(entrada)) != EOF) {
        char* codigo = codigos[(unsigned char)c];
        for (int i = 0; codigo[i] != '\0'; i++) {
            buffer = (buffer << 1) | (codigo[i] - '0');
            contador_bits++;
            if (contador_bits == 8) {
                fputc(buffer, salida);
                contador_bits = 0;
                buffer = 0;
            }
        }
    }

    if (contador_bits > 0) {
        buffer <<= (8 - contador_bits);
        fputc(buffer, salida);
    }

    fclose(entrada);
}

// =================== recoleccion de frecuencias ===================
void recolectar_frecuencias(const char* ruta_archivo, int frecuencias[]) {
    FILE* entrada = fopen(ruta_archivo, "rb");
    if (!entrada) {
        perror("error al abrir archivo");
        return;
    }
    int c;
    while ((c = fgetc(entrada)) != EOF) {
        frecuencias[(unsigned char)c]++;
    }
    fclose(entrada);
}

// =================== funcion principal ===================
int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("uso: %s <directorio_entrada> <archivo_salida>\n", argv[0]);
        return 1;
    }

    // empezamos a medir tiempo
    clock_t tiempo_inicio = clock();

    const char* directorio_entrada = argv[1];
    const char* archivo_salida = argv[2];

    int frecuencias[NUM_CHARS] = {0};
    DIR* dir = opendir(directorio_entrada);
    if (!dir) {
        perror("no se pudo abrir el directorio");
        return 1;
    }

    // paso 1: calcular frecuencias globales de todos los archivos
    struct dirent* entrada_archivo;
    int contador_archivos = 0;

    while ((entrada_archivo = readdir(dir)) != NULL) {
        if (entrada_archivo->d_type == DT_REG) {
            char ruta[512];
            snprintf(ruta, sizeof(ruta), "%s/%s", directorio_entrada, entrada_archivo->d_name);
            recolectar_frecuencias(ruta, frecuencias);
            contador_archivos++;
        }
    }
    closedir(dir);

    if (contador_archivos == 0) {
        printf("no se encontraron archivos en el directorio.\n");
        return 1;
    }

    // paso 2: construir arbol huffman
    HuffmanNode* raiz = construir_arbol_huffman(frecuencias);
    char* codigos[NUM_CHARS] = {0};
    char codigo[256];
    generar_codigos(raiz, codigo, 0, codigos);

    // paso 3: crear archivo de salida
    FILE* salida = fopen(archivo_salida, "wb");
    if (!salida) {
        perror("no se pudo crear archivo de salida");
        return 1;
    }

    // guardar tabla de frecuencias
    fwrite(frecuencias, sizeof(int), NUM_CHARS, salida);

    // guardar numero de archivos
    fwrite(&contador_archivos, sizeof(int), 1, salida);

    // guardar metadatos y comprimir cada archivo
    dir = opendir(directorio_entrada);
    while ((entrada_archivo = readdir(dir)) != NULL) {
        if (entrada_archivo->d_type == DT_REG) {
            char ruta[512];
            snprintf(ruta, sizeof(ruta), "%s/%s", directorio_entrada, entrada_archivo->d_name);

            struct stat st;
            stat(ruta, &st);
            uint64_t tamano_archivo = st.st_size;

            int longitud_nombre = strlen(entrada_archivo->d_name);
            fwrite(&longitud_nombre, sizeof(int), 1, salida);
            fwrite(entrada_archivo->d_name, 1, longitud_nombre, salida);
            fwrite(&tamano_archivo, sizeof(uint64_t), 1, salida);

            comprimir_archivo(ruta, salida, codigos);
        }
    }
    closedir(dir);
    fclose(salida);

    // fin de la medicion de tiempo
    clock_t tiempo_fin = clock();
    double tiempo_ms = ((double)(tiempo_fin - tiempo_inicio) / CLOCKS_PER_SEC) * 1000.0;

    printf("Compresión completada en %.2f ms. \nArchivos comprimidos en: %s\n", tiempo_ms, archivo_salida);
    return 0;
}
