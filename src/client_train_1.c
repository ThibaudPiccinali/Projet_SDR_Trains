#include <stdio.h>
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

#define DEFAULT_REMOTEPORT   3000
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


int main(int argc, char *argv[]) {

    /********************************* Rappel ligne de commande  ********************************/
    printf("Usage: %s <API IP> <API Port>  <API XWAY Adress> <RESS Port> <RESS IP>\n", argv[0]); 

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
    
    
    int sd_ress;

    char buff_emission[MAXOCTETS+1];
    char buff_reception[MAXOCTETS+1];

    char message_mutex_demandee[MAXOCTETS+1];
    sprintf(message_mutex_demandee, "Demande Mutex prise en compte");

    char message_mutex_relachee[MAXOCTETS+1];
    sprintf(message_mutex_relachee, "Mutex restituée");

    init_tcp_socket(&sd_ress, ip, port); 

    /****************************** Connexion à l'API automate #xway *****************************/

    int sd_api;

    if (argc < 4){
        printf("Fail : Attention aux arguments en ligne de commande ! Les 4 premiers sont nécessaires\n");
        exit(EXIT_FAILURE);
    }

    char * api_ip = argv[1]; // IP de l'API
    u_int16_t api_port = (uint16_t) atoi(argv[2]); // Port TCP ouvert de l'API
    int api_xway_adress = atoi(argv[3]); // Adresse XWAY de l'API

    /*   Création de la socket de dialogue TCP   */

    init_tcp_socket(&sd_api, api_ip, api_port);

    /*   Fin d'établissement de connexion TCP   */

    u_int16_t pc_adress = XWAY_ADRESS << 8 | 0x10;
    api_xway_adress = api_xway_adress << 8 | 0x10;

    WriteInformation * write_info = malloc(sizeof(WriteInformation));


    /****************************** Réseau de pétri du train 1 **********************************/
    int state = 0; 

    while(1){
        switch(state){
            case(0): // En Ti03, demande Tj1d et Pa0d
                write_demand(0xFFFF, 0x1F, 39, sd_api, pc_adress, api_xway_adress, write_info);
                state++;
                break; 
            case(1): // En Ti03 avec ack Tj1d et Pa0d, demande avancement Tn03
                write_demand(0x3, 0xFFFF, 39, sd_api, pc_adress, api_xway_adress, write_info);
                state++;
                break;
            case(2): // En T23, demande R6
                take_mutex(6, buff_emission, buff_reception, sd_ress, message_mutex_demandee); 
                state++;
                break; 
            case(3): // En T23 avec R6, demande Pa2d et Tj2d
                write_demand(0xFFFF, 0x16, 39, sd_api, pc_adress, api_xway_adress, write_info);
                state++;
                break;
            case(4): // En T23 avec R6, Pa2d et Tj2d, demande avancement T23
                write_demand(0x17, 0xFFFF, 39, sd_api, pc_adress, api_xway_adress, write_info);
                state++;
                break;
            case(5): // En Ti10, relache R6
                release_mutex(6, buff_emission, buff_reception, sd_ress, message_mutex_relachee); 
                state ++; 
                break; 
            case(6): // En Ti10 sans R6, demande R3
                take_mutex(3, buff_emission, buff_reception, sd_ress, message_mutex_demandee); 
                state ++; 
                break; 
            case(7): // En Ti10 avec R3, demande R4
                take_mutex(4, buff_emission, buff_reception, sd_ress, message_mutex_demandee); 
                state ++; 
                break; 
            case(8): // En Ti10 avec R3 et R4, demande Tj3d
                write_demand( 0xFFFF, 0x21, 39, sd_api, pc_adress, api_xway_adress, write_info);
                state ++; 
                break; 
            case(9): // En Ti10 avec R3 et R4 et Tj3d, demande avancement Tn10
                write_demand( 0xA, 0xFFFF, 39, sd_api, pc_adress, api_xway_adress, write_info);
                state ++; 
                break; 
            case(10): // En T29 avec R3 et R4, libère R4
                release_mutex(4, buff_emission, buff_reception, sd_ress, message_mutex_relachee); 
                state ++; 
                break; 
            case(11): // En T29 avec R3, demande R2
                take_mutex(2, buff_emission, buff_reception, sd_ress, message_mutex_demandee); 
                state ++; 
                break; 
            case(12): //En T29 avec R3 et R2, demande Pa3b, A11b et !!!A7d!!! (bloquant?)
                write_demand( 0xFFFF, 0x3, 39, sd_api, pc_adress, api_xway_adress, write_info);
                state ++; 
                break; 
            case(13): //En T29 avec R3 et R2, Pa3b, A11b et A7d , demande avancement T29
                write_demand( 0x1D, 0xFFFF, 39, sd_api, pc_adress, api_xway_adress, write_info);
                state ++; 
                break; 
            case(14): // En T19, avec R3 et R2, relache R3
                release_mutex(3, buff_emission, buff_reception, sd_ress, message_mutex_relachee); 
                state ++; 
                break; 
            case(15): // En T19, avec R2, relache R2
                release_mutex(2, buff_emission, buff_reception, sd_ress, message_mutex_relachee); 
                state ++; 
                break; 
            case(16): // En T19, demande T19
                write_demand(0x13, 0xFFFF, 39, sd_api, pc_adress, api_xway_adress, write_info);
                state = 0;
                break;
        }
    }



    CHECK_ERROR(close(sd_ress), -1, "Erreur lors de la fermeture de la socket de gestion des ressources");
    CHECK_ERROR(close(sd_api), -1, "Erreur lors de la fermeture de la socket de l'API");

    exit(EXIT_SUCCESS);
}
