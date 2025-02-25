#include "include/schneider.h"

void create_frame(u_int8_t * frame, int total_length, u_int16_t pc_adress, u_int16_t server_adress, u_int8_t mode, u_int8_t thread, u_int8_t command, WriteInformation * write_info){
    write_mobus_tcp(frame, total_length);
    write_unite_request(frame, total_length, pc_adress, server_adress, mode, thread);
    write_command(frame, command, HEADER_SIZE + 7 , pc_adress, write_info);
}

void write_mobus_tcp(u_int8_t * frame, int total_length){
    frame[0] = 0x00;
    frame[1] = 0x00;
    frame[2] = 0x00;
    frame[3] = 0x01;
    frame[4] = 0x00;
    frame[5] = total_length;
    frame[6] = 0x00;
}

void write_unite_request(u_int8_t * frame, int total_length, u_int16_t pc_adress, u_int16_t server_adress, u_int8_t mode, u_int8_t thread){
    frame[HEADER_SIZE] = 0xF1;
    frame[HEADER_SIZE + 1] = pc_adress >> 8;
    frame[HEADER_SIZE + 2] = pc_adress & 0xFF;
    frame[HEADER_SIZE + 3] = server_adress >> 8;
    frame[HEADER_SIZE + 4] = server_adress & 0xFF;
    frame[HEADER_SIZE + 5] = mode;
    frame[HEADER_SIZE + 6] = thread;
}

void write_command(u_int8_t * frame, u_int8_t command, int starting_byte, u_int16_t pc_adress, WriteInformation * write_info){
    frame[starting_byte] = command;
    if (command == 0xFE){
        return;
    }
    frame[starting_byte + 1] = 0x06;
    if (command == 0x37){
        int adresse_premier_mot = write_info->adresse_premier_mot;
        u_int16_t troncon = write_info->troncon;
        u_int16_t aiguillage = write_info->aiguillage;

        frame[starting_byte + 2] = 0x68; // pour numérique
        frame[starting_byte + 3] = 0x07; // pour mots internes
        frame[starting_byte + 4] = adresse_premier_mot & 0xFF;
        frame[starting_byte + 5] = adresse_premier_mot >> 8;
        frame[starting_byte + 6] = 3 & 0xFF; // on écrit 3 mots pour activer un tronçon ou un aiguillage
        frame[starting_byte + 7] = 3 >> 8;
        frame[starting_byte + 8] = pc_adress & 0xFF;
        frame[starting_byte + 9] = pc_adress >> 8;
        frame[starting_byte + 10] = troncon & 0xFF;
        frame[starting_byte + 11] = troncon >> 8;
        frame[starting_byte + 12] = aiguillage & 0xFF;
        frame[starting_byte + 13] = aiguillage >> 8;
    }
}

char * read_response(u_int8_t * frame){
    int length = frame[5];
    // HEADER_SIZE - 1 = taille non incluse dans length
    // length - 1 = dernier byte de la frame
    int response = frame[(HEADER_SIZE - 1) + length - 1];
    if (response == 0xFE){
        return "Demande traitee";
    }
    else if (response == 0xFD){
        return "Erreur requete";
    }
    else{
        return "Reponse inconnue";
    }
}

void handle_communication(int sd, u_int8_t * buff_emission, u_int8_t * buff_reception, int pc_adress, int api_xway_adress, int total_length, char * user_command){
    int nbcar;
    // Envoi du message au serveur
    nbcar = write(sd, buff_emission, total_length);
    CHECK_ERROR(nbcar, -1, "\nProblème d'émission !!!\n");

    #ifdef SCHNEIDER_DEBUG
    printf("Message envoyé en hexadécimal: ");
    for (int i = 0; i < nbcar; i++) {
        printf("%02X ", buff_emission[i]);
    }
    printf("\n");
    #endif

    // Réception du message du serveur
    nbcar = read(sd, buff_reception, MAXOCTETS);
    CHECK_ERROR(nbcar, -1, "\nProblème de réception !!!\n");
    CHECK_ERROR(nbcar, 0, "\nProblème de réception !!!\n");

    #ifdef SCHNEIDER_DEBUG
    printf("Message reçu en hexadécimal: ");
    for (int i = 0; i < nbcar; i++) {
        printf("%02X ", buff_reception[i]);
    }
    printf("\n");
    printf("Réponse : %s\n", read_response(buff_reception));
    #endif

    // we wait for the train to pass the next sensor
    if (strcmp(user_command, "write") == 0) {
        nbcar = read(sd, buff_reception, MAXOCTETS);
        CHECK_ERROR(nbcar, -1, "\nProblème de réception !!!\n");
        CHECK_ERROR(nbcar, 0, "\nProblème de réception !!!\n");

        #ifdef SCHNEIDER_DEBUG
        printf("Message reçu en hexadécimal: ");
        for (int i = 0; i < nbcar; i++) {
            printf("%02X ", buff_reception[i]);
        }
        printf("\n");
        #endif

        create_frame(buff_emission, 0x09, pc_adress, api_xway_adress, 0x19, buff_reception[13], 0xFE, NULL);

        nbcar = write(sd, buff_emission, 0x09 + HEADER_SIZE - 1);
        CHECK_ERROR(nbcar, -1, "\nProblème d'émission !!!\n");

        #ifdef SCHNEIDER_DEBUG
        printf("Message envoyé en hexadécimal: ");
        for (int i = 0; i < nbcar; i++) {
            printf("%02X ", buff_emission[i]);
        }
        printf("\n");
        #endif
    }
}

// to change to allow to write values from parameters
void transmit_command(int sd, char * command, int pc_adress, int api_xway_adress, WriteInformation * write_info){
    if (write_info != NULL && write_info->troncon != 0xFFFF && write_info->aiguillage != 0xFFFF){
        perror("Invalid write information. One of troncon or aiguillage must be set to 0xFFFF\n");
        exit(EXIT_FAILURE);
    }
    int total_length;
    u_int8_t buff_emission[MAXOCTETS+1];
    u_int8_t buff_reception[MAXOCTETS+1];

    if(strcmp(command, "start") == 0){
        create_frame(buff_emission, 0x0A, pc_adress, api_xway_adress, 0x09, 0x01, 0x24, NULL);
        total_length = 0x0A + HEADER_SIZE - 1;
    }
    else if(strcmp(command, "stop") == 0){
        create_frame(buff_emission, 0x0A, pc_adress, api_xway_adress, 0x09, 0x01, 0x25, NULL);
        total_length = 0x0A + HEADER_SIZE - 1;
    }
    else if(strcmp(command, "write") == 0){
        int nombre_de_mot_a_ecrire = 3;
        create_frame(buff_emission, 16 + 2 * nombre_de_mot_a_ecrire, pc_adress, api_xway_adress, 0x09, 0x01, 0x37, write_info);
        total_length = HEADER_SIZE + 15 + 2 * nombre_de_mot_a_ecrire;
    }
    else{
        perror("Invalid command. Valid commands are: start, stop, write\n");
        exit(EXIT_FAILURE);
    }
    handle_communication(sd, buff_emission, buff_reception, pc_adress, api_xway_adress, total_length, command);
}
