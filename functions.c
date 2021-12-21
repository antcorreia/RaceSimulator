//Antonio Correia 2019216646
//Pedro Martins 2019216826

#include "funcoes.h"

sem_t *semaforoLogs;
sem_t *semaforoSHM;
sem_t *semaforoMSQ;
sem_t *semaforoBOX;
mainstruct* sharedmemory;
carro lastcar_added;
int main_pid;
int shmid, unidades, dist, n_voltas, n_equipas, max_carros, t_avaria, t_box_min, t_box_max, deposito;
int fd, fdunnamed[TAMANHO][2], fdbox[TAMANHO][2];
pthread_t thread_id[MAXCARROS];


void logoutput(char* string){
	sem_wait(semaforoLogs);
	
    time_t tempo;
    time(&tempo);

    FILE* f = fopen("log.txt", "a+");
    fprintf(f, "%s - %s\n", strtok(asctime(localtime(&tempo)), "\n"), string);
    fclose(f);
    printf("%s\n", string);
    
    sem_post(semaforoLogs);
}

bool recebeConfiguracoes(char* configuracoes){
	char* line = NULL;
    size_t len = 0;
    int count = 1;
    char check;

    FILE* f = fopen(configuracoes, "r");
    if (f == NULL){
        char string[1000];
		sprintf(string, "Ficheiro %s nao encontrado", configuracoes);
		logoutput(string);
        return false;
    }
    
    for (int c = getc(f); c != EOF; c = getc(f))
        if (c == '\n')
            count = count + 1;
    fclose(f);

    if (count < 8){
        logoutput("Ficheiro nao tem linhas suficientes\n");
        return false;
    }
    else if (count > 8){
        logoutput("Ficheiro tem demasiadas linhas\n");
        return false;
    }

    f = fopen(configuracoes, "r");

    getline(&line, &len, f);
    sscanf(line, "%d%c", &unidades, &check);
    if ((unidades == 0) || ((check != '\n') && (check != '\r'))){
        logoutput("Primeira linha do ficheiro mal estruturada\n");
        return false;
    }

    getline(&line, &len, f);
    sscanf(line, "%d, %d%c", &dist, &n_voltas, &check);
    if ((dist == 0) || (n_voltas == 0) || ((check != '\n') && (check != '\r'))){
        logoutput("Segunda linha do ficheiro mal estruturada\n");
        return false;
    }

    getline(&line, &len, f);
    sscanf(line, "%d%c", &n_equipas, &check);
    if ((n_equipas == 0) || ((check != '\n') && (check != '\r'))){
        logoutput("Terceira linha do ficheiro mal estruturada\n");
        return false;
    }
    if (n_equipas < 3){
    	logoutput("Numero de equipas tem de ser igual ou superior a 3");
    	return false;
    }
    
    getline(&line, &len, f);
    sscanf(line, "%d%c", &max_carros, &check);
    if ((max_carros == 0) || ((check != '\n') && (check != '\r'))){
        logoutput("Quarta linha do ficheiro mal estruturada\n");
        return false;
    }


    getline(&line, &len, f);
    sscanf(line, "%d%c", &t_avaria, &check);
    if ((t_avaria == 0) || ((check != '\n') && (check != '\r'))){
        logoutput("Quinta linha do ficheiro mal estruturada\n");
        return false;
    }


    getline(&line, &len, f);
    sscanf(line, "%d, %d%c", &t_box_min, &t_box_max, &check);
    if ((t_box_min == 0) || (t_box_max == 0) || ((check != '\n') && (check != '\r'))){
        logoutput("Sexta linha do ficheiro mal estruturada\n");
        return false;
    }


    getline(&line, &len, f);
    sscanf(line, "%d%c", &deposito, &check);
    if ((deposito == 0) || ((check != '\n') && (check != '\r'))){
        logoutput("Setima linha do ficheiro mal estruturada\n");
        return false;
    }
    
    return true;
}

void inicializaSharedMemory(){
	#ifdef DEBUG
    printf("Ler e validar as configuracoes iniciais.\n");
    #endif
	if (!recebeConfiguracoes("configuracoes.txt"))
    	exit(0);
    
    #ifdef DEBUG
    printf("Criar e alocar a sharedmemory.\n");
   	#endif	
    printf("%d %d\n", n_equipas, max_carros);
    shmid = shmget(IPC_PRIVATE, sizeof(mainstruct) + n_equipas*sizeof(equipa) + n_equipas*max_carros*sizeof(carro) + 5*sizeof(int), IPC_CREAT|0777);
    if (shmid == -1){
        logoutput("Shared memory initialization failed");
        exit(0);
    }
    
    if ((sharedmemory = (mainstruct*) shmat(shmid, NULL, 0)) == (mainstruct*) - 1){
    	logoutput("Shared memory initialization failed");
        exit(0);
    }
    
    sharedmemory->listaDeEquipas = (equipa*) (sharedmemory + 1);
    for (int i = 0; i < n_equipas; i++){
    	sharedmemory->listaDeEquipas[i].listaDeCarros = (carro*) (sharedmemory->listaDeEquipas + n_equipas + i*max_carros);
    }	
    
}

void inicializaSemaforos(){
  	sem_unlink("SemaforoLogs");
  	sem_unlink("SemaforoMSQ");
  	sem_unlink("SemaforoSHM");
  	sem_unlink("SemaforoBOX");
  	
	#ifdef DEBUG
    printf("Abrir os semáforos para a simulacao.\n");
    #endif
	semaforoLogs = sem_open("SemaforoLogs", O_CREAT, 0644, 1);
	semaforoSHM = sem_open("SemaforoSHM", O_CREAT|O_EXCL, 0644, 1);
	semaforoMSQ = sem_open("SemaforoMSQ", O_CREAT, 0644, 1);
	semaforoBOX = sem_open("SemaforoBOX", O_CREAT, 0644, 1);
}

