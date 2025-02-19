#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "include/schneider.h"

#define CHECK_ERROR(val1,val2,msg)   if (val1==val2) \
                                    { perror(msg); \
                                        exit(EXIT_FAILURE); }

#define DEFAULT_REMOTEPORT   8000
#define DEFAULT_REMOTE_IP     "127.0.0.1"
#define MAXOCTETS   150
#define IP_AUTOMATE "10.31.125.14"
#define XWAY_ADRESS 35

#define IP_SIZE 16
char ip[IP_SIZE];
int port;

void init_tcp_socket(int *sd, char *remote_ip, u_int16_t remote_port){
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    *sd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK_ERROR(*sd, -1, "Erreur socket non cree !!! \n");
    
    //preparation de l'adresse de la socket
    addr.sin_family = AF_INET;
    addr.sin_port = htons(remote_port);
    addr.sin_addr.s_addr = inet_addr(remote_ip);
    
    // Connexion au serveur distant
    int erreur = connect(*sd, (struct sockaddr *)&addr, addr_len);
    CHECK_ERROR(erreur, -1, "Erreur de connexion !!!\n");

    printf("Connexion établie\n");
}

void send_message_ress(int sd_ress, const char *message) {
    int nbcar = send(sd_ress, message, strlen(message) + 1, 0);
    CHECK_ERROR(nbcar, 0, "\nProblème d'émission !!!\n");
}

void receive_message_ress(int sd_ress, char *buffer) {
    int nbcar = recv(sd_ress, buffer, MAXOCTETS, 0);
    CHECK_ERROR(nbcar, -1, "\nProblème de réception !!!\n");
    buffer[nbcar] = '\0';
    printf("MSG RECU DU SERVEUR : %s\n", buffer);
}

void take_mutex(int nb_mutex, char * buff_emission, char * buff_reception, int sd_ress , char * message ){
    printf("CLIENT> ");
    sprintf(buff_emission, "0%d\n", nb_mutex);
    buff_emission[strlen(buff_emission) - 1] = '\0';
    printf("%s\n", buff_emission); 
    send_message_ress(sd_ress, buff_emission);

    // Réception du message du serveur
    receive_message_ress(sd_ress, buff_reception); //attente du message "Demande mutex prise en compte"

    if(strcmp(message, buff_reception) == 0) {
        receive_message_ress(sd_ress, buff_reception); //attente du message "Mutex xx donnée"
    }
    else {
        printf("Problème, mutex non donnée"); 
    }
}

void release_mutex(int nb_mutex, char * buff_emission, char * buff_reception, int sd_ress , char * message ){
    printf("CLIENT> ");
    sprintf(buff_emission, "1%d\n", nb_mutex);
    buff_emission[strlen(buff_emission) - 1] = '\0';
    printf("%s\n", buff_emission); 
    send_message_ress(sd_ress, buff_emission);

    // Réception du message du serveur
    receive_message_ress(sd_ress, buff_reception); //attente du message "Mutex restituée"

}

void write_demand(uint16_t troncon, uint16_t aiguillage, uint16_t adresse_mot, int sd, uint16_t pc_adress, uint16_t api_xway_adress, WriteInformation *write_info) {
    write_info->troncon = troncon;
    write_info->aiguillage = aiguillage;
    write_info->adresse_premier_mot = adresse_mot;
    
    transmit_command(sd, "write", pc_adress, api_xway_adress, write_info);
}

void handle_sigint(int sig) {

    extern int sd_ress, sd_api; 

    if (sd_ress != -1) CHECK_ERROR(close(sd_ress), -1, "Erreur lors de la fermeture de la socket de gestion des ressources");
    if (sd_api != -1) CHECK_ERROR(close(sd_api), -1, "Erreur lors de la fermeture de la socket de l'API");
   
    printf("\nSockets fermées proprement. Fin du programme.\n");
    exit(EXIT_SUCCESS);

}

