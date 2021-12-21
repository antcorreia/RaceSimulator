//Antonio Correia 2019216646
//Pedro Martins 2019216826

#include "functions.h"

int main(){
    inicializaSemaforos();
    inicializaSharedMemory();
    process_maker();

    terminar();
    return 0;
}
