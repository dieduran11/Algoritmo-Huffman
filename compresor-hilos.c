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
#define MAX_FILES 10000
#define BUFFER_SIZE 64*1024  // Buffer de 64KB para I/O más eficiente

// =================== nodo de huffman ===================
typedef struct HuffmanNode {
    unsigned char data;       
    unsigned freq;            
    struct HuffmanNode *left, *right;
} HuffmanNode;

// =================== min-heap ===================
typedef struct {
    int size;
    int capacity;
    HuffmanNode** array;
} MinHeap;

// =================== cola de trabajo thread-safe ===================
typedef struct {
    char filepath[512];
    char filename[256];
    uint64_t filesize;
    int file_index;
} TareaArchivo;

typedef struct {
    TareaArchivo* tareas;
    int capacidad;
    int count;
    int head;
    int tail;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int finished; // flag para indicar que no hay más trabajo
} ColaTrabajos;

// =================== estructura para estadisticas de hilos ===================
typedef struct {
    int hilo_id;
    pthread_t thread_id;
    int archivos_procesados;
    clock_t tiempo_inicio;
    clock_t tiempo_fin;
    uint64_t bytes_procesados;
} EstadisticasHilo;

// =================== datos comprimidos por archivo ===================
typedef struct {
    char filename[256];
    uint64_t original_size;
    unsigned char* data_comprimida;
    size_t tamano_comprimido;
    int file_index;
} ArchivoComprimido;

// Variables globales
ColaTrabajos* cola_trabajos;
ArchivoComprimido* archivos_comprimidos;
pthread_mutex_t mutex_archivos_comprimidos = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_consola = PTHREAD_MUTEX_INITIALIZER;
int total_archivos_procesados = 0;
int total_archivos = 0;

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

// generacion de codigos
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

// =================== cola de trabajos ===================
ColaTrabajos* crear_cola_trabajos(int capacidad) {
    ColaTrabajos* cola = malloc(sizeof(ColaTrabajos));
    cola->tareas = malloc(capacidad * sizeof(TareaArchivo));
    cola->capacidad = capacidad;
    cola->count = 0;
    cola->head = 0;
    cola->tail = 0;
    cola->finished = 0;
    pthread_mutex_init(&cola->mutex, NULL);
    pthread_cond_init(&cola->not_empty, NULL);
    pthread_cond_init(&cola->not_full, NULL);
    return cola;
}

void agregar_tarea(ColaTrabajos* cola, TareaArchivo* tarea) {
    pthread_mutex_lock(&cola->mutex);
    
    while (cola->count == cola->capacidad) {
        pthread_cond_wait(&cola->not_full, &cola->mutex);
    }
    
    cola->tareas[cola->tail] = *tarea;
    cola->tail = (cola->tail + 1) % cola->capacidad;
    cola->count++;
    
    pthread_cond_signal(&cola->not_empty);
    pthread_mutex_unlock(&cola->mutex);
}

int obtener_tarea(ColaTrabajos* cola, TareaArchivo* tarea) {
    pthread_mutex_lock(&cola->mutex);
    
    while (cola->count == 0 && !cola->finished) {
        pthread_cond_wait(&cola->not_empty, &cola->mutex);
    }
    
    if (cola->count == 0 && cola->finished) {
        pthread_mutex_unlock(&cola->mutex);
        return 0; // no hay más trabajo
    }
    
    *tarea = cola->tareas[cola->head];
    cola->head = (cola->head + 1) % cola->capacidad;
    cola->count--;
    
    pthread_cond_signal(&cola->not_full);
    pthread_mutex_unlock(&cola->mutex);
    return 1; // tarea obtenida
}

void finalizar_cola(ColaTrabajos* cola) {
    pthread_mutex_lock(&cola->mutex);
    cola->finished = 1;
    pthread_cond_broadcast(&cola->not_empty);
    pthread_mutex_unlock(&cola->mutex);
}

