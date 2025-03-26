#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#include <sys/fcntl.h>
#include <errno.h>
#include <time.h>
#include <sched.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>          
#include <sys/mman.h>

#include "utils.h"

#define NB_TRAINS 4

#define NB_MUTEX (1<<8)
#define NB_PROC_MAX_FA 50
#define MAXOCTETS 150

#define IP_SIZE 16
#define DEFAULT_LOCALIP "127.0.0.1"

char ip[IP_SIZE];
int ports[NB_TRAINS] = {3000,8000,8080,5000};

void train(int no);
void bye();

 // Memoire partagée
struct file_attente {
    pid_t file[NB_PROC_MAX_FA]; // 50 personnes MAX
    int longueurs; // Longueurs actuelles de la file d'attente
    int current_check; //La valeur actuellement check sur la file d'attente
    unsigned char resources; // 7 bit pour représenter les ressources
};
struct file_attente* f_a;
size_t size = sizeof(f_a);
int shm_fd;

int se[NB_TRAINS];
int erreur;
struct sockaddr_in adrserveur;
struct sockaddr_in adrclient;
socklen_t adrclient_len = sizeof(adrclient);

sem_t* lock;

int main(int argc, char *argv[]) {
    
    if(argc == 2){
        strncpy(ip, argv[1], IP_SIZE - 1);
        ip[IP_SIZE - 1] = '\0';
    }
    else{
        strncpy(ip, DEFAULT_LOCALIP, IP_SIZE - 1);
        ip[IP_SIZE - 1] = '\0';
    }
    
    printf("Adresse IP locale : %s\n",ip);

    pid_t pid[NB_TRAINS];

    // Gestion de mémoire partagée (file_attente)
    // Création ou ouverture de la mémoire partagée
    CHECK(shm_fd = shm_open("file_attente", O_CREAT | O_RDWR, 0666),"shm_open(file_attente)");
    // Définir la taille de la mémoire partagée pour contenir la structure SharedData
    size = sysconf(_SC_PAGE_SIZE); 
    CHECK(ftruncate(shm_fd, size),"ftruncate(shm_fd)");
    // Mappage de la mémoire partagée
    CHECK_MAP(f_a = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0),"mmap");

    // Initialisation de la file d'attente
    f_a->longueurs = 0;
    f_a->current_check = 0;
    f_a->resources = 0b0000000; // 7 bits pour représenter les ressources
    for (int j = 0; j < NB_PROC_MAX_FA; j++) {
            f_a->file[j] = 0;
    }

    CHECK_S(lock = sem_open("lock",O_CREAT|O_EXCL,0666,1),"sem_open(lock)");

    // Permet de faire le cleanning des sémaphores lors des exits
    atexit(bye);// bye detruit les semaphores

    // Avec ça le père sera "imunisé" au Ctrl-C mais pas ses fils (on va réactiver le SIGINT pour eux) ! 
    // Du coup ils vont tous se terminer, sauf le père qui va pouvoir les récupérer et terminer correctement est donc faire le nettoyage des sémaphores
    // Masque SIGINT pour le père
    sigset_t Mask,OldMask;
    CHECK(sigemptyset(&Mask), "sigemptyset()");
    CHECK(sigaddset(&Mask , SIGINT), "sigaddset(SIGINT)");
    CHECK(sigprocmask(SIG_SETMASK , &Mask , &OldMask), "sigprocmask()");

    // Création des fils du processus père (un par train)
    for (int i=0;i<NB_TRAINS;i++){
        CHECK(pid[i]=fork(),"fork(pid)");
        if (pid[i]==0){
            // Démasque SIGINT
            CHECK(sigprocmask(SIG_SETMASK , &OldMask , NULL), "sigprocmask()");

            // Création de la socket d'écoute
            CHECK(se[i]=socket(AF_INET, SOCK_STREAM, 0),"Erreur socket d'écoute non cree !!! \n");
            // Permet de réutiliser l'adresse
            int opt = 1;
            CHECK_0(setsockopt(se[i], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)),"setsockopt\n");

            train(i);
        }
    }

    // Processus Père
    // Attente de la terminaison des fils
    for (int i=0;i<NB_TRAINS;i++){
        int status;
        CHECK(wait(&status), "wait()");
        close(se[i]);
    }
    return 0;
}

void bye(){

    // Suppression de la mémoire partagée
    CHECK(munmap(f_a, size),"munmap(file_attente)");
    CHECK(close(shm_fd),"close(shm_fd)");
    CHECK(shm_unlink("file_attente"),"shm_unlink(file_attente)");

    CHECK(sem_close(lock),"sem_close(lock)");
    CHECK(sem_unlink("lock"),"sem_unlink(lock)");

}


