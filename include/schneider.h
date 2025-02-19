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

/**
 * @brief Structure containing information for writing to a device.
 *
 * If troncon is put to a value, aiguillage must be set to 0xFFFF and vice versa.
 *
 * @param troncon The section of the track.
 * @param aiguillage The switch information.
 * @param adresse_premier_mot The address of the first word. Depends on who uses the request.
 */
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

/**
 * @brief Transmits a command to a specified address.
 *
 * This function sends a command to a device using the provided socket descriptor.
 *
 * @param sd The socket descriptor used for communication.
 * @param command The command to be transmitted.
 * @param pc_adress The address of the PC to which the command is sent.
 * @param api_xway_adress The API XWAY address for the communication.
 * @param write_info A pointer to a WriteInformation structure containing additional data for the transmission.
 * 
 * @exception Stops the thread if the write information is invalid.
 */
void transmit_command(int sd, char * command, int pc_adress, int api_xway_adress, WriteInformation * write_info);

#endif
