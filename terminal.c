#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    int opcion;
    do {
        printf("\n--- Menú de Huffman ---\n");
        printf("1. Compresor serial\n");
        printf("2. Compresor con hilos\n");
        printf("3. Compresor con fork\n");
        printf("4. Descompresor serial\n");
        printf("5. Descompresor con hilos\n");
        printf("6. Descompresor con fork\n");
        printf("7. Salir\n");
        printf("Seleccione una opción: ");
        scanf("%d", &opcion);

        switch(opcion) {
            case 1:
                printf("Ejecutando compresor serial...\n");
                system("./compresor ./textos ./salida.huff");
                break;
            case 2:
                printf("Ejecutando compresor-hilos...\n");
                system("./compresor-hilos ./textos ./salida.huff");
                break;
            case 3:
                printf("Ejecutando compresor-fork...\n");
                system("./compresor-fork ./textos ./salida.huff");
                break;
            case 4:
                printf("Ejecutando descompresor serial...\n");
                system("./descompresor ./salida.huff ./textos_salida");
                break;
            case 5:
                printf("Ejecutando descompresor-hilos...\n");
                system("./descompresorThreads ./salida.huff ./textos_salida");
                break;
            case 6:
                printf("Ejecutando descompresor-fork...\n");
                system("./descompresorFork ./salida.huff ./textos_salida");
                break;
            case 7:
                printf("Saliendo...\n");
                break;
            default:
                printf("Opción inválida\n");
        }
    } while(opcion != 7);

    return 0;
}
