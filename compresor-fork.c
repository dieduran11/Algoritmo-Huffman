#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>

#define NUM_CHARS 256
#define MAX_FILES 1000
#define MAX_FILENAME 256

// =================== nodo de huffman ===================
typedef struct HuffmanNode {
    unsigned char dato;
    unsigned frecuencia;
    struct HuffmanNode *izquierda, *derecha;
} HuffmanNode;

// =================== min-heap ===================
typedef struct {
    int tamano;
    int capacidad;
    HuffmanNode** array;
} MinHeap;

// =================== estructura para archivos ===================
typedef struct {
    char nombre[MAX_FILENAME];
    char ruta[512];
    uint64_t tamano;
} ArchivoInfo;

// =================== memoria compartida para frecuencias ===================
typedef struct {
    int frecuencias[NUM_CHARS];
    int archivos_procesados;
    int mutex; // simple mutex usando variable compartida
} MemoriaCompartida;

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

// =================== mutex simple usando busy waiting ===================
void lock_mutex(volatile int* mutex) {
    while (__sync_lock_test_and_set(mutex, 1)) {
        // busy wait
        usleep(1); // pequeña pausa para evitar uso excesivo de CPU
    }
}

void unlock_mutex(volatile int* mutex) {
    __sync_lock_release(mutex);
}

// =================== recoleccion de frecuencias por archivo ===================
void procesar_archivo_individual(const char* ruta_archivo, MemoriaCompartida* memoria_compartida) {
    int frecuencias_locales[NUM_CHARS] = {0};
    
    // procesar archivo
    FILE* entrada = fopen(ruta_archivo, "rb");
    if (!entrada) {
        perror("error al abrir archivo");
        return;
    }
    
    int c;
    while ((c = fgetc(entrada)) != EOF) {
        frecuencias_locales[(unsigned char)c]++;
    }
    fclose(entrada);
    
    // agregar frecuencias locales a la memoria compartida (sección crítica)
    lock_mutex(&memoria_compartida->mutex);
    for (int i = 0; i < NUM_CHARS; i++) {
        memoria_compartida->frecuencias[i] += frecuencias_locales[i];
    }
    memoria_compartida->archivos_procesados++;
    unlock_mutex(&memoria_compartida->mutex);
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

// =================== funcion principal ===================
int main(int argc, char* argv[]) {
    if (argc != 3) {
        return 1;
    }

    clock_t tiempo_inicio = clock();
    const char* directorio_entrada = argv[1];
    const char* archivo_salida = argv[2];

    // crear memoria compartida
    MemoriaCompartida* memoria_compartida = mmap(NULL, sizeof(MemoriaCompartida),
                                               PROT_READ | PROT_WRITE,
                                               MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (memoria_compartida == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    // inicializar memoria compartida
    memset(memoria_compartida->frecuencias, 0, sizeof(memoria_compartida->frecuencias));
    memoria_compartida->archivos_procesados = 0;
    memoria_compartida->mutex = 0;

    // recopilar información de archivos
    DIR* dir = opendir(directorio_entrada);
    if (!dir) {
        perror("no se pudo abrir el directorio");
        return 1;
    }

    ArchivoInfo archivos[MAX_FILES];
    int contador_archivos = 0;
    struct dirent* entrada_archivo;

    while ((entrada_archivo = readdir(dir)) != NULL && contador_archivos < MAX_FILES) {
        if (entrada_archivo->d_type == DT_REG) {
            strcpy(archivos[contador_archivos].nombre, entrada_archivo->d_name);
            snprintf(archivos[contador_archivos].ruta, sizeof(archivos[contador_archivos].ruta),
                    "%s/%s", directorio_entrada, entrada_archivo->d_name);
            
            struct stat st;
            stat(archivos[contador_archivos].ruta, &st);
            archivos[contador_archivos].tamano = st.st_size;
            contador_archivos++;
        }
    }
    closedir(dir);

    if (contador_archivos == 0) {
        printf("no se encontraron archivos en el directorio.\n");
        return 1;
    }


    // crear un fork por cada archivo
    pid_t* pids = malloc(contador_archivos * sizeof(pid_t));
    
    for (int i = 0; i < contador_archivos; i++) {
        pids[i] = fork();
        
        if (pids[i] < 0) {
            perror("fork failed");
            return 1;
        }
        else if (pids[i] == 0) {
            // proceso hijo - procesa un archivo específico
            procesar_archivo_individual(archivos[i].ruta, memoria_compartida);
            exit(0);
        }
        // proceso padre continúa creando más forks
    }

    // el proceso padre espera a que todos los procesos hijos terminen
    for (int i = 0; i < contador_archivos; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        if (WEXITSTATUS(status) != 0) {
            printf("Advertencia: el proceso hijo %d terminó con código de error %d\n", 
                   pids[i], WEXITSTATUS(status));
        }
    }
    
    free(pids);

    printf("Todos los archivos han sido procesados. Total procesados: %d\n", 
           memoria_compartida->archivos_procesados);

    // construir arbol Huffman con frecuencias combinadas
    HuffmanNode* raiz = construir_arbol_huffman(memoria_compartida->frecuencias);
    if (!raiz) {
        printf("Error: no se pudo construir el árbol Huffman\n");
        return 1;
    }

    char* codigos[NUM_CHARS] = {0};
    char codigo[256];
    generar_codigos(raiz, codigo, 0, codigos);

    // crear archivo de salida y comprimir
    FILE* salida = fopen(archivo_salida, "wb");
    if (!salida) {
        perror("no se pudo crear archivo de salida");
        return 1;
    }

    // guardar tabla de frecuencias
    fwrite(memoria_compartida->frecuencias, sizeof(int), NUM_CHARS, salida);

    // guardar número de archivos
    fwrite(&contador_archivos, sizeof(int), 1, salida);

    // comprimir cada archivo
    printf("Comprimiendo archivos:\n");
    for (int i = 0; i < contador_archivos; i++) {
        int longitud_nombre = strlen(archivos[i].nombre);
        fwrite(&longitud_nombre, sizeof(int), 1, salida);
        fwrite(archivos[i].nombre, 1, longitud_nombre, salida);
        fwrite(&archivos[i].tamano, sizeof(uint64_t), 1, salida);
        
        comprimir_archivo(archivos[i].ruta, salida, codigos);
    }

    fclose(salida);

    // limpiar memoria compartida
    munmap(memoria_compartida, sizeof(MemoriaCompartida));

    // liberar memoria de códigos
    for (int i = 0; i < NUM_CHARS; i++) {
        if (codigos[i]) free(codigos[i]);
    }

    clock_t tiempo_fin = clock();
    double tiempo_ms = ((double)(tiempo_fin - tiempo_inicio) / CLOCKS_PER_SEC) * 1000.0;

    printf("Compresión completada en %.2f ms.\n", tiempo_ms);
    printf("Archivos comprimidos en: %s\n", archivo_salida);
    return 0;
}