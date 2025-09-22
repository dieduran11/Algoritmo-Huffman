#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <stdint.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define NUM_CHARS 256
#define MAX_HILOS 100

// =================== nodo de huffman ===================
typedef struct HuffmanNode {
    unsigned char data;       // el caracter
    unsigned freq;            // frecuencia de aparicion
    struct HuffmanNode *left, *right;
} HuffmanNode;

// =================== min-heap ===================
typedef struct {
    int size;
    int capacity;
    HuffmanNode** array;
} MinHeap;

// =================== estructura para estadisticas de hilos ===================
typedef struct {
    int hilo_id;
    pthread_t thread_id;
    int archivos_procesados;
    clock_t tiempo_inicio;
    clock_t tiempo_fin;
} EstadisticasHilo;

// crear un nodo de huffman
HuffmanNode* crear_nodo(unsigned char data, unsigned freq) {
    HuffmanNode* node = (HuffmanNode*) malloc(sizeof(HuffmanNode));
    node->data = data;
    node->freq = freq;
    node->left = node->right = NULL;
    return node;
}

// crear un min-heap
MinHeap* crear_min_heap(int capacity) {
    MinHeap* min_heap = (MinHeap*) malloc(sizeof(MinHeap));
    min_heap->size = 0;
    min_heap->capacity = capacity;
    min_heap->array = (HuffmanNode**) malloc(capacity * sizeof(HuffmanNode*));
    return min_heap;
}

// intercambiar nodos
void intercambiar_nodos(HuffmanNode** a, HuffmanNode** b) {
    HuffmanNode* t = *a;
    *a = *b;
    *b = t;
}

// mantener propiedad de min-heap
void min_heapify(MinHeap* min_heap, int idx) {
    int smallest = idx;
    int left = 2 * idx + 1;
    int right = 2 * idx + 2;

    if (left < min_heap->size && min_heap->array[left]->freq < min_heap->array[smallest]->freq)
        smallest = left;
    if (right < min_heap->size && min_heap->array[right]->freq < min_heap->array[smallest]->freq)
        smallest = right;

    if (smallest != idx) {
        intercambiar_nodos(&min_heap->array[smallest], &min_heap->array[idx]);
        min_heapify(min_heap, smallest);
    }
}

// extraer nodo con menor frecuencia
HuffmanNode* extraer_min(MinHeap* min_heap) {
    HuffmanNode* temp = min_heap->array[0];
    min_heap->array[0] = min_heap->array[min_heap->size - 1];
    min_heap->size--;
    min_heapify(min_heap, 0);
    return temp;
}

