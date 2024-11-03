#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "KISS.h"
#include "Serial.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

int frame_len;
bool IN_FRAME;
bool ESCAPE;

uint8_t kiss_command = CMD_UNKNOWN;
uint8_t frame_buffer[MAX_PAYLOAD];
uint8_t write_buffer[MAX_PAYLOAD * 2 + 3];

extern bool verbose;
extern bool daemonize;
extern int attached_if;
extern int device_type;
extern void cleanup(void);
extern void encryptDecrypt(uint8_t *input, uint8_t *output, int len);

void kiss_frame_received(int frame_len)
{
    if ((device_type == IF_TUN && frame_len >= TUN_MIN_FRAME_SIZE) || (device_type == IF_TAP && frame_len >= ETHERNET_MIN_FRAME_SIZE))
    {
        int written = write(attached_if, frame_buffer, frame_len);
        if (written == -1)
        {
            if (verbose && !daemonize)
                printf("Could not write received KISS frame (%d bytes) to network interface, is the interface up?\r\n", frame_len);
        }
        else if (written != frame_len)
        {
            if (!daemonize)
                printf("Error: Could only write %d of %d bytes to interface", written, frame_len);
            cleanup();
            exit(1);
        }
        if (verbose && !daemonize)
            printf("Got %d bytes from TNC, wrote %d bytes to interface\r\n", frame_len, written);
    }
}

void kiss_serial_read(uint8_t sbyte)
{    
    if (IN_FRAME && sbyte == FEND && kiss_command == CMD_DATA)
    {
        IN_FRAME = false;
        kiss_frame_received(frame_len);
    }
    else if (sbyte == FEND)
    {
        IN_FRAME = true;
        kiss_command = CMD_UNKNOWN;
        frame_len = 0;
    }
    else if (IN_FRAME && frame_len < MAX_PAYLOAD)
    {
        // Have a look at the command byte first
        if (frame_len == 0 && kiss_command == CMD_UNKNOWN)
        {
            // Strip of port nibble
            kiss_command = sbyte & 0x0F;
        }
        else if (kiss_command == CMD_DATA)
        {
            if (sbyte == FESC)
            {
                ESCAPE = true;
            }
            else
            {
                if (ESCAPE)
                {
                    if (sbyte == TFEND)
                        sbyte = FEND;
                    if (sbyte == TFESC)
                        sbyte = FESC;
                    ESCAPE = false;
                }

                if (frame_len < MAX_PAYLOAD)
                {
                    frame_buffer[frame_len++] = sbyte;
                }
            }
        }
    }
}

int kiss_write_frame(int type, char *tcp_host, int tcp_port, int serial_port, uint8_t *buffer, int frame_len)
{
    struct sockaddr_in client_addr;

    int write_len = 0;
    write_buffer[write_len++] = FEND;
    write_buffer[write_len++] = CMD_DATA;
    for (int i = 0; i < frame_len; i++)
    {
        uint8_t byte = buffer[i];
        if (byte == FEND)
        {
            write_buffer[write_len++] = FESC;
            write_buffer[write_len++] = TFEND;
        }
        else if (byte == FESC)
        {
            write_buffer[write_len++] = FESC;
            write_buffer[write_len++] = TFESC;
        }
        else
        {
            write_buffer[write_len++] = byte;
        }
    }
    write_buffer[write_len++] = FEND;

    uint8_t write_buffer_enc[write_len];
 
    encryptDecrypt(write_buffer, write_buffer_enc, write_len);

    if (type == 1)
    {
        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sin_family = AF_INET;
        client_addr.sin_port = htons(tcp_port);
        client_addr.sin_addr.s_addr = inet_addr(tcp_host);

        return sendto(serial_port, write_buffer_enc, write_len, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
    }

    return write(serial_port, write_buffer_enc, write_len);
}