void train(int no){

    //preparation de l'adresse de la socket
    adrserveur.sin_family = AF_INET;
    adrserveur.sin_port = htons(ports[no]);
    adrserveur.sin_addr.s_addr = inet_addr(ip);
    //Affectation d'une adresse a la socket
    CHECK(erreur = bind(se[no], (const struct sockaddr *)&adrserveur, sizeof(adrserveur)),"Erreur de bind !!!\n");

    char buff_reception[MAXOCTETS + 1];
    char buff_emission[MAXOCTETS + 1];
    char num_mutex[MAXOCTETS + 1];
    int nbcar;
    int client_sd;
    listen(se[no], 1);
    CHECK(client_sd = accept(se[no], (struct sockaddr *)&adrclient, &adrclient_len),"Erreur de accept !!!\n");
    while (1){
        CHECK(nbcar = recv(client_sd, buff_reception, MAXOCTETS, 0),"Problème de réception !!!\n");
        buff_reception[nbcar] = '\0';
        printf("MSG RECU DU CLIENT %d : %s\n",client_sd, buff_reception);
        // Faire ici des cas en fonction de la demande du client (demande ou restitution de Mutex)
        if(buff_reception[0]=='0'){
            // Demande de Mutex
            strcpy(num_mutex, buff_reception + 1); // Copie à partir du deuxième caractère
            printf("Demande de prise de la mutex %d\n",atoi(num_mutex));
            if(atoi(num_mutex) <= NB_MUTEX && atoi(num_mutex) >0){
                // On rajoute le client sur la file d'attente
                if(f_a->longueurs < NB_PROC_MAX_FA){
                    sprintf(buff_emission, "Demande Mutex prise en compte");
                    CHECK(nbcar = send(client_sd, buff_emission, strlen(buff_emission) + 1, 0),"Problème d'émission !!!\n");
                    // Je me mets dans la file d'attente
                    f_a->file[f_a->longueurs] = getpid();
                    f_a->longueurs++;
                    // J'attends que ce soit mon tour
                    while(1){
                        if((f_a->current_check>0) && (f_a->file[f_a->current_check]==getpid())){
                            CHECK(sem_wait(lock),"sem_wait(lock])");
                            if((f_a->resources & atoi(num_mutex)) == 0){ // & logique bit à bit. Il faut que aucun bit soient en commun entre ceux demandés et ceux occupés
                                sleep(0.5); // Parce que si il n'y a pas d'attente et que le gestionnaire répond tout de suite, le client n'a pas le temps de capter la réponse
                                break;
                            }
                            else{
                                f_a->current_check++; //Si les ressources ne sont pas dispo maintenant alors il faut passer au prochain demandeur dans la file d'attente
                                if(f_a->current_check == f_a->longueurs){
                                    f_a->current_check = 0;
                                }
                                CHECK(sem_post(lock),"sem_post(lock)");
                            }
                        }
                    }
                    // C'est mon tour
                    f_a->resources |= atoi(num_mutex); // Prendre les ressources
                    f_a->current_check = -1; //On met cette valeur temporaire le temps de faire nos modifications, afin d'éviter qu'un processus fils autre ne commence à prendre des ressources
                    // On met à jour la file d'attente
                    // Déplacer chaque élément vers la position précédente
                    for (int i = f_a->current_check; i < f_a->longueurs - 1; i++) {
                        f_a->file[i] = f_a->file[i + 1];
                    }
                    // Décrémenter la longueur de la file d'attente
                    f_a->longueurs--;
                    // Mettre à jour le dernier élément (ici on le met à 0)
                    f_a->file[f_a->longueurs] = 0;
                    CHECK(sem_post(lock),"sem_post(lock)");
                    f_a->current_check = 0;
                    sprintf(buff_emission, "Mutex obtenue");
                    CHECK(nbcar = send(client_sd, buff_emission, strlen(buff_emission) + 1, 0),"Problème d'émission !!!\n");
                    printf("Mutex %d donnée\n",atoi(num_mutex));
                }
                else {
                    // File d'attente pleine
                    sprintf(buff_emission, "File d'attente pleine pour cette mutex");
                    CHECK(nbcar = send(client_sd, buff_emission, strlen(buff_emission) + 1, 0),"Problème d'émission !!!\n");
                    printf("File d'attente pleine\n");
                }
            }
            else{
                // Mutex inconnue
                sprintf(buff_emission, "Mutex inconnue");
                CHECK(nbcar = send(client_sd, buff_emission, strlen(buff_emission) + 1, 0),"Problème d'émission !!!\n");
                printf("Mutex %d inconnue\n",atoi(num_mutex));
            }   
        }
        else if(buff_reception[0]=='1'){
            // Restitution de mutex
            strcpy(num_mutex, buff_reception + 1); // Copie à partir du deuxième caractère
            printf("Demande de restitution de la mutex %d\n",atoi(num_mutex));
            if(atoi(num_mutex) <= NB_MUTEX && atoi(num_mutex) >0){
                CHECK(sem_wait(lock),"sem_wait(lock)");
                f_a->resources &= ~atoi(num_mutex); // Libérer les ressources
                CHECK(sem_post(lock),"sem_wait(lock)");
                sprintf(buff_emission, "Mutex restituée"); 
                printf("Mutex %d restituée\n",atoi(num_mutex));
            }
            else{
                // Mutex inconnue
                sprintf(buff_emission, "Mutex inconnue");
                printf("Mutex %d inconnue\n",atoi(num_mutex));
            }
            CHECK(nbcar = send(client_sd, buff_emission, strlen(buff_emission) + 1, 0),"Problème d'émission !!!\n");
        }
        else{
            // Demande non prise en compte
            sprintf(buff_emission, "Erreur : Demande non prise en compte");
            CHECK(nbcar = send(client_sd, buff_emission, strlen(buff_emission) + 1, 0),"Problème d'émission !!!\n");
            printf("Demande non prise en compte\n");
        }
    }
    
}
