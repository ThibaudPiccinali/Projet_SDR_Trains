#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <errno.h>
#include <time.h>
#include <sched.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <sys/mman.h>

#include "utils.h"

#define NB_MUTEX 6
#define NB_PROC_MAX_FA 50
#define MAXCLIENTS 10
#define MAXOCTETS 150

#define LOCALPORT 3000
#define LOCALIP "127.0.0.1"

void gestion_mutex();
void bye();

 // Memoire partagée
struct file_attente {
    pid_t file[NB_MUTEX][NB_PROC_MAX_FA]; // 50 personnes MAX dans chaque file d'attente
    int longueurs[NB_MUTEX]; // Longueurs actuelles des files d'attente
};
struct file_attente* f_a;
size_t size = sizeof(f_a);
int shm_fd;

sem_t* mutex[NB_MUTEX];

int se;
int erreur;
struct sockaddr_in adrserveur;
struct sockaddr_in adrclient;
socklen_t adrclient_len = sizeof(adrclient);

int main() {
    pid_t pid;

    // Création de la socket d'écoute
    CHECK(se=socket(AF_INET, SOCK_STREAM, 0),"Erreur socket d'écoute non cree !!! \n");
    // Permet de réutiliser l'adresse
    int opt = 1;
    CHECK_0(setsockopt(se, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)),"setsockopt\n");
    //preparation de l'adresse de la socket
    adrserveur.sin_family = AF_INET;
    adrserveur.sin_port = htons(LOCALPORT);
    adrserveur.sin_addr.s_addr = inet_addr(LOCALIP);
    //Affectation d'une adresse a la socket
    CHECK(erreur = bind(se, (const struct sockaddr *)&adrserveur, sizeof(adrserveur)),"Erreur de bind !!!\n");

    // Gestion de mémoire partagée (file_attente)
    // Création ou ouverture de la mémoire partagée
    CHECK(shm_fd = shm_open("file_attente", O_CREAT | O_RDWR, 0666),"shm_open(file_attente)");
    // Définir la taille de la mémoire partagée pour contenir la structure SharedData
    size = sysconf(_SC_PAGE_SIZE); 
    CHECK(ftruncate(shm_fd, size),"ftruncate(shm_fd)");
    // Mappage de la mémoire partagée
    CHECK_MAP(f_a = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0),"mmap");

    // Initialisation de la file d'attente
    for (int i = 0; i < NB_MUTEX; i++) {
        f_a->longueurs[i] = 0;
        for (int j = 0; j < NB_PROC_MAX_FA; j++) {
            f_a->file[i][j] = 0;
        }
    }

    // Création des mutex
    char mutex_name[20];
    for(int i = 0;i<NB_MUTEX;i++){
        sprintf(mutex_name, "mutex[%d]", i);
        CHECK_S(mutex[i] = sem_open(mutex_name,O_CREAT|O_EXCL,0666,1),"sem_open(mutex_name)");
    }

    // Permet de faire le cleanning des sémaphores lors des exits
    atexit(bye);// bye detruit les semaphores

    // Avec ça le père sera "imunisé" au Ctrl-C mais pas ses fils (on va réactiver le SIGINT pour eux) ! 
    // Du coup ils vont tous se terminer, sauf le père qui va pouvoir les récupérer et terminer correctement est donc faire le nettoyage des sémaphores
    // Masque SIGINT pour le père
    sigset_t Mask,OldMask;
    CHECK(sigemptyset(&Mask), "sigemptyset()");
    CHECK(sigaddset(&Mask , SIGINT), "sigaddset(SIGINT)");
    CHECK(sigprocmask(SIG_SETMASK , &Mask , &OldMask), "sigprocmask()");

    // Création du fils du processus père
    CHECK(pid=fork(),"fork(pid)");
    if (pid==0){
        // Démasque SIGINT
        CHECK(sigprocmask(SIG_SETMASK , &OldMask , NULL), "sigprocmask()");
        gestion_mutex();
    }

    // Processus Père
    // Attente de la terminaison du fils
    int status;
    CHECK(wait(&status), "wait()");
    CHECK(close(se), "Erreur lors de la fermeture de la socket d'écoute");;
    return 0;
}

void bye(){
    
    // Fermeture et supression des sémaphores nommées
    char mutex_name[20];
    for(int i =0;i<NB_MUTEX;i++){
        sprintf(mutex_name, "mutex[%d]", i);
        CHECK(sem_close(mutex[i]),"sem_close(mutex_name)");
        CHECK(sem_unlink(mutex_name),"sem_unlink(mutex_name)");
    }

    // Suppression de la mémoire partagée
    CHECK(munmap(f_a, size),"munmap(file_attente)");
    CHECK(close(shm_fd),"close(shm_fd)");
    CHECK(shm_unlink("file_attente"),"shm_unlink(file_attente)");

}

