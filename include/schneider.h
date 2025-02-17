#ifndef SCHNEIDER_H
#define SCHNEIDER_H

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

#define HEADER_SIZE 7
#define MAXOCTETS   150

// write only one value to troncon or aiguillage into this simultanously, the other must me at 0xFFFF
typedef struct {
    u_int16_t troncon;
    u_int16_t aiguillage;
    u_int16_t adresse_premier_mot;
} WriteInformation;

void create_frame(u_int8_t * frame, int total_length, u_int16_t pc_adress, u_int16_t server_adress, u_int8_t mode, u_int8_t thread, u_int8_t command, WriteInformation *write_info);
void write_mobus_tcp(u_int8_t * frame, int total_length);
void write_unite_request(u_int8_t * frame, int total_length, u_int16_t pc_adress, u_int16_t server_adress, u_int8_t mode, u_int8_t thread);
void write_command(u_int8_t * frame, u_int8_t command, int starting_byte, u_int16_t pc_adress, WriteInformation * write_info);
char * read_response(u_int8_t * frame);
void handle_communication(int sd, u_int8_t * buff_emission, u_int8_t * buff_reception, int pc_adress, int api_xway_adress, int total_length, char * user_command);
void transmit_command(int sd, char * command, int pc_adress, int api_xway_adress, WriteInformation * write_info);

#endif