// =================== compresion optimizada ===================
size_t comprimir_archivo_a_memoria(const char* filepath, char* codes[NUM_CHARS], 
                                   unsigned char** buffer_salida) {
    FILE* in = fopen(filepath, "rb");
    if (!in) return 0;
    
    // obtener tamaño del archivo
    fseek(in, 0, SEEK_END);
    long file_size = ftell(in);
    fseek(in, 0, SEEK_SET);
    
    // estimar tamaño comprimido máximo
    size_t max_compressed_size = file_size * 2; // estimación conservadora
    *buffer_salida = malloc(max_compressed_size);
    if (!*buffer_salida) {
        fclose(in);
        return 0;
    }
    
    unsigned char buffer = 0;
    int bit_count = 0;
    size_t output_pos = 0;
    unsigned char input_buffer[BUFFER_SIZE];
    size_t bytes_read;
    
    while ((bytes_read = fread(input_buffer, 1, BUFFER_SIZE, in)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            char* code = codes[input_buffer[i]];
            if (!code) continue;
            
            for (int j = 0; code[j] != '\0'; j++) {
                buffer = (buffer << 1) | (code[j] - '0');
                bit_count++;
                if (bit_count == 8) {
                    (*buffer_salida)[output_pos++] = buffer;
                    bit_count = 0;
                    buffer = 0;
                }
            }
        }
    }
    
    if (bit_count > 0) {
        buffer <<= (8 - bit_count);
        (*buffer_salida)[output_pos++] = buffer;
    }
    
    fclose(in);
    return output_pos;
}

// recoleccion de frecuencias optimizada
void recolectar_frecuencias_optimizada(const char* filepath, int freq[]) {
    FILE* in = fopen(filepath, "rb");
    if (!in) return;
    
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, in)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            freq[buffer[i]]++;
        }
    }
    fclose(in);
}

// =================== datos para hilos ===================
typedef struct {
    int hilo_id;
    char** codes;
    EstadisticasHilo* stats;
} HiloArgs;

// funcion que ejecuta cada hilo
void* tarea_hilo(void* arg) {
    HiloArgs* datos = (HiloArgs*) arg;
    datos->stats->tiempo_inicio = clock();
    datos->stats->thread_id = pthread_self();
    datos->stats->archivos_procesados = 0;
    datos->stats->bytes_procesados = 0;
    
    TareaArchivo tarea;
    while (obtener_tarea(cola_trabajos, &tarea)) {
        // comprimir archivo a memoria
        unsigned char* data_comprimida;
        size_t tamano_comprimido = comprimir_archivo_a_memoria(tarea.filepath, 
                                                              datos->codes, 
                                                              &data_comprimida);
        
        if (tamano_comprimido > 0) {
            // almacenar resultado comprimido
            pthread_mutex_lock(&mutex_archivos_comprimidos);
            strcpy(archivos_comprimidos[tarea.file_index].filename, tarea.filename);
            archivos_comprimidos[tarea.file_index].original_size = tarea.filesize;
            archivos_comprimidos[tarea.file_index].data_comprimida = data_comprimida;
            archivos_comprimidos[tarea.file_index].tamano_comprimido = tamano_comprimido;
            archivos_comprimidos[tarea.file_index].file_index = tarea.file_index;
            total_archivos_procesados++;
            pthread_mutex_unlock(&mutex_archivos_comprimidos);
            
            datos->stats->archivos_procesados++;
            datos->stats->bytes_procesados += tarea.filesize;
            
        }
    }
    
    datos->stats->tiempo_fin = clock();
    return NULL;
}

