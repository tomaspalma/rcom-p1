// Link layer protocol implementation
#include <stdbool.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "link_layer.h"
#include "serial_port.h"

// MISC

int fd;
bool resend = FALSE;

enum OPEN_STATES { START_RCV, FLAG_RCV, A_RCV, C_RCV, BCC_RCV };

void resend_timer(int signal) {
  resend = TRUE;
  alarm(TIMEOUT);
}

int send_supervised_set() {
  unsigned char set[SETUP_COMM_MSG_SIZE] = { DELIMETER, A_SENDER, C_SET, A_SENDER ^ C_SET, DELIMETER };
    
  if(write(fd, set, SETUP_COMM_MSG_SIZE) != 0) {
    printf("Failed writing SET message to serial port\n");
    return -1;
  }

  return 0;
}

void recv_set_state_machine() {
  enum OPEN_STATES set_state = START_RCV;
  unsigned char recv_set = 0;
  unsigned char stop = FALSE;
  while (stop == FALSE) {
    read(fd, &recv_set, 1);

    printf("Received: %x\n", recv_set);

    if (set_state == START_RCV) {
      if (recv_set == DELIMETER)
        set_state = FLAG_RCV;
      else
        set_state = START_RCV;
    } else if (set_state == FLAG_RCV) {
      if (recv_set == DELIMETER)
        set_state = FLAG_RCV;
      else if (recv_set == A_SENDER)
        set_state = A_RCV;
      else
        set_state = START_RCV;
    } else if (set_state == A_RCV) {
      if (recv_set == DELIMETER)
        set_state = FLAG_RCV;
      else if (recv_set == C_SET)
        set_state = C_RCV;
      else
        set_state = START_RCV;
    } else if (set_state == C_RCV) {
      if (recv_set == DELIMETER)
        set_state = FLAG_RCV;
      else if (recv_set == (A_SENDER ^ C_SET))
        set_state = BCC_RCV;
      else
        set_state = START_RCV;
    } else if (set_state == BCC_RCV) {
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
  while (stop == FALSE) {
    if(resend) {
      ua_state = START_RCV;
      if(send_supervised_set() != 0) return;
    }

    read(fd, &recv_ua, 1);

    printf("Received: %x\n", recv_ua);

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
        stop = TRUE;
      }
      else
        ua_state = START_RCV;
    }
  }
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {
  fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
  if(fd != 0) return -1;

  setup_port(fd);

  if(connectionParameters.role == LlRx) {
    recv_set_state_machine();
    
    unsigned char ua[SETUP_COMM_MSG_SIZE] = { DELIMETER, A_RECEIVER, C_UA, A_RECEIVER ^ C_UA, DELIMETER};
    write(fd, ua, SETUP_COMM_MSG_SIZE);
  } else if(connectionParameters.role == LlTx) {
    (void)signal(SIGALRM, resend_timer);
    if(send_supervised_set() != 0) return -1;
    alarm(connectionParameters.timeout);
    recv_ua_state_machine();
  } else {
    return -1;
  }

  return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize) {
  // TODO

  return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
  // TODO

  return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) {
  // TODO

  return 1;
}