int main(int argc, char *argv[]) {
    
    /********************************* Rappel ligne de commande  ********************************/
    printf("Usage: %s <API IP> <API Port>  <API XWAY Adress> [RESS Port] [RESS IP]\n", argv[0]); 
    if (argc < 4){
        printf("Fail : Usage\n");
        exit(EXIT_FAILURE);
    }

    /************************************Controle C handler *************************************/
    struct sigaction sa;
    sa.sa_handler = handle_sigint; 
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);  

    /**************************** Connexion socket de gestion des ressources ********************/
    
    if(argc == 5){
        port = atoi(argv[4]);
        strncpy(ip, DEFAULT_REMOTE_IP, IP_SIZE - 1);
        ip[IP_SIZE - 1] = '\0';
    }
    else if(argc == 6){
        port = atoi(argv[4]);
        strncpy(ip, argv[5], IP_SIZE - 1);
        ip[IP_SIZE - 1] = '\0';
    }
    else{
        port = DEFAULT_REMOTEPORT;
        strncpy(ip, DEFAULT_REMOTE_IP, IP_SIZE - 1);
        ip[IP_SIZE - 1] = '\0';
    }
    
    static int sd_ress = -1;

    char buff_emission[MAXOCTETS+1];
    char buff_reception[MAXOCTETS+1];

    char message_mutex_demandee[MAXOCTETS+1];
    sprintf(message_mutex_demandee, "Demande Mutex prise en compte");

    char message_mutex_relachee[MAXOCTETS+1];
    sprintf(message_mutex_relachee, "Mutex restituée");

    init_tcp_socket(&sd_ress, ip, port); 

    /****************************** Connexion à l'API automate #xway *****************************/

    static int sd_api = -1;

    char * api_ip = argv[1]; // IP de l'API
    u_int16_t api_port = (uint16_t) atoi(argv[2]); // Port TCP ouvert de l'API
    int api_xway_adress = atoi(argv[3]); // Adresse XWAY de l'API

    /*   Création de la socket de dialogue TCP   */

    init_tcp_socket(&sd_api, api_ip, api_port);

    /*   Fin d'établissement de connexion TCP   */

    u_int16_t pc_adress = XWAY_ADRESS << 8 | 0x10;
    api_xway_adress = api_xway_adress << 8 | 0x10;

    WriteInformation * write_info = malloc(sizeof(WriteInformation));


    /****************************** Réseau de pétri du train 2 **********************************/
    int state = 0; 

    while(1){
        switch(state){
            case(0): // En Ti04, demande Tn04
                write_demand(0x4, 0xFFFF, 42, sd_api, pc_adress, api_xway_adress, write_info);
                state++;
                break; 
            case(1): // En T22, demande R7
                take_mutex(7, buff_emission, buff_reception, sd_ress, message_mutex_demandee); 
                state++;
                break;
            case(2): // En T22 avec R7, demande R1
                take_mutex(1, buff_emission, buff_reception, sd_ress, message_mutex_demandee); 
                state++;
                break; 
            case(3): // En T22 avec R7 et R1, demande R2
                take_mutex(2, buff_emission, buff_reception, sd_ress, message_mutex_demandee); 
                state++;
                break;
            case(4): // En T22 avec R7 et R1 et R2, demande A7d, A11b et PA3d
                write_demand(0xFFFF, 0x7, 42, sd_api, pc_adress, api_xway_adress, write_info);
                state++;
                break;
            case(5): // En T22 avec R7 et R1 et R2, A7d, A11b et PA3d, demande avancement T22
                write_demand(0x16, 0xFFFF, 42, sd_api, pc_adress, api_xway_adress, write_info);
                state ++; 
                break; 
            case(6): // En T27 avec R7, R1 et R2, lib R1
                release_mutex(1, buff_emission, buff_reception, sd_ress, message_mutex_relachee); 
                state ++; 
                break; 
            case(7): // En T27 avec R7 et R2, lib R2
                release_mutex(2, buff_emission, buff_reception, sd_ress, message_mutex_relachee); 
                state ++; 
                break; 
            case(8): // En T27 avec R7, demande avancement T27
                write_demand( 0x1B, 0xFFFF, 42, sd_api, pc_adress, api_xway_adress, write_info);
                state ++; 
                break; 
            case(9): // En T28 avec R7, demande R5
                take_mutex(5, buff_emission, buff_reception, sd_ress, message_mutex_demandee); 
                state ++; 
                break; 
            case(10): // En T28 avec R7 et R5, demande A13d 
                write_demand( 0xFFFF, 0xD, 42, sd_api, pc_adress, api_xway_adress, write_info);
                state ++; 
                break; 
            case(11): // En T28 avec R7 et R5, A13d , demande T28
                write_demand( 0x1C, 0xFFFF, 42, sd_api, pc_adress, api_xway_adress, write_info);
                state ++; 
                break; 
            case(12): // En Ti09 avec R7 et R5, lib R7
                release_mutex(7, buff_emission, buff_reception, sd_ress, message_mutex_relachee); 
                state ++; 
                break; 
            case(13): // En Ti09 avec R5, demande A12d
                write_demand( 0xFFFF, 0xC, 42, sd_api, pc_adress, api_xway_adress, write_info);
                state ++; 
                break; 
            case(14): // En Ti09 avec R5 et A12d, demande avancement Tn09
                write_demand( 0x9, 0xFFFF, 42, sd_api, pc_adress, api_xway_adress, write_info);
                state ++; 
                break; 
            case(15): // En T24 avec R5, lib R5
                release_mutex(5, buff_emission, buff_reception, sd_ress, message_mutex_relachee); 
                state ++; 
                break; 
            case(16): // En T24, demande PA0d, A5d, A6d
                write_demand(0xFFFF, 0x14, 42, sd_api, pc_adress, api_xway_adress, write_info);
                state ++ ;
                break;
            case(17): // En T24, avec PA0d, A5d, A6d,  demande avancement T24
                write_demand(0x18, 0xFFFF, 42, sd_api, pc_adress, api_xway_adress, write_info);
                state = 0;
                break;
        }
    }




    CHECK_ERROR(close(sd_ress), -1, "Erreur lors de la fermeture de la socket de gestion des ressources");
    CHECK_ERROR(close(sd_api), -1, "Erreur lors de la fermeture de la socket de l'API");

    exit(EXIT_SUCCESS);
}