void process_maker(){
	char string3[STRING_SIZE];
	int o = 0;
	#ifdef DEBUG
    printf("Abrir o ficheiro logs para escrita (w+).\n");
    #endif
	FILE* f = fopen("log.txt", "w+");
	fclose(f);
	
	#ifdef DEBUG
    printf("Alterar a execucao de alguns sinais e ignorar o resto.\n");
    #endif
	signal(SIGINT, fimdacorrida);
	signal(SIGTSTP, imprimirestatisticas);
	
	for(int i=1; i<31; i++){
    	if((i!=2) && (i!=20)){
      		signal(i, SIG_IGN);
    	}
  	}
	
	#ifdef DEBUG
    printf("Inicializar mutex e variável de condicao.\n");
    #endif
	pthread_mutexattr_t mutexattr;
	pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);
	pthread_condattr_t condattr;
	pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);
	pthread_condattr_t condattrfim;
	pthread_condattr_setpshared(&condattrfim, PTHREAD_PROCESS_SHARED);
	pthread_condattr_t condattrbox;
	pthread_condattr_setpshared(&condattrbox, PTHREAD_PROCESS_SHARED);
	
	
	pthread_mutex_init(&sharedmemory->mutex, &mutexattr);
	pthread_cond_init(&sharedmemory->start, &condattr);
	pthread_cond_init(&sharedmemory->fim, &condattrfim);
	pthread_cond_init(&sharedmemory->box, &condattrbox);
	
	assert( (sharedmemory->mqid = msgget(IPC_PRIVATE, IPC_CREAT|0777)) != -1 );
	
	printf("Bem vindo ao nosso simulador de corridas.\n"
			"Utilize o comando 'ADDCAR (Nome da equipa) (Numero) (Velocidade) (Consumo) (Fidelidade)' para adicionar carros as equipas.\n"
			"Quando estiver pronto pode escrever 'START RACE' para comecar a simulacao.\n\n");
	
	char string[STRING_SIZE], string2[STRING_SIZE];
	
	main_pid = getpid();
    
    if (fork() == 0){
    	#ifdef DEBUG
    	printf("Criar o processo Gestor de Corridas.\n");
    	#endif
        gestordecorridas();
    }
    else{
    	if (fork() == 0){
    		#ifdef DEBUG
    		printf("Criar o processo Gestor de Avarias.\n");
    		#endif
        	gestordeavarias();
        } 
        else{
        	#ifdef DEBUG
    		printf("Criar e abrir o named pipe.\n");
    		#endif
        	if ((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600)<0) && (errno != EEXIST)){
        		sprintf(string, "Erro a criar o named pipe!\n");
        		logoutput(string);
        		terminar();
        	}
        	
        	if ((fd = open(PIPE_NAME, O_WRONLY)) < 0){
        		sprintf(string, "Erro a abrir o named pipe para escrita!\n");
        		logoutput(string);
        		terminar();
        	}
        	
        	while (1){
        		fgets(string2, 100, stdin);
        		write(fd, string2, sizeof(string2));
        		sleep(0.5);
        		if (sharedmemory->global == 1){
        			break;
        		}	   	
        	}
        	
        	while(sharedmemory->global == 1 || sharedmemory->restart == 1){
        		fgets(string2, 100, stdin);
        		if (sharedmemory->restart == 1){
        			char* ptr = strtok(string2, " ");
		
					while(ptr != NULL){
						strcpy(string3[o++], ptr);
						ptr = strtok(NULL, " ");
					}
					
					if (strcmp(string3[0], "START") == 0 && strcmp(string3[1], "RACE!") == 0){
						write(fd, string2, sizeof(string2));
					}
        		} else {
        			printf("RACE ALREADY STARTED!\n");
        		}
        		o = 0;	
        	}	
        	
        	wait(NULL);
        	close(fd);		
        	imprimirestatisticas();
        }
    }
}

