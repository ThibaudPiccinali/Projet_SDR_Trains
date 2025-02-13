#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define CHECK_ERROR(val1,val2,msg)   if (val1==val2) \
                                    { perror(msg); \
                                        exit(EXIT_FAILURE); }

#define REMOTEPORT   3000
#define LOCALIP     "127.0.0.1"
#define REMOTE     "127.0.0.1"
#define MAXOCTETS   150

int main(int argc, char *argv[]) {
    int sd ;
    struct sockaddr_in adrlect;
    socklen_t adrlect_len;
    int erreur;
    int nbcar;
    char buff_emission[MAXOCTETS+1];
    char buff_reception[MAXOCTETS+1];

    char message_mutex_demandee[MAXOCTETS+1];
    sprintf(message_mutex_demandee, "Demande Mutex prise en compte");

    adrlect_len = sizeof(adrlect);
    
    sd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK_ERROR(sd, -1, "Erreur socket non cree !!! \n");
    printf("N° de la socket : %d \n", sd);
    
    // Préparation de l'adresse de la socket
    adrlect.sin_family = AF_INET;
    adrlect.sin_port = htons(REMOTEPORT);
    adrlect.sin_addr.s_addr = inet_addr(REMOTE);
    
    // Connexion au serveur distant
    erreur = connect(sd, (struct sockaddr *)&adrlect, adrlect_len);
    CHECK_ERROR(erreur, -1, "Erreur de connexion !!!\n");


    // Envoi du message au serveur
    printf("0 -> demande de mutex\n1 -> restitution de mutex\nExemple : 05 -> demande mutex 5\n");
    printf("CLIENT> ");
    fgets(buff_emission, MAXOCTETS, stdin);
    buff_emission[strlen(buff_emission) - 1] = '\0';
    nbcar = send(sd, buff_emission, strlen(buff_emission) + 1, 0);
    CHECK_ERROR(nbcar, 0, "\nProblème d'émission !!!\n");


    // Réception du message du serveur
    nbcar = recv(sd, buff_reception, MAXOCTETS, 0);
    CHECK_ERROR(nbcar, -1, "\nProblème de réception !!!\n");
    buff_reception[nbcar] = '\0';
    printf("MSG RECU DU SERVEUR : %s\n", buff_reception);

    if(strcmp(message_mutex_demandee,buff_reception)==0){
        // Réception du message du serveur
        nbcar = recv(sd, buff_reception, MAXOCTETS, 0);
        CHECK_ERROR(nbcar, -1, "\nProblème de réception !!!\n");
        buff_reception[nbcar] = '\0';
        printf("MSG RECU DU SERVEUR : %s\n", buff_reception);
    }

    CHECK_ERROR(close(sd), -1, "Erreur lors de la fermeture de la socket");

    exit(EXIT_SUCCESS);
}