void gestion_mutex(){
    char buff_reception[MAXOCTETS + 1];
    char buff_emission[MAXOCTETS + 1];
    char num_mutex[MAXOCTETS + 1];
    int nbcar;
    int client_sd;
    listen(se, MAXCLIENTS);
    while (1){
        CHECK(client_sd = accept(se, (struct sockaddr *)&adrclient, &adrclient_len),"Erreur de accept !!!\n");
        CHECK(nbcar = recv(client_sd, buff_reception, MAXOCTETS, 0),"Problème de réception !!!\n");
        buff_reception[nbcar] = '\0';
        printf("MSG RECU DU CLIENT %d : %s\n",client_sd, buff_reception);
        // Faire ici des cas en fonction de la demande du client (demande ou restitution de Mutex)
        if(buff_reception[0]=='0'){
            // Demande de Mutex
            strcpy(num_mutex, buff_reception + 1); // Copie à partir du deuxième caractère
            printf("Demande de prise de la mutex %d\n",atoi(num_mutex));
            if(atoi(num_mutex) < NB_MUTEX){
                // On rajoute le client sur la file d'attente
                if(f_a->longueurs[atoi(num_mutex)] < NB_PROC_MAX_FA){
                    sprintf(buff_emission, "Demande Mutex prise en compte");
                    CHECK(nbcar = send(client_sd, buff_emission, strlen(buff_emission) + 1, 0),"Problème d'émission !!!\n");
                    // On crée un processus fils
                    pid_t pid;
                    CHECK(pid=fork(),"fork(pid)");
                    if(pid==0){
                        // Je suis le fils
                        // Je me mets dans la file d'attente
                        f_a->file[atoi(num_mutex)][f_a->longueurs[atoi(num_mutex)]] = getpid();
                        f_a->longueurs[atoi(num_mutex)]++;
                        // J'attends que ce soit mon tour
                        while(1){
                            if(f_a->file[atoi(num_mutex)][0]==getpid()){
                                break;
                            }
                        }
                        // C'est mon tour
                        // Je demande la mutex
                        CHECK(sem_wait(mutex[atoi(num_mutex)]),"sem_wait(mutex[atoi(num_mutex)])");
                        // Je l'obtiens
                        // On met à jour la file d'attente
                        // Déplacer chaque élément vers la position précédente
                        for (int i = 0; i < f_a->longueurs[atoi(num_mutex)] - 1; i++) {
                            f_a->file[atoi(num_mutex)][i] = f_a->file[atoi(num_mutex)][i + 1];
                        }
                        // Décrémenter la longueur de la file d'attente
                        f_a->longueurs[atoi(num_mutex)]--;
                        // Mettre à jour le dernier élément (ici on le met à 0)
                        f_a->file[atoi(num_mutex)][f_a->longueurs[atoi(num_mutex)]] = 0;
                        sprintf(buff_emission, "Mutex obtenue");
                        CHECK(nbcar = send(client_sd, buff_emission, strlen(buff_emission) + 1, 0),"Problème d'émission !!!\n");
                        CHECK(close(client_sd), "Erreur lors de la fermeture de la socket client");
                        printf("Mutex %d donnée\n",atoi(num_mutex));
                    }
                }
                else{
                    // File d'attente pleine
                    sprintf(buff_emission, "File d'attente pleine pour cette mutex");
                    CHECK(nbcar = send(client_sd, buff_emission, strlen(buff_emission) + 1, 0),"Problème d'émission !!!\n");
                    CHECK(close(client_sd), "Erreur lors de la fermeture de la socket client");
                    printf("File d'attente pleine\n");
                }
            }
            else{
                // Mutex inconnue
                sprintf(buff_emission, "Mutex inconnue");
                CHECK(nbcar = send(client_sd, buff_emission, strlen(buff_emission) + 1, 0),"Problème d'émission !!!\n");
                CHECK(close(client_sd), "Erreur lors de la fermeture de la socket client");
                printf("Mutex %d inconnue\n",atoi(num_mutex));
            }   
        }
        else if(buff_reception[0]=='1'){
            // Restitution de mutex
            strcpy(num_mutex, buff_reception + 1); // Copie à partir du deuxième caractère
            printf("Demande de restitution de la mutex %d\n",atoi(num_mutex));
            if(atoi(num_mutex) < NB_MUTEX){
                CHECK(sem_post(mutex[atoi(num_mutex)]),"sem_post(mutex_name)");// On rend dispo la mutex
                sprintf(buff_emission, "Mutex restituée"); 
                printf("Mutex %d restituée\n",atoi(num_mutex));
            }
            else{
                // Mutex inconnue
                sprintf(buff_emission, "Mutex inconnue");
                printf("Mutex %d inconnue\n",atoi(num_mutex));
            }
            CHECK(nbcar = send(client_sd, buff_emission, strlen(buff_emission) + 1, 0),"Problème d'émission !!!\n");
            CHECK(close(client_sd), "Erreur lors de la fermeture de la socket client");

        }
        else{
            // Demande non prise en compte
            sprintf(buff_emission, "Erreur : Demande non prise en compte");
            CHECK(nbcar = send(client_sd, buff_emission, strlen(buff_emission) + 1, 0),"Problème d'émission !!!\n");
            CHECK(close(client_sd), "Erreur lors de la fermeture de la socket client");
            printf("Demande non prise en compte\n");
        }
    }
    
}