void gestordecorridas(){
	char string[STRING_SIZE], string2[STRING_SIZE], string3[STRING_SIZE][STRING_SIZE], string4[STRING_SIZE], message[MSG_SIZE];
	int count = 0, o=0, lenBuffer, posicao_carro;
	carro carro;
	fd_set read_set;
	equipa equipa;
	sharedmemory->pidcorrida = getpid();
	#ifdef DEBUG
	printf("GESTOR DE CORRIDA PID: %d\n", getpid());
	#endif
	
    if ((fd = open(PIPE_NAME, O_RDONLY)) < 0){
        sprintf(string, "Erro a abrir o named pipe para leitura!\n");
        logoutput(string);
        terminar();
    }
    
    while (sharedmemory->global == 0){
    	lenBuffer = read(fd, string2, sizeof(string2));
    	string2[lenBuffer] = '\0';
    	string2[strlen(string2)-1] = '\0';
    	strcpy(string4, string2);
        char* ptr = strtok(string2, " ");
		
		while(ptr != NULL){
			strcpy(string3[o++], ptr);
			ptr = strtok(NULL, " ");
		}
       	if(strcmp(string3[0],"START")==0){
        	if(strcmp(string3[1],"RACE")==0){
        		if (sharedmemory->equipas_counter < n_equipas){
        			sprintf(string, "Ainda nao introduziu equipas suficientes\n");
        			logoutput(string);
        		} else {
        			sharedmemory->global = 1;
        			sprintf(string, "Corrida a comecar\n");
        			logoutput(string);
        			sleep(1);
        			for (int i = 0; i < sharedmemory->equipas_counter; i++){
        				write(fdunnamed[i][1], &string, sizeof(string));
        			}
        			kill(sharedmemory->pidavarias, SIGUSR2);
        			//pthread_mutex_lock(&sharedmemory->mutex);
        			pthread_cond_broadcast(&sharedmemory->start);
        			pthread_mutex_unlock(&sharedmemory->mutex);
        			fflush(stdout);
        		}
        	} else {
        		sprintf(string, "Comando nao existente ou mal estruturado => %s\n", string4);
        		logoutput(string);		
        	}
        } 
        else if (strcmp(string3[0],"ADDCAR")==0){
        	if (strcmp(string3[5], "\0") == 0 || strcmp(string3[7], "\0") != 0)
        		count = 10;
        	
        	for (int i = 0; i < n_equipas; i++)
        		if ((sharedmemory->listaDeEquipas[i].nome != NULL) && (strcmp(string3[1],sharedmemory->listaDeEquipas[i].nome)==0)){
        			count++;
        			break;
        		}	
        	
        	strcpy(carro.teamname, string3[1]);
 			if(verify_atoi(string3[2])==1){
 				count = count + 2;
 				carro.num = atoi(string3[2]);
 				if(verify_atoi(string3[3])==1){
 					count = count + 2;
 					carro.speed = atoi(string3[3]);
 					if(verify_atoi(string3[4])==1){
 						count = count + 2;
 						carro.consumption = atoi(string3[4]);
 						if(verify_atoi(string3[5])==1){
 							count = count + 2;
 							carro.reliability = atoi(string3[5]);
 						}	
 					}
 				}
 			}
 			
 			if (count == 8){
 				if (checkIfPossibleEquipa() == 0){
 					sprintf(string, "Numero maximo de equipas ja atingido. Adicione carros a equipas ja existentes ou comece a corrida");
 					logoutput(string);
 					
 				} else {
 					sprintf(string, "Nova equipa criada e carro adicionado\n");
        			logoutput(string);
        			pipe(fdunnamed[sharedmemory->equipas_counter]);
        			pipe(fdbox[sharedmemory->equipas_counter]);
        			lastcar_added = carro;
        			strcpy(equipa.nome, string3[1]);
        			strcpy(equipa.boxstate, "Livre");
        			
        			sem_wait(semaforoSHM);				
        			strcpy(sharedmemory->listaDeEquipas[sharedmemory->equipas_counter].nome, string3[1]);
        			strcpy(sharedmemory->listaDeEquipas[sharedmemory->equipas_counter].boxstate, "Livre");
        			sharedmemory->listaDeEquipas[sharedmemory->equipas_counter].go = 0;
        			sem_post(semaforoSHM);
        			
        			if (fork() == 0){
        				gestordeequipas(sharedmemory->equipas_counter);
        			}

        			sem_wait(semaforoSHM);
        			strcpy(sharedmemory->listaDeEquipas[sharedmemory->equipas_counter].listaDeCarros[0].teamname, string3[1]);
        			strcpy(sharedmemory->listaDeEquipas[sharedmemory->equipas_counter].listaDeCarros[0].state, "Corrida");
        			sharedmemory->listaDeEquipas[sharedmemory->equipas_counter].listaDeCarros[0].num = atoi(string3[2]);
        			sharedmemory->listaDeEquipas[sharedmemory->equipas_counter].listaDeCarros[0].deposito = (double) deposito;
        			sharedmemory->listaDeEquipas[sharedmemory->equipas_counter].listaDeCarros[0].speed = atoi(string3[3]);
        			sharedmemory->listaDeEquipas[sharedmemory->equipas_counter].listaDeCarros[0].consumption = atof(string3[4]);
        			sharedmemory->listaDeEquipas[sharedmemory->equipas_counter].listaDeCarros[0].reliability = atoi(string3[5]);
        			sem_post(semaforoSHM);

        			sprintf(string, "ADDCAR");
        			sleep(1);
        			write(fdunnamed[sharedmemory->equipas_counter][1], &string, sizeof(string));
        			sem_wait(semaforoSHM);
        			sharedmemory->equipas_counter++; 
        			sem_post(semaforoSHM);       			
				}
				
 			} 
 			else if (count == 9){
 				for (int k = 0; k < n_equipas; k++){
        			if (strcmp(string3[1],sharedmemory->listaDeEquipas[k].nome)==0){
        				int i;
        				for (i = 0; i < sharedmemory->listaDeEquipas[k].threads_created; i++){
        					if (sharedmemory->listaDeEquipas[k].listaDeCarros[i].num == atoi(string3[2])){
        						logoutput("Esse numero ja exite na equipa indicada");
        						break;
        					}
        				}
        				if (sharedmemory->listaDeEquipas[k].listaDeCarros[i].num == atoi(string3[2]))
        					break;
        				if (checkIfPossibleCar(k) == 0){
        					sprintf(string, "Esta equipa ja tem o maximo de carros permitido!\n");
        					logoutput(string);
        				}
        				else {
        					sprintf(string, "Novo carro adicionado! \n");
        					logoutput(string);
        					lastcar_added = carro;
        					
        					sem_wait(semaforoSHM);
        					posicao_carro = sharedmemory->listaDeEquipas[k].threads_created;
        					sem_post(semaforoSHM);
        					
        					sem_wait(semaforoSHM);
        					strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[posicao_carro].teamname, string3[1]);
        					sharedmemory->listaDeEquipas[k].listaDeCarros[posicao_carro].num = atoi(string3[2]);
        					strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[posicao_carro].state, "Corrida");
        					sharedmemory->listaDeEquipas[k].listaDeCarros[posicao_carro].speed = atoi(string3[3]);
        					sharedmemory->listaDeEquipas[k].listaDeCarros[posicao_carro].deposito = (double) deposito;
        					sharedmemory->listaDeEquipas[k].listaDeCarros[posicao_carro].consumption = atof(string3[4]);
        					sharedmemory->listaDeEquipas[k].listaDeCarros[posicao_carro].reliability = atoi(string3[5]);
        					sem_post(semaforoSHM);
   
      						sprintf(string, "ADDCAR");
      						write(fdunnamed[k][1], &string, sizeof(string));
        					break;
        				}	
        			}
        		}
 			} 
 			else {
 				sprintf(string, "Comando nao existente ou mal estruturado => %s\n", string4);
        		logoutput(string);
 			}			
        	
        } 
        else {
       		sprintf(string, "Comando nao existente ou mal estruturado => %s\n", string4);
       		logoutput(string);
       	}    
       	
       	count = 0;
       	o = 0;
       	fflush(stdout);    	
       	strcpy(string2, "\0");
        for (int i = 0; i < sizeof(string3)/sizeof(string3[0]); i++)
        	strcpy(string3[i], "\0");
     }
     
     sleep(1);
     
     #ifdef DEBUG
     printf("GESTOR DE CORRIDA EM MODO CORRIDA\n");
     #endif
     
     
     //Modo corrida e Multiplexing dos unnamed pipes
    while(sharedmemory->global == 1){
    
    	while (sharedmemory->restart == 1){
    		lenBuffer = read(fd, string2, sizeof(string2));
    		string2[lenBuffer] = '\0';
    		string2[strlen(string2)-1] = '\0';
    		strcpy(string4, string2);
        	char* ptr = strtok(string2, " ");
			
			while(ptr != NULL){
				strcpy(string3[o++], ptr);
				ptr = strtok(NULL, " ");
			}
			
			if (strcmp(string3[0], "START") == 0 && strcmp(string3[1], "RACE") == 0){
				sprintf(string, "RACE RESUMING");
        		logoutput(string);
        		sharedmemory->restart = 0;
        		sharedmemory->global = 1;
        		for (int i = 0; i < sharedmemory->equipas_counter; i++){
        			write(fdunnamed[i][1], &string, sizeof(string));
        		}
        		kill(sharedmemory->pidavarias, SIGUSR2);
        		//pthread_mutex_lock(&sharedmemory->mutex);
        		sleep(5);
        		pthread_cond_broadcast(&sharedmemory->start);
        		pthread_mutex_unlock(&sharedmemory->mutex);
        		
			}
    	}
    	
     	while (sharedmemory->carros_terminados != sharedmemory->threads_adicionadas){
     		FD_ZERO(&read_set);
     		for (int i = 0; i < sharedmemory->equipas_counter; i++){
     			FD_SET(fdunnamed[i][0], &read_set);
     		}
     		
     		if 	(select(fdunnamed[sharedmemory->equipas_counter - 1][0] + 1, &read_set, NULL, NULL, NULL) > 0){
     			for (int i = 0; i < sharedmemory->equipas_counter; i++){
     				if (FD_ISSET(fdunnamed[i][0], &read_set)){
     					lenBuffer = read(fdunnamed[i][0], &message, sizeof(message));
     					o = 0;
     					
    					message[lenBuffer] = '\0';
    					strcpy(string4, message);
        				char* ptr = strtok(message, " ");
			
						while(ptr != NULL){
							strcpy(string3[o++], ptr);
							ptr = strtok(NULL, " ");
						}
						
						if ((strcmp(string3[2],"ENDED")==0)){
							sem_wait(semaforoSHM);
							sharedmemory->carros_terminados++;
							#ifdef DEBUG
							logoutput("CAR ENDED\n");
							#endif
							sem_post(semaforoSHM);
						}
						
        				logoutput(string4);
     				}
     			}
     		}
		}
		sem_wait(semaforoSHM);
		sharedmemory->global = 0;
		sem_post(semaforoSHM);	
		for (int i = 0; i < sharedmemory->equipas_counter; i++){
			sprintf(string, "-2");
			write(fdbox[i][1], &string, sizeof(string));
			
		}
	}	
	
	for (int j = 0; j < sharedmemory->equipas_counter; j++){
		for (int h = 0; h < sharedmemory->listaDeEquipas[j].threads_created; h++){
			#ifdef DEBUG
			printf("NUM: %d TIME: %f CONSUMPTION: %f RELIABILITY: %d TIME: %f J: %d H: %d\n", sharedmemory->listaDeEquipas[j].listaDeCarros[h].num, sharedmemory->listaDeEquipas[j].listaDeCarros[h].tempo, sharedmemory->listaDeEquipas[j].listaDeCarros[h].consumption, sharedmemory->listaDeEquipas[j].listaDeCarros[h].reliability, sharedmemory->listaDeEquipas[j].listaDeCarros[h].tempo, j, h);
			#endif
		}
	}
	
    exit(0);
}

