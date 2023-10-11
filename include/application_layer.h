// Application layer protocol header.
// NOTE: This file must not be changed.

#ifndef _APPLICATION_LAYER_H_
#define _APPLICATION_LAYER_H_

#define FILE_NAME 1
#define FILE_SIZE 0

#define CONTROL_DATA 1
#define CONTROL_START 2
#define CONTROL_END 3

// Application layer main function.
// Arguments:
//   serialPort: Serial port name (e.g., /dev/ttyS0).
//   role: Application role {"tx", "rx"}.
//   baudrate: Baudrate of the serial port.
//   nTries: Maximum number of frame retries.
//   timeout: Frame timeout.
//   filename: Name of the file to send / receive.
void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename);

int transmitter_application_layer(const char *filename);

int send_file(FILE *file);

int send_control_frame(const char *filename, int file_size,
                       int app_layer_control);

#endif // _APPLICATION_LAYER_H_
