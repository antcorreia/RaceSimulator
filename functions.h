//Antonio Correia 2019216646
//Pedro Martins 2019216826

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include <assert.h>

#define PIPE_NAME "Teste"
#define READ 0
#define WRITE 1
#define TAMANHO 100
#define MAXCARROS 100
#define MSG_SIZE 1000
#define STRING_SIZE 1000

typedef struct{
    int num;
    int speed;
    double consumption;
    int reliability; //Tem de ser double
    char state[100];
    char teamname[100];
    int avaria;
    int id;
    double tempo;
    int voltas_completas;
    int distancia_percorrida;
    int distancia_volta;
    int num_pitstops;
    double deposito; //Tem de ser double
    char modo[100];

}carro;

typedef struct{
	int id;
	char teamname[100];
	int threadid;
}pass_argument;

typedef struct{
	long messagetype;
	int num;
}messagequeue;

typedef struct{
    char nome[100];
    carro *listaDeCarros;
    int go;
    char boxstate[100];
    int threads_created;
    pid_t pid;

}equipa;

typedef struct{
    equipa *listaDeEquipas;
    int total_avarias;
    int total_abastecimentos;
    int equipas_counter;
    int threads_adicionadas;
    int carros_terminados;
    int global;
    int newcar;
    int restart;
    pthread_cond_t start;
    pthread_cond_t fim;
    pthread_mutex_t mutex;
    pthread_cond_t box;
    int mqid;
    pid_t pidavarias;
    pid_t pidcorrida;

}mainstruct;

void logoutput();
bool recebeConfiguracoes(char* configurations);
void inicializaSharedMemory();
void inicializaSemaforos();
void printSharedMemory();
void process_maker();
void gestordecorridas();
int checkIfPossibleEquipa();
int checkIfPossibleCar();
void fimdacorrida();
void create_thread();
void gestordeavarias();
void imprimirestatisticas();
void gestordeequipas(int equipas_counter2);
void *carro_simulacao();
void simulacao();
void startrace();
void terminar();
void SIGUSR1handler();
int verify_atoi(char *num);

#ifndef PROJETO_FUNCOES_H
#define PROJETO_FUNCOES_H

#endif
