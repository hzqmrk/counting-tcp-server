#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <netinet/tcp.h>

#define SS_DEBUG (0)
#define NO_NAGLE (0)
#define QUICKACK (0)

#define TCP_PORT (5127)
#define RX_BUFF_SIZE (1525)
#define TX_BUFF_SIZE (1525)

#define SLEEP_USECS (250000)

#define END_LINK (9)

void wait_connect(int lfd, int *cfd) {

	int connected_fd = 0;
	socklen_t client_addr_length = 0;
	struct sockaddr_in client_addr;

	memset(&client_addr, 0, sizeof(client_addr));

	// blocking wait for connection from client
	client_addr_length = 0;
	connected_fd = accept(lfd, (struct sockaddr*restrict) &client_addr,
			&client_addr_length);
	if (connected_fd < 0) {
		perror("\n Error : Unable to accept connection \n");
	} else {
		*cfd = connected_fd;
	}

}

int main(int argc, char *argv[]) {
	int tmp = 0;
	int done = 0;
	int listenfd = 0;
	int connfd = 0;
	struct sockaddr_in serv_addr;
	char receiveBuff[RX_BUFF_SIZE];
	char sendBuff[TX_BUFF_SIZE];
	time_t ticks;
#if (QUICKACK ==1) || (NO_NAGLE ==1)
	int one = 1;
#endif

	// open file descriptor for IPV4 server
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd < 0) {
		perror("\n Error : Unable to get socket file descriptor \n");
		return EXIT_FAILURE;
	}

#if (NO_NAGLE ==1)
	// disable Nagle to chunk small data
	tmp = setsockopt(listenfd, SOL_TCP, TCP_NODELAY, &one, sizeof(one));
	if (tmp < 0) {
		printf("\n Error : Could not set socket option \n");
		return 1;
	}
#endif

	// clear server address context and transmit buffer
	memset(&serv_addr, 0, sizeof(serv_addr));
	memset(sendBuff, 0, TX_BUFF_SIZE);

	// bind for ANY IPV4 address on ALL interfaces
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(TCP_PORT);
	tmp = bind(listenfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	if (tmp != 0) {
		perror("\n Error : Unable to bind to requested address or port \n");
		return EXIT_FAILURE;
	}

	// and finally, LISTEN for a connection
	tmp = listen(listenfd, 10);
	if (tmp != 0) {
		perror("\n Error : Unable to listen on requested address or port \n");
		return EXIT_FAILURE;
	}

	// loop here and do work
	while (!done) {

		if (connfd == 0) {
			wait_connect(listenfd, &connfd);

#if (QUICKACK ==1)
			// don't wait to ack (must be done after connected)
			tmp = setsockopt(listenfd, SOL_TCP, TCP_QUICKACK, &one,
					sizeof(one));
			if (tmp < 0) {
				printf("\n Error : Could not set socket option \n");
				return 1;
			}
#endif

			// reply with current time
			ticks = time(NULL);
			snprintf(sendBuff, TX_BUFF_SIZE, "Started at %.24s\n",
					ctime(&ticks));
			tmp = write(connfd, sendBuff, TX_BUFF_SIZE);
			if (tmp != TX_BUFF_SIZE) {
				perror(
						"\n Error : Unable to write all bytes or write error \n");
			}
			printf("%s", sendBuff);

		}

		// clear the buffer and read some data
		memset(receiveBuff, 0, RX_BUFF_SIZE);
		tmp = read(connfd, receiveBuff, RX_BUFF_SIZE);
		if (tmp < 0) {
			perror("\n Error : Read error! \n");
		} else if (tmp > 0) {

#if (SS_DEBUG==1)
			// print out what we received
			for(int i=0;i<tmp; i++) {
				printf("%02x ", receiveBuff[i]);
			}
			printf("\n");
#else
			printf("%s", receiveBuff);
#endif
		}

		if (receiveBuff[8] == '0' + END_LINK) {

			usleep(SLEEP_USECS);

			// reply with end time
			ticks = time(NULL);
			snprintf(sendBuff, TX_BUFF_SIZE, "Ended at %.24s\n", ctime(&ticks));
			tmp = write(connfd, sendBuff, TX_BUFF_SIZE);
			if (tmp != TX_BUFF_SIZE) {
				perror(
						"\n Error : Unable to write all bytes or write error \n");
			}

			// close connection
			printf("%s", sendBuff);
			close(connfd);
			connfd = 0;
		}

		// wait a little before starting again
		usleep(SLEEP_USECS);
	}

	// close connection
	close(connfd);
	printf("Server shut down\n");

}