int checkIfPossibleEquipa(){
	if (sharedmemory->equipas_counter == n_equipas){
		return 0;
	}	
	return 1;	
}

int checkIfPossibleCar(int i){
	if (sharedmemory->listaDeEquipas[i].threads_created == max_carros){
		return 0;
	}
	return 1;
}

void gestordeavarias(){
	messagequeue message;
	struct sigaction act;
	
	act.sa_handler =  startrace;
	act.sa_flags = SA_RESTART;
	sigemptyset(&act.sa_mask);
	sigaction(SIGUSR2, &act, NULL);
	
	sem_wait(semaforoSHM);
	sharedmemory->pidavarias = getpid();
	sem_post(semaforoSHM);
	
	while(sharedmemory->global == 0){
		pause();
	}
	#ifdef DEBUG
	printf("COMEÇOU A CORRIDA NO GESTOR DE AVARIAS\n");
	#endif
	while (sharedmemory->global == 1){
		sleep(t_avaria);
		for (int i = 0; i < sharedmemory->equipas_counter; i++){
			for (int j = 0; j < max_carros; j++){
				carro carro = sharedmemory->listaDeEquipas[i].listaDeCarros[j];
				int r = rand() % 100;
				if (strlen(carro.state) != 0 && strcmp(carro.state, "Terminado") != 0 && strcmp(carro.state, "Desistencia") != 0){
					message.messagetype = i*max_carros + j + 1;
					if (r > sharedmemory->listaDeEquipas[i].listaDeCarros[j].reliability){
						printf("GEREI AVARIA\n");
						message.num = carro.num;
						msgsnd(sharedmemory->mqid, &message, sizeof(message)-sizeof(long), 0);
					} else {
						message.num = -1;
						msgsnd(sharedmemory->mqid, &message, sizeof(message)-sizeof(long), 0);
					}
				}	
			}	
		}
	}
	terminar();
}

void imprimirestatisticas(){
	int top_i[5];
	int top_j[5];
	int total_carros = 0, total_carros_pista = 0, temp_i, temp_j;

	if (getpid() == main_pid){
		if (sharedmemory->global == 1){
			int count = 0;
			for (int i = 0; i < sharedmemory->equipas_counter; i++){
				for (int j = 0; j < sharedmemory->listaDeEquipas[i].threads_created; j++){
					top_i[count] = i;
					top_j[count] = j;
					count++;
					if (count == 6)
						break;
				}
				if (count == 6)
					break;
			}
		
			for (int i = 0; i < 5; i++)
				for (int j = i + 1; j < 5; j++){
						printf("Ainda estou vivo\n");
					if (sharedmemory->listaDeEquipas[top_i[i]].listaDeCarros[top_j[i]].distancia_percorrida > sharedmemory->listaDeEquipas[top_i[j]].listaDeCarros[top_j[j]].distancia_percorrida){
						printf("Nao morri\n");
						temp_i = top_i[i];
						temp_j = top_j[i];
						top_i[i] = top_i[j];
						top_j[i] = top_j[j];
						top_i[j] = temp_i;
						top_j[j] = temp_j;
					}
				}
		
			for (int i = 0; i < sharedmemory->equipas_counter; i++){
				for (int j = 0; j < sharedmemory->listaDeEquipas[i].threads_created; j++){
					total_carros++;
					if (strcmp(sharedmemory->listaDeEquipas[i].listaDeCarros[j].state, "Desistencia") != 0 && strcmp(sharedmemory->listaDeEquipas[i].listaDeCarros[j].state, "Terminado") != 0)
						total_carros_pista++;
						
					if (sharedmemory->listaDeEquipas[i].listaDeCarros[j].distancia_percorrida >= sharedmemory->listaDeEquipas[top_i[0]].listaDeCarros[top_j[0]].distancia_percorrida && (i != top_i[0] || j != top_j[0])){
						top_i[4] = top_i[3];
						top_j[4] = top_j[3];
						
						top_i[3] = top_i[2];
						top_j[3] = top_j[2];
						
						top_i[2] = top_i[1];
						top_j[2] = top_j[1];
						
						top_i[1] = top_i[0];
						top_j[1] = top_j[0];
						
						top_i[0] = i;
						top_j[0] = j;
					}
				}
			}
		}
		else{
			
		}
	
		printf("MOSTRANDO ESTATISTICAS!\n");
		if (total_carros >= 5)
			printf("TOP 5 DA CORRIDA:\n");
		else
			printf("TOP %d DA CORRIDA:\n", total_carros);
		printf("1 LUGAR: Carro n%d da equipa %s - %d voltas realizadas - %d paragens na box\n", sharedmemory->listaDeEquipas[top_i[0]].listaDeCarros[top_j[0]].num, sharedmemory->listaDeEquipas[top_i[0]].nome, sharedmemory->listaDeEquipas[top_i[0]].listaDeCarros[top_j[0]].voltas_completas, sharedmemory->listaDeEquipas[top_i[0]].listaDeCarros[top_j[0]].num_pitstops);
		if (total_carros >= 2)
			printf("2 LUGAR: Carro n%d da equipa %s - %d voltas realizadas - %d paragens na box\n", sharedmemory->listaDeEquipas[top_i[1]].listaDeCarros[top_j[1]].num, sharedmemory->listaDeEquipas[top_i[1]].nome, sharedmemory->listaDeEquipas[top_i[1]].listaDeCarros[top_j[1]].voltas_completas, sharedmemory->listaDeEquipas[top_i[1]].listaDeCarros[top_j[1]].num_pitstops);
		if (total_carros >= 3)
			printf("3 LUGAR: Carro n%d da equipa %s - %d voltas realizadas - %d paragens na box\n", sharedmemory->listaDeEquipas[top_i[2]].listaDeCarros[top_j[2]].num, sharedmemory->listaDeEquipas[top_i[2]].nome, sharedmemory->listaDeEquipas[top_i[2]].listaDeCarros[top_j[2]].voltas_completas, sharedmemory->listaDeEquipas[top_i[2]].listaDeCarros[top_j[2]].num_pitstops);
		if (total_carros >= 4)
			printf("4 LUGAR: Carro n%d da equipa %s - %d voltas realizadas - %d paragens na box\n", sharedmemory->listaDeEquipas[top_i[3]].listaDeCarros[top_j[3]].num, sharedmemory->listaDeEquipas[top_i[3]].nome, sharedmemory->listaDeEquipas[top_i[3]].listaDeCarros[top_j[3]].voltas_completas, sharedmemory->listaDeEquipas[top_i[3]].listaDeCarros[top_j[3]].num_pitstops);
		if (total_carros >= 5)
			printf("5 LUGAR: Carro n%d da equipa %s - %d voltas realizadas - %d paragens na box\n\n", sharedmemory->listaDeEquipas[top_i[4]].listaDeCarros[top_j[4]].num, sharedmemory->listaDeEquipas[top_i[4]].nome, sharedmemory->listaDeEquipas[top_i[4]].listaDeCarros[top_j[4]].voltas_completas, sharedmemory->listaDeEquipas[top_i[4]].listaDeCarros[top_j[4]].num_pitstops);
		printf("NUMERO DE AVARIAS: %d\n", sharedmemory->total_avarias);
		printf("NUMERO DE ABASTECIMENTOS: %d\n", sharedmemory->total_abastecimentos);
		printf("NUMERO DE CARROS EM PISTA: %d\n", total_carros_pista);
	}
}