// =================== compresor principal ===================
int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("uso: %s <directorio_entrada> <archivo_salida>\n", argv[0]);
        return 1;
    }

    clock_t tiempo_inicio = clock();
    const char* dir_entrada = argv[1];
    const char* archivo_salida = argv[2];

    // paso 1: contar archivos
    DIR* dir = opendir(dir_entrada);
    if (!dir) {
        perror("no se pudo abrir el directorio");
        return 1;
    }

    struct dirent* entry;
    total_archivos = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            total_archivos++;
        }
    }
    closedir(dir);

    if (total_archivos == 0) {
        printf("No se encontraron archivos en el directorio\n");
        return 1;
    }


    // paso 2: calcular frecuencias globales
    int freq[NUM_CHARS] = {0};
    dir = opendir(dir_entrada);
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", dir_entrada, entry->d_name);
            recolectar_frecuencias_optimizada(filepath, freq);
        }
    }
    closedir(dir);

    // paso 3: construir arbol huffman
    HuffmanNode* root = construir_arbol_huffman(freq);
    if (!root) {
        printf("Error: No se pudo construir el árbol Huffman\n");
        return 1;
    }
    
    char* codes[NUM_CHARS] = {0};
    char code[256];
    generar_codigos(root, code, 0, codes);

    // paso 4: preparar estructuras de datos
    cola_trabajos = crear_cola_trabajos(total_archivos);
    archivos_comprimidos = calloc(total_archivos, sizeof(ArchivoComprimido));
    
    // llenar cola de trabajos
    dir = opendir(dir_entrada);
    int file_index = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            TareaArchivo tarea;
            snprintf(tarea.filepath, sizeof(tarea.filepath), "%s/%s", dir_entrada, entry->d_name);
            strcpy(tarea.filename, entry->d_name);
            tarea.file_index = file_index;
            
            struct stat st;
            stat(tarea.filepath, &st);
            tarea.filesize = st.st_size;
            
            agregar_tarea(cola_trabajos, &tarea);
            file_index++;
        }
    }
    closedir(dir);

    // paso 5: crear todos los hilos inmediatamente
    pthread_t hilos[MAX_HILOS];
    HiloArgs args_hilos[MAX_HILOS];
    EstadisticasHilo stats_hilos[MAX_HILOS];

    for (int i = 0; i < MAX_HILOS; i++) {
        args_hilos[i].hilo_id = i + 1;
        args_hilos[i].codes = codes;
        args_hilos[i].stats = &stats_hilos[i];
        memset(&stats_hilos[i], 0, sizeof(EstadisticasHilo));
        stats_hilos[i].hilo_id = i + 1;
        
        pthread_create(&hilos[i], NULL, tarea_hilo, &args_hilos[i]);
    }

    // marcar que no hay más trabajo
    finalizar_cola(cola_trabajos);

    // esperar a que todos los hilos terminen
    for (int i = 0; i < MAX_HILOS; i++) {
        pthread_join(hilos[i], NULL);
    }

    // paso 6: escribir archivo de salida
    FILE* out = fopen(archivo_salida, "wb");
    if (!out) {
        perror("no se pudo crear archivo de salida");
        return 1;
    }

    // escribir metadatos
    fwrite(freq, sizeof(int), NUM_CHARS, out);
    fwrite(&total_archivos, sizeof(int), 1, out);

    // escribir archivos comprimidos en orden
    for (int i = 0; i < total_archivos; i++) {
        int name_len = strlen(archivos_comprimidos[i].filename);
        fwrite(&name_len, sizeof(int), 1, out);
        fwrite(archivos_comprimidos[i].filename, 1, name_len, out);
        fwrite(&archivos_comprimidos[i].original_size, sizeof(uint64_t), 1, out);
        fwrite(archivos_comprimidos[i].data_comprimida, 1, 
               archivos_comprimidos[i].tamano_comprimido, out);
        
        free(archivos_comprimidos[i].data_comprimida);
    }

    fclose(out);

    // mostrar estadísticas finales
    int hilos_activos = 0;
    uint64_t total_bytes = 0;
    for (int i = 0; i < MAX_HILOS; i++) {
        if (stats_hilos[i].archivos_procesados > 0) {
            hilos_activos++;
            total_bytes += stats_hilos[i].bytes_procesados;
        }
    }
    printf("Hilos activos utilizados: %d/%d\n", hilos_activos, MAX_HILOS);
    printf("Total archivos procesados: %d\n", total_archivos_procesados);

    // cleanup
    free(archivos_comprimidos);
    free(cola_trabajos->tareas);
    free(cola_trabajos);

    for (int i = 0; i < NUM_CHARS; i++) {
        if (codes[i]) free(codes[i]);
    }

    clock_t tiempo_fin = clock();
    double tiempo_ms = ((double)(tiempo_fin - tiempo_inicio) / CLOCKS_PER_SEC) * 1000.0;

    printf("Compresión completada en: %.2f ms\n", tiempo_ms);
    printf("Archivos comprimidos en: %s\n", archivo_salida);

    return 0;
}