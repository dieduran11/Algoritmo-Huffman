#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    int opcion;
    char nombreDirectorio[256];
    char comando[512];
    printf("Seleccione el método de descompresión:\n");
    printf("1. Serial\n");
    printf("2. Con hilos (pthreads)\n");
    printf("3. Con procesos (fork)\n");
    printf("Ingrese una opción (1-3): ");
    scanf("%d", &opcion);
    printf("Ingrese el nombre del directorio de salida: ");
    scanf("%s", nombreDirectorio);

    switch(opcion) {
        case 1:
            snprintf(comando, sizeof(comando), "./descompresor salida.huff %s", nombreDirectorio);
            break;
        case 2:
            snprintf(comando, sizeof(comando), "./descompresorThreads salida.huff %s", nombreDirectorio);
            break;
        case 3:
            snprintf(comando, sizeof(comando), "./descompresorFork salida.huff %s", nombreDirectorio);
            break;
        default:
            printf("Opción no válida.\n");
            return 1;
    }
    printf("Ejecutando: %s\n", comando);
    int res = system(comando);
    if (res != 0) {
        printf("Error al ejecutar el descompresor.\n");
        return 1;
    }
    return 0;
}