void gestordeequipas(int equipas_counter2){
    char message[1024];
	int j;
	pass_argument pass_argument;
	struct sigaction act;
	
	act.sa_handler =  startrace;
	act.sa_flags = SA_RESTART;
	sigemptyset(&act.sa_mask);
	sigaction(SIGUSR2, &act, NULL);
	
	sem_wait(semaforoSHM);
	sharedmemory->listaDeEquipas[equipas_counter2].pid = getpid();
	sem_post(semaforoSHM);
    
    while(sharedmemory->global == 0){
   	 	read(fdunnamed[equipas_counter2][0], &message, sizeof(message));
   	 	if (strcmp(message, "ADDCAR") == 0){
   	 		sleep(1);
   	 		create_thread();
   	 	} else{
   	 		break;
   	 	}
 	}
 	
 	#ifdef DEBUG
 	printf("COMEÇOU A CORRIDA NO GESTOR DE EQUIPAS Nº %d\n", equipas_counter2);
 	#endif
    while(sharedmemory->restart == 1 || sharedmemory->global == 1){
    
    	while (sharedmemory->restart == 1){
    		read(fdunnamed[equipas_counter2][0], &message, sizeof(message));
    		if (strcmp(message, "RACE RESUMING") == 0){
    			for (int i = 0; i < sharedmemory->listaDeEquipas[equipas_counter2].threads_created; i++){
    				pass_argument.id = i;
    				strcpy(pass_argument.teamname, sharedmemory->listaDeEquipas[equipas_counter2].nome);
   					pass_argument.threadid = sharedmemory->newcar;
   					
   					if (pthread_create(&thread_id[sharedmemory->newcar], NULL, carro_simulacao, &pass_argument) != 0){
						char string[STRING_SIZE];
						sprintf(string, "Erro a criar thread - Carro %d - PID %d", sharedmemory->listaDeEquipas[j].threads_created, getpid());
						logoutput(string);
					}
					else{
						printf("Thread criada\n");
    				}
    				sem_wait(semaforoSHM);
    				sharedmemory->newcar++;
    				sem_post(semaforoSHM);
    			}	
    		}
    	}
		while (sharedmemory->global == 1){
			read(fdbox[equipas_counter2][0], &message, sizeof(message));
			#ifdef DEBUG
			printf("MENSAGEM NOVA NO GESTOR DE EQUIPAS: %s\n", message);
			#endif
			j = atoi(message);
			if (sharedmemory->listaDeEquipas[equipas_counter2].listaDeCarros[j].avaria == 1){
				#ifdef DEBUG
				printf("ESTOU A ARRANJAR UM CARRO equipas_counter2: %d J: %d\n", equipas_counter2, j);
				#endif
				int num = (rand() % (t_box_max - t_box_min + 1)) + t_box_min;
				sem_wait(semaforoSHM);
				sharedmemory->total_avarias++;
				sharedmemory->listaDeEquipas[equipas_counter2].listaDeCarros[j].tempo += num;
				sem_post(semaforoSHM);
				sleep(num);
			} else if (j == -2){
				sleep(1);
			} else {
				#ifdef DEBUG
				printf("ESTOU A ABASTECER UM CARRO equipas_counter2: %d J: %d\n", equipas_counter2, j);
				#endif
				sem_wait(semaforoSHM);
				sharedmemory->total_abastecimentos++;
				sharedmemory->listaDeEquipas[equipas_counter2].listaDeCarros[j].deposito = deposito;
				sharedmemory->listaDeEquipas[equipas_counter2].listaDeCarros[j].tempo += 2 / unidades;
				sem_post(semaforoSHM);
				sleep(2 / unidades);
			}
			sem_wait(semaforoSHM);
			strcpy(sharedmemory->listaDeEquipas[equipas_counter2].boxstate, "Livre");
			sharedmemory->listaDeEquipas[equipas_counter2].go = 0;
			sem_post(semaforoSHM);
			pthread_cond_signal(&sharedmemory->box);
        	pthread_mutex_unlock(&sharedmemory->mutex);
			
		}
		
	}
	terminar();	
}

