// Link layer protocol implementation
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "link_layer.h"
#include "serial_port.h"

extern struct termios oldtio;

enum OPEN_STATES { START_RCV, FLAG_RCV, A_RCV, C_RCV, BCC_RCV };

enum READ_STATES { START, READ_FLAG, READ_A, READ_C, READ_DATA, ESC_RCV };

int transfered_file_size = 0;
int number_of_sender_retries = 0;
int number_of_errors_detected = 0;
clock_t start_time;
clock_t end_time;

int fd;
bool resend = FALSE;
int send_tries = 0;

int ns = 0;
int nr = 1;

void resend_timer(int signal) {
  fprintf("Hello", stdout);
  resend = TRUE;
  send_tries++;
  alarm(TIMEOUT);
}

int send_supervised_frame(int address, int control_byte) {
  unsigned char frame[SUPERVISION_FRAME_SIZE] = {
      DELIMETER, address, control_byte, address ^ control_byte, DELIMETER};

  // TODO Refactor this so we exit after alarm tries reach limit
  if (write(fd, frame, SUPERVISION_FRAME_SIZE) == -1) {
    printf("Failed writing SET message to serial port\n");
    return -1;
  }

  return 0;
}

void recv_supervision_state_machine(unsigned char address_byte,
                                    unsigned char control_byte) {
  enum OPEN_STATES set_state = START_RCV;
  unsigned char recv_set = 0;
  unsigned char stop = FALSE;
  while (stop == FALSE) {
    while (read(fd, &recv_set, 1) == 0)
      ;

    printf("Received: %x\n", recv_set);

    if (set_state == START_RCV) {
      if (recv_set == DELIMETER)
        set_state = FLAG_RCV;
      else
        set_state = START_RCV;
    } else if (set_state == FLAG_RCV) {
      if (recv_set == DELIMETER)
        set_state = FLAG_RCV;
      else if (recv_set == address_byte)
        set_state = A_RCV;
      else
        set_state = START_RCV;
    } else if (set_state == A_RCV) {
      if (recv_set == DELIMETER)
        set_state = FLAG_RCV;
      else if (recv_set == control_byte)
        set_state = C_RCV;
      else
        set_state = START_RCV;
    } else if (set_state == C_RCV) {
      if (recv_set == DELIMETER)
        set_state = FLAG_RCV;
      else if (recv_set == (address_byte ^ control_byte))
        set_state = BCC_RCV;
      else
        set_state = START_RCV;
    } else if (set_state == BCC_RCV) {
      printf("What the actual fuck: %d", recv_set == DELIMETER);
      if (recv_set == DELIMETER)
        stop = TRUE;
      else
        set_state = START_RCV;
    }
  }
}

