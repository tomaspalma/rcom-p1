// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

#define DELIMETER 0x7E
#define A_SENDER 0x03
#define A_RECEIVER 0x01
#define C_SET 0x03
#define C_UA 0x07
#define C_NUMBER0 0x00
#define C_NUMBER1 0x40
#define C_RR_BASE 0x05
#define C_RR(n) ((n << 7) | C_RR_BASE)
#define C_REJ_BASE 0X01
#define C_REJ(n) ((n << 7) | C_REJ_BASE)

#define SETUP_COMM_MSG_SIZE 5

#define HEADER_START_SIZE 4
#define HEADER_END_SIZE 2

#define ESCAPE 0x7D
#define ESCAPED_DELIMITER 0x5e
#define ESCAPED_ESCAPE 0x5d

typedef enum {
  LlTx,
  LlRx,
} LinkLayerRole;

typedef struct {
  char serialPort[50];
  LinkLayerRole role;
  int baudRate;
  int nRetransmissions;
  int timeout;
} LinkLayer;

// SIZE of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 1000

// MISC
#define FALSE 0
#define TRUE 1

// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(LinkLayer connectionParameters);

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(const unsigned char *buf, int bufSize);

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(unsigned char *packet);

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console
// on close. Return "1" on success or "-1" on error.
int llclose(int showStatistics);

#endif // _LINK_LAYER_H_
