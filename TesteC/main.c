#include <stdio.h>
#include "calculos.h"

int main() {

    int resultado = soma(10, 5);
    printf("Soma: %d\n", resultado);
    resultado = multiplicacao(10, 5);
    printf("Multiplicacao: %d\n", resultado);

    return 0;
}