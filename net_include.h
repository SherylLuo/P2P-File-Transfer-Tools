#ifndef NET_INCLUDE_
#define NET_INCLUDE_

#include <stdio.h>

#include <stdlib.h>

#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h> 
#include <netdb.h>
#include <sys/time.h>

#include <errno.h>

#define PORT	     10350

#define MAX_MESS_LEN 1400

#define WINDOW_LEN 40

typedef struct
{
	int 	index;
	char	data[MAX_MESS_LEN - 2 * sizeof(int)];
	int	 	data_size;
} packet;

typedef struct
{
	int		ack;
	int		nacks[WINDOW_LEN];
} feedback;

#endif
