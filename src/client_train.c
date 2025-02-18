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

#define DEFAULT_REMOTEPORT   8080
#define DEFAULT_REMOTE_IP     "127.0.0.1"
#define MAXOCTETS   150

#define IP_SIZE 16
char ip[IP_SIZE];
int port;

int main(int argc, char *argv[]) {
    
    if(argc == 2){
        port = atoi(argv[1]);
        strncpy(ip, DEFAULT_REMOTE_IP, IP_SIZE - 1);
        ip[IP_SIZE - 1] = '\0';
    }
    else if(argc == 3){
        port = atoi(argv[1]);
        strncpy(ip, argv[2], IP_SIZE - 1);
        ip[IP_SIZE - 1] = '\0';
    }
    else{
        port = DEFAULT_REMOTEPORT;
        strncpy(ip, DEFAULT_REMOTE_IP, IP_SIZE - 1);
        ip[IP_SIZE - 1] = '\0';
    }

    printf("Adresse IP distante : %s, Port : %d\n",ip,port);
    
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
    adrlect.sin_port = htons(port);
    adrlect.sin_addr.s_addr = inet_addr(ip);
    
    // Connexion au serveur distant
    erreur = connect(sd, (struct sockaddr *)&adrlect, adrlect_len);
    CHECK_ERROR(erreur, -1, "Erreur de connexion !!!\n");

    printf("0 -> demande de mutex\n1 -> restitution de mutex\nExemple : 05 -> demande mutex 5\n");

    while(1){
        // Envoi du message au serveur
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
            printf("On attends la mutex\n");
            // Réception du message du serveur
            nbcar = recv(sd, buff_reception, MAXOCTETS, 0);
            CHECK_ERROR(nbcar, -1, "\nProblème de réception !!!\n");
            buff_reception[nbcar] = '\0';
            printf("MSG RECU DU SERVEUR : %s\n", buff_reception);
        }

    }
    CHECK_ERROR(close(sd), -1, "Erreur lors de la fermeture de la socket");
    exit(EXIT_SUCCESS);
}