void startrace(){
	sharedmemory->global = 1;
}

void create_thread(){
	pass_argument pass_argument;
	int j;
	
	for (j = 0; j < sharedmemory->equipas_counter; j++)
		if (getpid() == sharedmemory->listaDeEquipas[j].pid){
			pass_argument.id = sharedmemory->listaDeEquipas[j].threads_created;	
    		strcpy(pass_argument.teamname, sharedmemory->listaDeEquipas[j].nome);
   			pass_argument.threadid = sharedmemory->threads_adicionadas;
   			break;
		}
	
   	if (pthread_create(&thread_id[sharedmemory->threads_adicionadas], NULL, carro_simulacao, &pass_argument) != 0){
		char string[STRING_SIZE];
		sprintf(string, "Erro a criar thread - Carro %d - PID %d", sharedmemory->listaDeEquipas[j].threads_created, getpid());
		logoutput(string);
	}
	else{
		#ifdef DEBUG
		printf("Thread criada\n");
		#endif
	}
	
	sem_wait(semaforoSHM);
	sharedmemory->listaDeEquipas[j].threads_created++;
	sharedmemory->threads_adicionadas++;
	sem_post(semaforoSHM);
}

void *carro_simulacao(pass_argument *pass_argument){
	char string[STRING_SIZE];
	int k, volta_final, j = pass_argument->id;
	messagequeue message;
	
	for (int h = 0; h < sharedmemory->equipas_counter; h++){
		if (strcmp(sharedmemory->listaDeEquipas[h].nome, pass_argument->teamname) == 0){
			k = h;
			break;
		}
	}

	double gasto_volta = ((double) dist / (double) (sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * unidades)) * (sharedmemory->listaDeEquipas[k].listaDeCarros[j].consumption * unidades);
	#ifdef DEBUG
	printf("NUM: %d SPEED: %d CONSUMPTION: %f RELIABILITY: %d  GASTO VOLTA: %f ESTADO: %s K: %d J: %d\n", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed, sharedmemory->listaDeEquipas[k].listaDeCarros[j].consumption, sharedmemory->listaDeEquipas[k].listaDeCarros[j].reliability, gasto_volta, sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, k, j);
	#endif
	
	pthread_mutex_lock(&sharedmemory->mutex);
	while(sharedmemory->global == 0){
		pthread_cond_wait(&sharedmemory->start, &sharedmemory->mutex);
	}
	pthread_mutex_unlock(&sharedmemory->mutex);
	
	sleep(5);
	
	#ifdef DEBUG
	logoutput("CAR IS STARTING\n");
	#endif
	while (sharedmemory->global == 1){
		if (strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Corrida")==0){
			message.num = -1;
		
			if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * unidades >= dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta){
				if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas + 1 == n_voltas){
					sem_wait(semaforoSHM);
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].tempo += (dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta) / (double) (sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * unidades);
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas++;
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito -= (dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta) / (double) (sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * unidades) * sharedmemory->listaDeEquipas[k].listaDeCarros[j].consumption;
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_percorrida += dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta;
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta = 0;
					strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Terminado");
					sem_post(semaforoSHM);
					sprintf(string, "O carro n%d da equipa %s terminou", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].nome);
					write(fdunnamed[k][1], &string, sizeof(string));
					return NULL;
				} else if ((strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].modo, "box")==0) && (strcmp(sharedmemory->listaDeEquipas[k].boxstate, "Livre")==0)){
					sem_wait(semaforoSHM);
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].tempo += (dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta) / (double) (sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * unidades);
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas++;
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito -= (dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta) / (double) (sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * unidades) * sharedmemory->listaDeEquipas[k].listaDeCarros[j].consumption;
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_percorrida += dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta;
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta = 0;
					strcpy(sharedmemory->listaDeEquipas[k].boxstate, "Ocupada");
					strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Box");
					sem_post(semaforoSHM);
				}
			}
			if ((strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Terminado")!=0) && (strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Box")!=0)){
			
				sem_wait(semaforoSHM);
				
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_percorrida += sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * unidades;
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito -= sharedmemory->listaDeEquipas[k].listaDeCarros[j].consumption * unidades;
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta += sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * unidades;
				
				sem_post(semaforoSHM);
				
				if ((sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito < gasto_volta * 4) && (strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].modo, "box") != 0) && (strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].modo, "entra box") != 0)){
					sem_wait(semaforoSHM);
					strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].modo, "box");
					sem_post(semaforoSHM);
				}
				
				if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito < gasto_volta * 2){
					sem_wait(semaforoSHM);
					strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Seguranca");
					sem_post(semaforoSHM);
					sprintf(string, "O carro n%d da equipa %s entrou em modo de seguranca", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].nome);
					write(fdunnamed[k][1], &string, sizeof(string));
				}
				
				if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta >= dist){
					sem_wait(semaforoSHM);
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas++;
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta = sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta - dist;
					sem_post(semaforoSHM);
				}
				
				if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas == n_voltas){
					sem_wait(semaforoSHM);
					strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Terminado");
					if (strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].modo, "entra box") == 0){
						strcpy(sharedmemory->listaDeEquipas[k].boxstate, "Livre");
					}
					sem_post(semaforoSHM);
					sprintf(string, "O carro n%d da equipa %s terminou", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].nome);
					write(fdunnamed[k][1], &string, sizeof(string));
					return NULL;
				}
				
				if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito < sharedmemory->listaDeEquipas[k].listaDeCarros[j].consumption * unidades){
					sem_wait(semaforoSHM);
					strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Desistencia");
					if (strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].modo, "entra box") == 0){
						strcpy(sharedmemory->listaDeEquipas[k].boxstate, "Livre");
					}
					sem_post(semaforoSHM);
					sprintf(string, "O carro n%d da equipa %s desistiu por falta de combustivel", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].nome);
					write(fdunnamed[k][1], &string, sizeof(string));
					return NULL;
					
				}
				
				sem_wait(semaforoSHM);
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].tempo += 1;
				sem_post(semaforoSHM);
			}	
			
		} else if (strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Seguranca")==0){
		
			if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * 0.3 * unidades >= dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta){
				if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas + 1 == n_voltas){
					sem_wait(semaforoSHM);
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].tempo += (dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta) / (double) (sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * 0.3 * unidades);
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas++;
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito -= (dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta) / (double) (sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * unidades) * sharedmemory->listaDeEquipas[k].listaDeCarros[j].consumption;
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_percorrida += dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta;
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta = 0;
					strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Terminado");
					sem_post(semaforoSHM);
					sprintf(string, "O carro n%d da equipa %s terminou", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].nome);
					write(fdunnamed[k][1], &string, sizeof(string));
					return NULL;
				} else if ((strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].modo, "entra box")==0)){
					sem_wait(semaforoSHM);
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].tempo += (dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta) / (double) (sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * 0.3 * unidades);
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas++;
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito -= (dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta) / (double) (sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * unidades) * sharedmemory->listaDeEquipas[k].listaDeCarros[j].consumption;
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_percorrida += dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta;
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta = 0;
					strcpy(sharedmemory->listaDeEquipas[k].boxstate, "Ocupada");
					strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Box");
					sem_post(semaforoSHM);
				}
			}
			
			if ((strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Terminado")!=0) && (strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Box")!=0)){
			
				sem_wait(semaforoSHM);
				
				if (strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].modo, "entra box") != 0){
					strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].modo, "box");
				}
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_percorrida += sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * 0.3 * unidades;
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito -= sharedmemory->listaDeEquipas[k].listaDeCarros[j].consumption * 0.4 * unidades;
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta += sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * 0.3 * unidades;
				
				sem_post(semaforoSHM);
				
				if (strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].modo, "box") == 0){
					if (strcmp(sharedmemory->listaDeEquipas[k].boxstate, "Livre") == 0){
						sem_wait(semaforoSHM);
						strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].modo, "entra box");
						strcpy(sharedmemory->listaDeEquipas[k].boxstate, "Reservado");
						sem_post(semaforoSHM);
					}
						
				}
				
				if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta >= dist){
					sem_wait(semaforoSHM);
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas++;
					sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta = sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta - dist;
					sem_post(semaforoSHM);
				}
				
				if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas == n_voltas){
					sem_wait(semaforoSHM);
					strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Terminado");
					if (strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].modo, "entra box") == 0){
						strcpy(sharedmemory->listaDeEquipas[k].boxstate, "Livre");
					}
					sem_post(semaforoSHM);
					sprintf(string, "O carro n%d da equipa %s terminou", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].nome);
					write(fdunnamed[k][1], &string, sizeof(string));
					return NULL;
				}
				
				if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito < sharedmemory->listaDeEquipas[k].listaDeCarros[j].consumption * 0.4 * unidades){
					sem_wait(semaforoSHM);
					strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Desistencia");
					if (strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].modo, "entra box") == 0){
						strcpy(sharedmemory->listaDeEquipas[k].boxstate, "Livre");
					}
					sem_post(semaforoSHM);
					sprintf(string, "O carro n%d da equipa %s terminou", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].nome);
					write(fdunnamed[k][1], &string, sizeof(string));
					return NULL;
				}
				
				sem_wait(semaforoSHM);
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].tempo += 1;
				sem_post(semaforoSHM);
			}
			
		} else if (strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Box")==0){
			sprintf(string, "O carro n%d da equipa %s vai entrar na box", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].nome);
			write(fdunnamed[k][1], &string, sizeof(string));
			sem_wait(semaforoSHM);
			sharedmemory->listaDeEquipas[k].listaDeCarros[j].num_pitstops++;
			sem_post(semaforoSHM);
			sprintf(string, "%d", j);
			sharedmemory->listaDeEquipas[k].go = 1;
			write(fdbox[k][1], &string, sizeof(string));
			pthread_mutex_lock(&sharedmemory->mutex);
			while(sharedmemory->listaDeEquipas[k].go == 1){
				pthread_cond_wait(&sharedmemory->box, &sharedmemory->mutex);
			}
			pthread_mutex_unlock(&sharedmemory->mutex);
			if (sharedmemory->global == 0){
				sprintf(string, "O carro n%d da equipa %s desistiu", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].nome);
				write(fdunnamed[k][1], &string, sizeof(string));
				return NULL;
			}
			if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].avaria == 1){
				sem_wait(semaforoSHM);
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].avaria = 0;
				sem_post(semaforoSHM);
			}
			sem_wait(semaforoSHM);
			sprintf(string, "O carro n%d da equipa %s vai sair da box", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].nome);
			write(fdunnamed[k][1], &string, sizeof(string));
			strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Corrida");
			strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].modo, "run");
			sem_post(semaforoSHM);	
		}
		
		msgrcv(sharedmemory->mqid, &message, sizeof(message)-sizeof(long), k * max_carros + j + 1, IPC_NOWAIT);
		if (message.num == sharedmemory->listaDeEquipas[k].listaDeCarros[j].num && sharedmemory->listaDeEquipas[k].listaDeCarros[j].avaria != 1 && strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Terminado") != 0 && strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Desistencia") != 0){
			sprintf(string, "O carro n%d da equipa %s esta em modo de seguranca devido a um mau funcionamento", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].nome);
			write(fdunnamed[k][1], &string, sizeof(string));
			sem_wait(semaforoSHM);
			sharedmemory->listaDeEquipas[k].listaDeCarros[j].avaria = 1;
			if (strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Corrida") == 0){
				strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Seguranca");
				
			} 
			sem_post(semaforoSHM);
		}	
		#ifdef DEBUG
		printf("NUM: %d VOLTAS_COMPLETAS: %d DIST PERCORRIDA: %d DEPOSITO: %f  TEMPO: %f DISTANCIA VOLTA: %d STATE: %s COMPARE: %d K: %d J: %d\n", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas, sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_percorrida, sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito, sharedmemory->listaDeEquipas[k].listaDeCarros[j].tempo, sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta, sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, strcmp(sharedmemory->listaDeEquipas->listaDeCarros[j].state, "Seguranca"), k, j);
		#endif
		sleep(1);
	}
	
	printf("Os carros estao todos a terminar as suas voltas\n");
	volta_final = sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas + 1;
	while(sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas != volta_final){
		if (strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Corrida")==0){
			
			if ((sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * unidades >= dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta)){
				sem_wait(semaforoSHM);
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].tempo += (dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta) / (sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * unidades);
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas++;
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito -= (dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta) / (double) (sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * unidades) * sharedmemory->listaDeEquipas[k].listaDeCarros[j].consumption;
				sem_post(semaforoSHM);
				sprintf(string, "O carro n%d da equipa %s terminou", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].nome);
				write(fdunnamed[k][1], &string, sizeof(string));
				sem_wait(semaforoSHM);
                sharedmemory->carros_terminados++;
                sem_post(semaforoSHM);
				return NULL;
			}
				
			sem_wait(semaforoSHM);
				
			sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_percorrida += sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * unidades;
			sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito -= sharedmemory->listaDeEquipas[k].listaDeCarros[j].consumption * unidades;
			sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta += sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * unidades;
			
			sem_post(semaforoSHM);
			
			if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta >= dist){
				sem_wait(semaforoSHM);
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas++;
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta = sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta - dist;
				sem_post(semaforoSHM);
			}
			
			if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito < gasto_volta * 2){
				sem_wait(semaforoSHM);
				strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Seguranca");
				sem_post(semaforoSHM);
				sprintf(string, "O carro n%d da equipa %s entrou em modo de seguranca devido a ter pouco combustivel", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].nome);
				write(fdunnamed[k][1], &string, sizeof(string));
			}
			
			if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas == n_voltas){
				sem_wait(semaforoSHM);
				strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Terminado");
				sem_post(semaforoSHM);
				sprintf(string, "O carro n%d da equipa %s terminou", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].nome);
				write(fdunnamed[k][1], &string, sizeof(string));
				sem_wait(semaforoSHM);
                sharedmemory->carros_terminados++;
                sem_post(semaforoSHM);
				return NULL;
			}
			
			if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito < sharedmemory->listaDeEquipas[k].listaDeCarros[j].consumption * unidades){
				sem_wait(semaforoSHM);
				strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Desistencia");
				sem_post(semaforoSHM);
				sprintf(string, "O carro n%d da equipa %s desistiu devido a falta de combustivel", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].nome);
				write(fdunnamed[k][1], &string, sizeof(string));
				sem_wait(semaforoSHM);
                sharedmemory->carros_terminados++;
                sem_post(semaforoSHM);
				return NULL;
			}
			
		} else if (strcmp(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Seguranca")==0){
		
			if ((sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * 0.3 * unidades >= dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta)){
				sem_wait(semaforoSHM);
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].tempo += (dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta) / (sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * 0.3 * unidades);
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas++;
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito -= (dist - sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta) / (double) (sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * unidades) * sharedmemory->listaDeEquipas[k].listaDeCarros[j].consumption;
				sem_post(semaforoSHM);
				sprintf(string, "O carro n%d da equipa %s terminou", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].nome);
				write(fdunnamed[k][1], &string, sizeof(string));
				sem_wait(semaforoSHM);
                sharedmemory->carros_terminados++;
                sem_post(semaforoSHM);
				return NULL;
			}
		
			sem_wait(semaforoSHM);
	
			strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].modo, "box");
			sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_percorrida += sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * 0.3 * unidades;
			sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito -= sharedmemory->listaDeEquipas[k].listaDeCarros[j].consumption * 0.4 * unidades;
			sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta += sharedmemory->listaDeEquipas[k].listaDeCarros[j].speed * 0.3 * unidades;
			
			sem_post(semaforoSHM);
			
			if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta >= dist){
				sem_wait(semaforoSHM);
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas++;
				sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta = sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta - dist;
				sem_post(semaforoSHM);
			}
		
			if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas == n_voltas){
				sem_wait(semaforoSHM);
				strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Terminado");
				sem_post(semaforoSHM);
				sprintf(string, "O carro n%d da equipa %s terminou", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].nome);
				write(fdunnamed[k][1], &string, sizeof(string));
				sem_wait(semaforoSHM);
                sharedmemory->carros_terminados++;
                sem_post(semaforoSHM);
				return NULL;
			}
			
			if (sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito < sharedmemory->listaDeEquipas[k].listaDeCarros[j].consumption * 0.4 * unidades){
				sem_wait(semaforoSHM);
				strcpy(sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, "Desistencia");
				sem_post(semaforoSHM);
				sprintf(string, "O carro n%d da equipa %s desistiu devido a falt de combustivel", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].nome);
				write(fdunnamed[k][1], &string, sizeof(string));
				return NULL;
			}
			
		}
		#ifdef DEBUG
		printf("NUM: %d VOLTAS_COMPLETAS: %d DIST PERCORRIDA: %d DEPOSITO: %f  TEMPO: %f DISTANCIA VOLTA: %d STATE: %s COMPARE: %d K: %d J: %d\n", sharedmemory->listaDeEquipas[k].listaDeCarros[j].num, sharedmemory->listaDeEquipas[k].listaDeCarros[j].voltas_completas, sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_percorrida, sharedmemory->listaDeEquipas[k].listaDeCarros[j].deposito, sharedmemory->listaDeEquipas[k].listaDeCarros[j].tempo, sharedmemory->listaDeEquipas[k].listaDeCarros[j].distancia_volta, sharedmemory->listaDeEquipas[k].listaDeCarros[j].state, strcmp(sharedmemory->listaDeEquipas->listaDeCarros[j].state, "Seguranca"), k, j);
		#endif
		sleep(1);
		sem_wait(semaforoSHM);
		sharedmemory->listaDeEquipas[k].listaDeCarros[j].tempo += 1;
		sem_post(semaforoSHM);
	}	
	return NULL;
	
}