void recv_ua_state_machine() {
  enum OPEN_STATES ua_state = START_RCV;
  unsigned char recv_ua = 0;
  unsigned char stop = FALSE;
  int goback = 0;
  while (stop == FALSE) {
    if (resend) {
      if (send_tries >= 3) {
        return;
      }

      ua_state = START_RCV;
      resend = false;
      if (send_supervised_frame(A_SENDER, C_SET) != 0)
        return;
    }

    while (read(fd, &recv_ua, 1) == 0) {
      if (resend) {
        goback = 1;
        break;
      }
    }
    if (goback) {
      goback = false;
      continue;
    }

    // printf("Received: %x\n", recv_ua);

    if (ua_state == START_RCV) {
      if (recv_ua == DELIMETER)
        ua_state = FLAG_RCV;
      else
        ua_state = START_RCV;
    } else if (ua_state == FLAG_RCV) {
      if (recv_ua == DELIMETER)
        ua_state = FLAG_RCV;
      else if (recv_ua == A_RECEIVER)
        ua_state = A_RCV;
      else
        ua_state = START_RCV;
    } else if (ua_state == A_RCV) {
      if (recv_ua == DELIMETER)
        ua_state = FLAG_RCV;
      else if (recv_ua == C_UA)
        ua_state = C_RCV;
      else
        ua_state = START_RCV;
    } else if (ua_state == C_RCV) {
      if (recv_ua == DELIMETER)
        ua_state = FLAG_RCV;
      else if (recv_ua == (A_RECEIVER ^ C_UA))
        ua_state = BCC_RCV;
      else
        ua_state = START_RCV;
    } else if (ua_state == BCC_RCV) {
      if (recv_ua == DELIMETER) {
        alarm(0);
        send_tries = 0;
        stop = TRUE;
      } else
        ua_state = START_RCV;
    }
  }
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {
  fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
  if (fd == -1) {
    printf("Fd\n");
    return -1;
  }

  setup_port(fd);

  if (connectionParameters.role == LlRx) {
    recv_supervision_state_machine(A_SENDER, C_SET);

    unsigned char ua[SETUP_COMM_MSG_SIZE] = {DELIMETER, A_RECEIVER, C_UA,
                                             A_RECEIVER ^ C_UA, DELIMETER};
    write(fd, ua, SETUP_COMM_MSG_SIZE);
  } else if (connectionParameters.role == LlTx) {
    (void)signal(SIGALRM, resend_timer);
    printf("Started\n");
    alarm(connectionParameters.timeout);
    if (send_supervised_frame(A_SENDER, C_SET) != 0)
      return -1;
    recv_ua_state_machine();
  } else {
    return -1;
  }

  return 0;
}

int assemble_start_frame(unsigned char *frame) {
  if (frame == NULL) {
    return -1;
  }

  frame[0] = DELIMETER;
  frame[1] = A_SENDER;
  frame[2] = ns << 6;
  frame[3] = frame[1] ^ frame[2];

  return 0;
}

int assemble_end_frame(int offset, unsigned char *frame, unsigned char bcc2) {
  if (frame == NULL) {
    return -1;
  }

  printf("OFFSET IS: %d\n", offset);

  frame[offset] = bcc2;
  frame[offset + 1] = DELIMETER;

  return 0;
}

int stop_and_wait(unsigned char *frame, int size) {
  (void)signal(SIGALRM, resend_timer);
  alarm(TIMEOUT);
  int ntries = 0;
  bool received = FALSE;
  send_tries = 0;
  resend = FALSE;

  int goback = false;

  // for (int i = 0; i < size; i++) {
  //   printf("Sent: %x\n", frame[i]);
  // }

  if (write(fd, frame, size) == -1) {
    perror(strerror(errno));
    return -1;
  }

  printf("Wrote frame\n");

  bool cancel = 0;

  enum OPEN_STATES current_state = START_RCV;
  unsigned char buf = 0;
  unsigned char confirmation_byte = 0;
  while (!received) {
    if (resend) {
      printf("Send tries is: %d\n", send_tries);
      if (send_tries > N_TRIES) {
        printf("Send tries excedded\n");
        return -1;
      }

      number_of_sender_retries++;

      alarm(TIMEOUT);
      current_state = START_RCV;
      resend = false;
      if (write(fd, frame, size) == -1) {
        printf("Error writing frame in the link layer!");
        return -1;
      }
      printf("----\n");
      for (int i = 0; i < size; i++) {
        printf("0x%x\n", frame[i]);
      }
      printf("----\n");
      printf("Wrote frame again!\n");
    }

    printf("Reading: \n");
    while (read(fd, &buf, 1) == 0) {
      if (resend == true) {
        goback = true;
        break;
      }
    }
    if (goback) {
      goback = false;
      continue;
    }
    printf("Read Byte: %d\n", buf);

    if (current_state == START_RCV) { // 0
      if (buf == DELIMETER)
        current_state = FLAG_RCV;
    } else if (current_state == FLAG_RCV) { // 1
      if (buf == DELIMETER)
        current_state = FLAG_RCV;
      else if (buf == A_RECEIVER)
        current_state = A_RCV;
      else
        current_state = START_RCV;
    } else if (current_state == A_RCV) { // 2
      if (buf == DELIMETER)
        current_state = FLAG_RCV;
      else if (buf == C_RR(0) || buf == C_RR(1) || buf == C_REJ(0) ||
               buf == C_REJ(1)) {
        confirmation_byte = buf;
        current_state = C_RCV;
      } else
        current_state = START_RCV;
    } else if (current_state == C_RCV) {
      printf("Buf is: %d\n", buf);
      if (buf == DELIMETER)
        current_state = FLAG_RCV;
      else if (buf == (A_RECEIVER ^ C_RR(nr)) ||
               buf == (A_RECEIVER ^ C_REJ(nr))) {
        current_state = BCC_RCV;
      } else {
        printf("Está a entrar aqui???\n");
        current_state = START_RCV;
      }
    } else if (current_state == BCC_RCV) {
      // if (cancel) {
      // current_state = START_RCV;
      // }
      if (buf == DELIMETER) {
        send_tries = 0;

        if (confirmation_byte == C_RR(1) || confirmation_byte == C_RR(0)) {
          if (confirmation_byte != C_RR(nr)) {
            current_state = START_RCV;
            resend = false;
            alarm(0);
            continue;
          }

          printf("CONFIRMATION BYTE?\n");

          alarm(0);
          received = TRUE;
          ns = (ns + 1) % 2;
          nr = (nr + 1) % 2;
        } else if (confirmation_byte == C_REJ(0) ||
                   confirmation_byte == C_REJ(1)) {
          resend = true;
          printf("REJECTION!\n");
          alarm(TIMEOUT);
        }
      } else {
        current_state = START_RCV;
      }
    }
  }

  return 0;
}

/**
 * @brief
 *
 * @param buf Buffer of bytes of the data
 * @param bufSize Number of bytes of data
 * @return int
 */

int s = 0;

int llwrite(const unsigned char *buf, int bufSize) {
  // sleep(2);
  unsigned char *frame = (unsigned char *)malloc(HEADER_START_SIZE +
                                                 bufSize * 2 + HEADER_END_SIZE);
  assemble_start_frame(frame);

  unsigned char bcc2 = 0;
  int byte_stuffing_offset = 0;
  int frame_index = HEADER_START_SIZE - 1;
  for (int i = 0; i < bufSize; i++) {
    if (buf[i] == DELIMETER) {
      frame[++frame_index] = ESCAPE;
      frame[++frame_index] = ESCAPED_DELIMITER;
    } else if (buf[i] == ESCAPE) {
      frame[++frame_index] = ESCAPE;
      frame[++frame_index] = ESCAPED_ESCAPE;
    } else {
      frame[++frame_index] = buf[i];
    }

    // printf("Foda-se: %x\n", buf[i]);

    bcc2 ^= buf[i];
  }

  assemble_end_frame(frame_index + 1, frame, bcc2);
  frame_index += 2;
  printf("Please be delimeter uWu %x\n", frame[frame_index]);

  if (stop_and_wait(frame, frame_index + 1) == -1) {
    return -1;
  }

  return frame_index + 1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
  unsigned char byte = 0;
  unsigned char frame_number = 2;
  enum READ_STATES read_state = START;
  unsigned char stop = FALSE;

  // enum READ_STATES { START, READ_FLAG, READ_A, READ_C, READ_DATA, ESC_RCV };

  int fds = 0;
  int i = 0;
  while (!stop) {
    while (read(fd, &byte, 1) == 0) {
    };
    printf("Received: 0x%x\n and state is %d", byte, read_state);
    // if (byte == DELIMETER) {
    //   printf("Received delimiter and state is :%d\n", read_state);
    // } else {
    //   printf("Received other byte\n");
    // }

    int cancel = 0;

    if (read_state == START) {
      // printf("Entered START\n");
      if (byte == DELIMETER) {
        // printf("oh meu\n");
        read_state = READ_FLAG;
      } else
        read_state = START;
    } else if (read_state == READ_FLAG) {
      // printf("Entered READ_FLAG\n");
      if (byte == A_SENDER) {
        printf("Went to read A\n");
        read_state = READ_A;
      } else if (byte != DELIMETER) {
        printf("went back: 0x%x\n", byte);
        read_state = START;
      }
    } else if (read_state == READ_A) {
      // printf("Entered READ_A\n");
      if (byte == C_NUMBER0 || byte == C_NUMBER1) {
        if (byte != ns) {
          cancel = 1;
        }
        read_state = READ_C;
        frame_number = byte;
      } else if (byte == DELIMETER)
        read_state = READ_FLAG;
      else
        read_state = START;
    } else if (read_state == READ_C) {
      // printf("Entered READ_C\n");
      if (byte == (A_SENDER ^ frame_number))
        read_state = READ_DATA;
      else if (byte == DELIMETER)
        read_state = READ_FLAG;
      else
        read_state = START;
    } else if (read_state == READ_DATA) {
      // printf("Entered READ_DATA\n");
      if (byte == ESCAPE)
        read_state = ESC_RCV;
      else if (byte == DELIMETER) {
        if (cancel) {
          printf("Joined cancel culture\n");
          continue;
        }
        unsigned char recv_bcc2 = packet[i - 1];
        packet[--i] = 0;

        unsigned char calc_bcc2 = packet[0];
        for (unsigned int j = 1; j < i; j++) {
          calc_bcc2 ^= packet[j];
        }

        printf("Recv  bcc is: %d\n", recv_bcc2);
        printf("Calc  bcc is: %d\n", calc_bcc2);

        if (recv_bcc2 == calc_bcc2) {
          stop = TRUE;
          printf("A receiver: %d\n", A_RECEIVER);
          unsigned char acceptance_frame[5] = {DELIMETER, A_RECEIVER, C_RR(nr),
                                               A_RECEIVER ^ C_RR(nr),
                                               DELIMETER};
          printf("Accepted\n");
          write(fd, acceptance_frame, 5);
          nr = (nr + 1) % 2;
          ns = (ns + 1) % 2;

          printf("I is: %d\n", i);
          fds++;
          return i;
        } else if (recv_bcc2 != calc_bcc2) {
          unsigned char rejection_frame[5] = {DELIMETER, A_RECEIVER, C_REJ(nr),
                                              A_RECEIVER & C_REJ(nr),
                                              DELIMETER};

          number_of_errors_detected++;

          printf("Rejection!");

          write(fd, rejection_frame, 5);
          fds++;
          read_state = START;
        }
      } else if (!cancel) {
        packet[i] = byte;
        i++;
      }
    } else if (read_state == ESC_RCV) {
      read_state = READ_DATA;
      if (byte == ESCAPED_DELIMITER)
        packet[i++] = DELIMETER;
      else if (byte == ESCAPED_ESCAPE)
        packet[i++] = ESCAPE;
      // else {
      //   packet[i++] = ESCAPE;
      //   packet[i++] = byte;
      // }
    }

    cancel = 0;
  }

  return 0;
}

int llclose_transmitter() {
  printf("Llcose transmitter\n");
  if (send_supervised_frame(A_SENDER, DISC) == -1) {
    return -1;
  }

  printf("Wrote DISC\n");

  recv_supervision_state_machine(A_RECEIVER, DISC);

  printf("Received disc from receiver\n");

  if (send_supervised_frame(A_SENDER, C_UA) == -1) {
    return -1;
  }

  printf("closed!\n");
}

int llclose_receiver() {
  unsigned char buf;
  int stop = 0;

  recv_supervision_state_machine(A_SENDER, DISC);

  printf("receved disc\n");

  if (send_supervised_frame(A_RECEIVER, DISC) == -1) {
    return -1;
  }

  recv_supervision_state_machine(A_SENDER, C_UA);

  printf("closed\n");
}

int show_statistics(LinkLayerRole role) {
  /*
   *
* number_of_sender_retries = 0;
int number_of_errors_detected = 0;
int start_time = 0;
   * */

  printf("------------------\n");
  printf("STATISTICS:\n");
  printf("Size of file in bytes: %d\n", transfered_file_size);
  if (role == LlTx) {
    printf("Number of sender retries: %d\n", number_of_sender_retries);
  }
  if (role == LlRx) {
    printf("Number of errors detected %d\n", number_of_errors_detected);
  }
  printf("Time taken was: %f\n",
         ((double)(end_time - start_time)) / CLOCKS_PER_SEC);
  printf("------------------\n");
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(LinkLayerRole role, int showStatistics) {
  if (role == LlTx) {
    if (llclose_transmitter() == -1) {
      return -1;
    }
  } else {
    if (llclose_receiver() == -1) {
      return -1;
    }
  }

  if (showStatistics) {
    show_statistics(role);
  }

  if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  close(fd);

  return 0;
}