// insertar nodo en min-heap
void insertar_min_heap(MinHeap* min_heap, HuffmanNode* node) {
    int i = min_heap->size++;
    while (i && node->freq < min_heap->array[(i - 1) / 2]->freq) {
        min_heap->array[i] = min_heap->array[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    min_heap->array[i] = node;
}

// construir min-heap a partir de frecuencias
MinHeap* construir_min_heap(int freq[]) {
    MinHeap* min_heap = crear_min_heap(NUM_CHARS);
    for (int i = 0; i < NUM_CHARS; i++) {
        if (freq[i] > 0) {
            min_heap->array[min_heap->size] = crear_nodo((unsigned char)i, freq[i]);
            min_heap->size++;
        }
    }
    for (int i = (min_heap->size - 2) / 2; i >= 0; i--) {
        min_heapify(min_heap, i);
    }
    return min_heap;
}

// construir arbol de huffman
HuffmanNode* construir_arbol_huffman(int freq[]) {
    MinHeap* min_heap = construir_min_heap(freq);
    if (min_heap->size == 0) return NULL;

    while (min_heap->size > 1) {
        HuffmanNode* left = extraer_min(min_heap);
        HuffmanNode* right = extraer_min(min_heap);
        HuffmanNode* top = crear_nodo('$', left->freq + right->freq);
        top->left = left;
        top->right = right;
        insertar_min_heap(min_heap, top);
    }
    return extraer_min(min_heap);
}

// =================== generacion de codigos ===================
void generar_codigos(HuffmanNode* root, char* code, int depth, char* codes[NUM_CHARS]) {
    if (!root) return;

    if (!root->left && !root->right) {
        code[depth] = '\0';
        codes[root->data] = strdup(code);
        return;
    }
    code[depth] = '0';
    generar_codigos(root->left, code, depth + 1, codes);
    code[depth] = '1';
    generar_codigos(root->right, code, depth + 1, codes);
}

// =================== compresion ===================
void comprimir_archivo(const char* filepath, FILE* out, char* codes[NUM_CHARS]) {
    FILE* in = fopen(filepath, "rb");
    if (!in) {
        perror("error al abrir archivo");
        return;
    }

    unsigned char buffer = 0;
    int bit_count = 0;
    int c;

    while ((c = fgetc(in)) != EOF) {
        char* code = codes[(unsigned char)c];
        for (int i = 0; code[i] != '\0'; i++) {
            buffer = (buffer << 1) | (code[i] - '0');
            bit_count++;
            if (bit_count == 8) {
                fputc(buffer, out);
                bit_count = 0;
                buffer = 0;
            }
        }
    }

    if (bit_count > 0) {
        buffer <<= (8 - bit_count);
        fputc(buffer, out);
    }

    fclose(in);
}

// =================== recoleccion de frecuencias ===================
void recolectar_frecuencias(const char* filepath, int freq[]) {
    FILE* in = fopen(filepath, "rb");
    if (!in) {
        perror("error al abrir archivo");
        return;
    }
    int c;
    while ((c = fgetc(in)) != EOF) {
        freq[(unsigned char)c]++;
    }
    fclose(in);
}

// =================== datos para hilos ===================
typedef struct {
    int hilo_id;
    char filepath[512];
    char filename[256];
    FILE* out;
    char** codes;
    EstadisticasHilo* stats;
    pthread_mutex_t* mutex_salida;
    pthread_mutex_t* mutex_consola;
} HiloArgs;

// =================== variables globales para estadisticas ===================
static int total_archivos_procesados = 0;
pthread_mutex_t mutex_contador = PTHREAD_MUTEX_INITIALIZER;

// funcion que ejecuta cada hilo
void* tarea_hilo(void* arg) {
    HiloArgs* datos = (HiloArgs*) arg;
    datos->stats->tiempo_inicio = clock();
    datos->stats->thread_id = pthread_self();


    // obtener tamaño de archivo
    struct stat st;
    if (stat(datos->filepath, &st) != 0) {
        pthread_mutex_lock(datos->mutex_consola);
        printf("Hilo #%d: Error al obtener estadísticas de %s\n", 
               datos->hilo_id, datos->filename);
        fflush(stdout);
        pthread_mutex_unlock(datos->mutex_consola);
        return NULL;
    }
    
    uint64_t file_size = st.st_size;
    int name_len = strlen(datos->filename);

    // bloquear escritura en archivo
    pthread_mutex_lock(datos->mutex_salida);

    // escribir metadatos
    fwrite(&name_len, sizeof(int), 1, datos->out);
    fwrite(datos->filename, 1, name_len, datos->out);
    fwrite(&file_size, sizeof(uint64_t), 1, datos->out);

    // escribir datos comprimidos
    comprimir_archivo(datos->filepath, datos->out, datos->codes);

    // liberar bloqueo
    pthread_mutex_unlock(datos->mutex_salida);

    // actualizar estadísticas
    datos->stats->archivos_procesados = 1;
    datos->stats->tiempo_fin = clock();
    
    // actualizar contador global
    pthread_mutex_lock(&mutex_contador);
    total_archivos_procesados++;
    pthread_mutex_unlock(&mutex_contador);

    return NULL;
}

// =================== compresor principal ===================
int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("uso: %s <directorio_entrada> <archivo_salida>\n", argv[0]);
        return 1;
    }

    // empezar medicion de tiempo
    clock_t tiempo_inicio = clock();

    const char* dir_entrada = argv[1];
    const char* archivo_salida = argv[2];

    int freq[NUM_CHARS] = {0};
    DIR* dir = opendir(dir_entrada);
    if (!dir) {
        perror("no se pudo abrir el directorio");
        return 1;
    }

    //paso 1 contar archivos y calcular frecuencias globales
    struct dirent* entry;
    int num_archivos = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { 
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", dir_entrada, entry->d_name);
            recolectar_frecuencias(filepath, freq);
            num_archivos++;
        }
    }
    closedir(dir);

    if (num_archivos == 0) {
        printf("No se encontraron archivos en el directorio\n");
        return 1;
    }

    // paso 2 construir arbol huffman
    HuffmanNode* root = construir_arbol_huffman(freq);
    if (!root) {
        printf("Error: No se pudo construir el árbol Huffman\n");
        return 1;
    }
    
    char* codes[NUM_CHARS] = {0};
    char code[256];
    generar_codigos(root, code, 0, codes);

    // paso 3: crear archivo de salida
    FILE* out = fopen(archivo_salida, "wb");
    if (!out) {
        perror("no se pudo crear archivo de salida");
        return 1;
    }

    // escribir tabla de frecuencias
    fwrite(freq, sizeof(int), NUM_CHARS, out);
    // escribir numero de archivos
    fwrite(&num_archivos, sizeof(int), 1, out);

    // paso 4: configurar hilos y mutexes
    pthread_t hilos[MAX_HILOS];
    HiloArgs* args_hilos[MAX_HILOS];
    EstadisticasHilo stats_hilos[MAX_HILOS];
    int hilos_activos = 0;
    int hilo_contador = 0;

    pthread_mutex_t mutex_salida = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mutex_consola = PTHREAD_MUTEX_INITIALIZER;

    // procesar archivos con hilos
    dir = opendir(dir_entrada);
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            HiloArgs* args = malloc(sizeof(HiloArgs));
            args->hilo_id = ++hilo_contador;
            snprintf(args->filepath, sizeof(args->filepath), "%s/%s", dir_entrada, entry->d_name);
            strncpy(args->filename, entry->d_name, sizeof(args->filename) - 1);
            args->filename[sizeof(args->filename) - 1] = '\0';
            args->out = out;
            args->codes = codes;
            args->stats = &stats_hilos[hilos_activos];
            args->mutex_salida = &mutex_salida;
            args->mutex_consola = &mutex_consola;
            args_hilos[hilos_activos] = args;

            // inicializar estadísticas
            memset(&stats_hilos[hilos_activos], 0, sizeof(EstadisticasHilo));
            stats_hilos[hilos_activos].hilo_id = args->hilo_id;

            pthread_create(&hilos[hilos_activos], NULL, tarea_hilo, args);
            hilos_activos++;

            // si alcanzamos el máximo de hilos, esperar a que terminen
            if (hilos_activos >= MAX_HILOS) {
                for (int i = 0; i < hilos_activos; i++) {
                    pthread_join(hilos[i], NULL);
                    free(args_hilos[i]);
                }
                hilos_activos = 0;
            }
        }
    }
    closedir(dir);

    // esperar hilos restantes
    for (int i = 0; i < hilos_activos; i++) {
        pthread_join(hilos[i], NULL);
        free(args_hilos[i]);
    }

    fclose(out);

    // mostrar estadísticas finales
    printf("Total de archivos procesados: %d\n", total_archivos_procesados);
    printf("Total de hilos utilizados: %d\n", hilo_contador);
    
    // cleanup
    pthread_mutex_destroy(&mutex_salida);
    pthread_mutex_destroy(&mutex_consola);
    pthread_mutex_destroy(&mutex_contador);

    // liberar memoria de códigos
    for (int i = 0; i < NUM_CHARS; i++) {
        if (codes[i]) free(codes[i]);
    }

    // fin medicion de tiempo
    clock_t tiempo_fin = clock();
    double tiempo_ms = ((double)(tiempo_fin - tiempo_inicio) / CLOCKS_PER_SEC) * 1000.0;

    printf("Compresión completada en: %.2f ms\n", tiempo_ms);
    printf("Archivos comprimidos en: %s\n", archivo_salida);

    return 0;
}