int verify_atoi(char *num){
	for (int i = 0; i < strlen(num); i++){
		if (!isdigit(num[i])){
			return 0;
		}
	}
	return 1;
}

void fimdacorrida(){
    if (getpid() == main_pid){
        char string[1024];
        sprintf(string, " A terminar a corrida\n");
        logoutput(string);
        if (sharedmemory->global == 1){
            sem_wait(semaforoSHM);
            sharedmemory->global = 0;
            sem_post(semaforoSHM);

            pthread_mutex_lock(&sharedmemory->mutex);
            pthread_cond_broadcast(&sharedmemory->box);
            pthread_mutex_unlock(&sharedmemory->mutex);

            while (sharedmemory->carros_terminados != sharedmemory->threads_adicionadas){
                sleep(1);
            }
            imprimirestatisticas();
        }
        terminar();
    } else {
        if (sharedmemory->global == 0){
            exit(0);
        } else {
            while (sharedmemory->carros_terminados != sharedmemory->threads_adicionadas){
                sleep(1);
            }
            terminar();
        }
    }
}

void terminar(){
	if (getpid() == main_pid){
		for (int i = 0; i < sharedmemory->threads_adicionadas; i++){
			pthread_cancel(thread_id[i]);
    		pthread_join(thread_id[i], NULL);
		}
		
		close(fd);
		sem_close(semaforoLogs);
		sem_close(semaforoSHM);
		sem_close(semaforoMSQ);
		sem_unlink("SemaforoLogs");
		sem_unlink("SemaforoSM");
		sem_unlink("SemaforoMSQ");
		
		msgctl(sharedmemory->mqid, IPC_RMID, 0);
  	
		
		if (shmctl(shmid, IPC_RMID, NULL) == -1)
			logoutput("Erro ao encerrar a memoria partilhada");
		
		printf(" A terminar sessão e limpando os recursos.\n");
	}	
	
	exit(0);
}
