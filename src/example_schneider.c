#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "include/schneider.h"

#define XWAY_ADRESS 35

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


int main(int argc, char * argv[]){
    int sd;

    if (argc != 4){
        printf("Usage: %s <API IP> <API Port>  <API XWAY Adress>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char * api_ip = argv[1]; // IP de l'API
    u_int16_t api_port = (uint16_t) atoi(argv[2]); // Port TCP ouvert de l'API
    int api_xway_adress = atoi(argv[3]); // Adresse XWAY de l'API

    /*   Création de la socket de dialogue TCP   */

    init_tcp_socket(&sd, api_ip, api_port);

    /*   Fin d'établissement de connexion TCP   */

    u_int16_t pc_adress = XWAY_ADRESS << 8 | 0x10;
    api_xway_adress = api_xway_adress << 8 | 0x10;

    WriteInformation * write_info = malloc(sizeof(WriteInformation));

    write_info->troncon = 0x0000;
    write_info->aiguillage = 0xFFFF; // si l'un est à une valeur, l'autre doit être à 0xFFFF

    // chaque train à un numéro qui lui est associé. À écrire une seule fois au début
    write_info->adresse_premier_mot = 39; // dépendant du train qui fait la requete

    transmit_command(sd, "write", pc_adress, api_xway_adress, write_info);
    close(sd);
    return EXIT_SUCCESS;
}
