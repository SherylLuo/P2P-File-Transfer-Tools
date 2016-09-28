#include "sendto_dbg.h"
#include "net_include.h"

#define NAME_LENGTH 80

double diffTime(struct timeval left, struct timeval right);

int main(int argc, char const *argv[])
{
	int                   loss_rate;
    int                   ss,sr;
    int                   num;	/* select() return value*/
    int                   bytes;
    int                   busy = 0;
    int                   busyResult;
    int                   titleResult;
    int                   sendResult;
    int                   rcv_ack = -1;
    int                   ackUpdate = 0;
    int                   nack_index;
    int                   from_ip;
    int                   i;
    int                   writeReturn;
	struct sockaddr_in    name;
    struct sockaddr_in    from_addr;
    struct timeval        timeout; 
    socklen_t             from_len;  
    feedback              *currentFeedback = NULL;
    packet                *rcvPacket = NULL;	/*packet for recvfrom()*/
    fd_set                mask;
    fd_set                dummy_mask,temp_mask;
    char                  fileTitle[NAME_LENGTH] = {'\0'};
    FILE                  *fw = NULL;
    packet                *window[WINDOW_LEN] = {NULL};
    int                   current_sender_ip;

    /*time parameters*/

    struct timeval        start;
    struct timeval        end;
    struct timeval        lastEnd;
    double        		  difference;
    int                   data_100m = 100*1024*1024;
    int                   numOf100m = 1;
    int                   receivedMb = 0;
    double				  totalSize = 0;

    int                   count = 0;

    /*Commend usage error*/

    if (argc != 2)
    {
        printf("format: rcv <loss_rate_percent>\n");
        exit(1);
    }
    loss_rate = atoi(argv[1]);

    /*initial sendto_dbg */

    sendto_dbg_init(loss_rate);

    for(int i = 0; i < WINDOW_LEN; i++) {
    	window[i] = malloc(WINDOW_LEN * sizeof(packet *));
        window[i] = NULL;
    }    
    
	sr = socket(AF_INET, SOCK_DGRAM, 0);	/* socket for receiving (udp) */
    if (sr<0) {
        perror("rcv: sr socket\n");
        exit(1);
    }

    name.sin_family = AF_INET; 
    name.sin_addr.s_addr = INADDR_ANY; 
    name.sin_port = htons(PORT);  


    if ( bind( sr, (struct sockaddr *)&name, sizeof(name) ) < 0 ) {
        perror("rcv: bind\n");
        exit(1);
    }
 	
 	ss = socket(AF_INET, SOCK_DGRAM, 0);	/* socket for sending (udp) */
    if (ss<0) {
        perror("rcv: ss socket");
        exit(1);
    }

    /*set readfile mask*/

    FD_ZERO( &mask );
    FD_ZERO( &dummy_mask );
    FD_SET( sr, &mask );
    
    currentFeedback = malloc(sizeof(feedback));

    if (currentFeedback == NULL) 
    {
       	printf("rcv feedback: malloc error.\n");
        exit(1);
    }
    
    while (1)
    {
        temp_mask = mask;
    	timeout.tv_sec = 0;
    	timeout.tv_usec = 10000;
        num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
        if (num > 0)
        {
            if ( FD_ISSET( sr, &temp_mask) )  
            {
                from_len = sizeof(from_addr);
                rcvPacket = malloc(sizeof(packet));
                if (rcvPacket == NULL) {
                    printf("rcv: malloc error.\n");
                    exit(1);
                }
                bytes = recvfrom( sr, rcvPacket, sizeof(packet), 0,  
                          (struct sockaddr *)&from_addr, &from_len );
                
                if (bytes == -1) continue;

                from_addr.sin_port = htons(PORT);
                from_ip = from_addr.sin_addr.s_addr;

                /*to ensure to send ack = -5 to ncp, make sure ncp could close */
                if (rcvPacket->index == -4 && busy == 0)
            	{
            		currentFeedback->ack = -5;
                    do
                    {
                      sendResult = sendto_dbg(ss, (char*)currentFeedback, sizeof(feedback), 0, (struct sockaddr *)&from_addr, sizeof(from_addr)); 
                    } while (sendResult == -1);
            	}

                if (rcvPacket->index == -1 && busy == 0)   
                {
                    currentFeedback->ack = -2; 
                    do
                    {
                        busyResult = sendto_dbg(ss, (char*)currentFeedback, sizeof(feedback), 0, (struct sockaddr *)&from_addr, sizeof(from_addr)); 
                    } while (busyResult == -1);
                    busy = 1;
                    // printf("busy = 1\n");
                    current_sender_ip = from_ip;
                    continue;
                } else if (rcvPacket->index == -1 && busy == 1) /*check if the rcv is busy now*/
                {
                    currentFeedback->ack = -3;
                    do
                    {
                        busyResult = sendto_dbg(ss, (char*)currentFeedback, sizeof(feedback), 0, (struct sockaddr *)&from_addr, sizeof(from_addr)); 
                    } while (busyResult == -1);
                    continue;
                }

                if(busy == 1 && from_ip == current_sender_ip) 
                {
                    // printf( "Received from (%d.%d.%d.%d)\n", 
                    //             (htonl(from_ip) & 0xff000000)>>24,
                    //             (htonl(from_ip) & 0x00ff0000)>>16,
                    //             (htonl(from_ip) & 0x0000ff00)>>8,
                    //             (htonl(from_ip) & 0x000000ff)
                    //             );
                               
                    /*set the destination file name and create or open the file*/

                    if (rcvPacket->index == -2) 
                    {
                        strcpy(fileTitle, (char *)rcvPacket->data);
                        currentFeedback->ack = -4;
                        
                        do
                        {
                            titleResult = sendto_dbg(ss, (char*)currentFeedback, sizeof(feedback), 0, (struct sockaddr *)&from_addr, sizeof(from_addr)); 
                        } while (titleResult == -1);

 						/*clock*/

                    	gettimeofday(&start, NULL);
                    	end.tv_sec = start.tv_sec;
                    	end.tv_usec = start.tv_usec;

                        if((fw = fopen(fileTitle, "wb")) == NULL) 
                        {
                            perror("rcv: fopen\n");
                            exit(1);   
                        }   
                    } 
                    else if (rcvPacket->index >= 0)
                    {
                        count = 0;

                        /*check if the index out of current window range */

                        if (rcvPacket->index > rcv_ack + WINDOW_LEN || rcvPacket->index <= rcv_ack)  continue; 

                        /* Save packet to buffer, waiting for write operation */

                        window[rcvPacket->index % WINDOW_LEN] = rcvPacket;
                        
                    }

                    /* after receiving a window, send a feedback*/

                    else if (rcvPacket->index == -3)
                    {
                        count++;

                        if(count > 1) {
                            sendto_dbg(ss, (char*)currentFeedback, sizeof(feedback), 0, (struct sockaddr *)&from_addr, sizeof(from_addr));

                            // printf("-3 miss test send: %d, %d, %d, %d \n", currentFeedback->ack, currentFeedback->nacks[0], currentFeedback->nacks[1], currentFeedback->nacks[2] ); 

                            continue;
                        }
                       
                        /*Update ack*/

                        ackUpdate = rcv_ack;
                        for (i = rcv_ack + 1; i <= rcv_ack + WINDOW_LEN; i++)
                        {
                            if (window[i % WINDOW_LEN] == NULL) break;
                            ackUpdate++;
                        }
                        currentFeedback->ack = ackUpdate;

                        /*Set nacks[]*/

                        for (i = 0; i < WINDOW_LEN; i++) 
                        {
                            currentFeedback->nacks[i] = -1;
                        }

                        nack_index = 0;
                        for (i = rcv_ack + 1; i <= rcv_ack + WINDOW_LEN; i++) 
                        {
                            if (window[i % WINDOW_LEN] == NULL) {
                                currentFeedback->nacks[nack_index] = i;
                                nack_index++;
                            }
                        }

                        /*send feedback*/

                        do
                        {
                            sendResult = sendto_dbg(ss, (char*)currentFeedback, sizeof(feedback), 0, (struct sockaddr *)&from_addr, sizeof(from_addr));
                        } while (sendResult == -1);
                        // printf("send feedback: %d, %d, %d, %d\n", currentFeedback->ack, currentFeedback->nacks[0], currentFeedback->nacks[1], currentFeedback->nacks[2] );                       
                    	// printf("buffer first index; %d, %d, %d\n", window[0]->index, window[1]->index, window[3]->index  );
                        // printf("ackUpdate and rcv_ack%d, %d\n", ackUpdate, rcv_ack);
                        for (i = rcv_ack + 1; i <= ackUpdate; i++) 
                        {

                        	writeReturn = fwrite(window[i % WINDOW_LEN]->data, 1, window[i % WINDOW_LEN]->data_size, fw);

                            if (writeReturn != window[i % WINDOW_LEN]->data_size)
                            {
                                printf("fwrite error.\n");
                                exit(1);
                            }

                            // printf("write index %d\n", window[i % WINDOW_LEN]->index);

                       		window[i % WINDOW_LEN] = NULL;

                       		/*clock*/

                       		receivedMb = receivedMb + writeReturn;

	                        if (receivedMb >= data_100m * numOf100m)
	                        {
	                            printf("%d * 100mb file received. \n", numOf100m);
	                            lastEnd.tv_sec = end.tv_sec;
	                            lastEnd.tv_usec = end.tv_usec;
	                            gettimeofday(&end, NULL);
	                            difference = diffTime(lastEnd, end);
	                            printf("Last 100MB data transfer speed: %f  Mbits/sec\n", (double)(100*8)/difference );
	                            numOf100m++;
	                        }
                        }
                        
                        rcv_ack = ackUpdate;
                    }

                    /*Transmission complete*/

                    else if (rcvPacket->index == -4)
                    {
                    	/*clock*/

                    	totalSize = (double)receivedMb/(double)(1024 * 1024);
                        printf("Size of file transfered: %f Mbytes\n", totalSize);
                        gettimeofday(&end, NULL);
                        difference = diffTime(start, end);
                        printf("Time of Transmission: %f sec.\n", difference);
                        printf("Average transfer speed %f Mbits/sec\n", totalSize * 8 / difference ); 
                        fclose(fw);
                        fw = NULL;
                        
                        currentFeedback->ack = -5;
                        do
                        {
                            sendResult = sendto_dbg(ss, (char*)currentFeedback, sizeof(feedback), 0, (struct sockaddr *)&from_addr, sizeof(from_addr)); 
                        } while (sendResult == -1);

                        busy = 0;

                        
                        printf("Transmission completed.\n");

                        /*reset*/

                        numOf100m = 1;
                        receivedMb = 0;
                        count = 0;
                       	rcv_ack = -1;
                        ackUpdate = -1;
                        for (i = 0; i < WINDOW_LEN; i++) 
                        {
                            currentFeedback->nacks[i] = -1;
                        }
                         for(int i = 0; i < WINDOW_LEN; i++) {
					    	
					        window[i] = NULL;
					    }
                    }
                }

             }
         
        }else 
        {
            if(busy == 0) continue;

            sendto_dbg(ss, (char*)currentFeedback, sizeof(feedback), 0, (struct sockaddr *)&from_addr, sizeof(from_addr));
            // printf("timeout send: %d, %d, %d, %d \n", currentFeedback->ack, currentFeedback->nacks[0], currentFeedback->nacks[1], currentFeedback->nacks[2] ); 
            
        }

    }
}
double diffTime(struct timeval left, struct timeval right)
{
    struct timeval diff;
    double result;

    diff.tv_sec  = right.tv_sec - left.tv_sec;
    diff.tv_usec = right.tv_usec - left.tv_usec;

    if (diff.tv_usec < 0) {
        diff.tv_usec += 1000000;
        diff.tv_sec--;
    }

    if (diff.tv_sec < 0) {
        printf("WARNING: diffTime has negative result, returning 0!\n");
        diff.tv_sec = diff.tv_usec = 0;
    }

    result = (double)diff.tv_sec + (double)diff.tv_usec / (double)1000000;

    return result;
}
