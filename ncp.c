#include "net_include.h"
#include "sendto_dbg.h"

#define NAME_LENGTH 80
#define MAX_WAIT_TIME 60

int gethostname(char*,size_t);

void PromptForHostName( char *my_name, char *comp_name, size_t max_len ); 

void SendFile(char *source_file_name, char *dest_file_name, char *comp_name);

double diffTime(struct timeval left, struct timeval right);

int ReadFileToBuf();

FILE                  *fr;
packet                sender_buf[WINDOW_LEN];
int                   sender_ack = -1; /* keep track of the last packet in the current window */
int                   receiver_ack = -1; /* the most recent ack received from receiver */
int                   index_counter = -1; /* the index of current packet reading from file */
int                   last_packet = -1; /* index of the last packet */
int                   last_packet_size = 0; 


int main(int argc, char *argv[])
{
    int                   loss_rate_percent;
    char                  source_file_name[NAME_LENGTH] = {'\0'};
    char                  dest_file_name[NAME_LENGTH] = {'\0'};
    char                  *at;
    char                  comp_name[NAME_LENGTH] = {'\0'};

    if(argc != 4) {
        perror("ncp ussage: ncp <loss_rate_percent> <source_file_name> <dest_file_name>@<comp_name>\n");
        exit(1);
    }

    loss_rate_percent = (int)strtol(argv[1], (char **)NULL, 10);
    strcpy(source_file_name, argv[2]); 
    at = strchr(argv[3], '@');

    if(at == NULL) {
        perror("ncp ussage: ncp <loss_rate_percent> <source_file_name> <dest_file_name>@<comp_name>\n");
        exit(1);
    }

    *at = '\0';
    strcpy(dest_file_name, argv[3]);
    strcpy(comp_name, ++at);
    //strcat(host_name, ".cs.jhu.edu\0");

    sendto_dbg_init(loss_rate_percent);
    SendFile(source_file_name, dest_file_name, comp_name);
}

void SendFile(char *source_file_name, char *dest_file_name, char *comp_name) {
    struct sockaddr_in    name;
    struct sockaddr_in    send_addr;
    struct sockaddr_in    from_addr;
    socklen_t             from_len;
    struct hostent        h_ent;
    struct hostent        *p_h_ent;
    char                  my_name[NAME_LENGTH] = {'\0'};
    int                   host_num;
    int                   from_ip;
    int                   ss, sr;
    fd_set                mask;
    fd_set                dummy_mask,temp_mask;
    int                   num;
    struct timeval        timeout;
    int                   wait_counter = 0;
    feedback              *feedback_received;
    int                   connect = 0;
    int                   termination = 0;
    int                   dest_sent = 0;
    int                   busy = 0;
    struct timeval        start;
    struct timeval        end;
    struct timeval        last_time_point;
    double                difference;
    int                   numOf100M = 1;
    int                   size_Mbytes = 1024 * 1024;
    double                transfer_rate;
    double                data_Mbytes;

    /* Open file for reading */
    if((fr = fopen(source_file_name, "r")) == NULL) {
        perror("ncp: source file error");
        exit(1);
    }

    /* socket for receiving (udp) */
    sr = socket(AF_INET, SOCK_DGRAM, 0);  
    if (sr<0) {
        perror("ncp: socket");
        exit(1);
    }

    name.sin_family = AF_INET; 
    name.sin_addr.s_addr = INADDR_ANY; 
    name.sin_port = htons(PORT);

    if ( bind( sr, (struct sockaddr *)&name, sizeof(name) ) < 0 ) {
        perror("ncp: bind");
        exit(1);
    }

    ss = socket(AF_INET, SOCK_DGRAM, 0); /* socket for sending (udp) */
    if (ss<0) {
        perror("ncp: socket");
        exit(1);
    }
    
    PromptForHostName(my_name,comp_name,NAME_LENGTH);
    
    p_h_ent = gethostbyname(comp_name);
    if ( p_h_ent == NULL ) {
        printf("ncp: gethostbyname error.\n");
        exit(1);
    }

    memcpy( &h_ent, p_h_ent, sizeof(h_ent));
    memcpy( &host_num, h_ent.h_addr_list[0], sizeof(host_num) );

    send_addr.sin_family = AF_INET;
    send_addr.sin_addr.s_addr = host_num; 
    send_addr.sin_port = htons(PORT);

    FD_ZERO( &mask );
    FD_ZERO( &dummy_mask );
    FD_SET( sr, &mask );

    for(;;)
    {
        temp_mask = mask;
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000;
        num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
        if (num > 0) {
            feedback_received = malloc(sizeof(feedback));
            if(feedback_received == NULL) {
                perror("ncp: malloc error");
                exit(1);
            }

            if ( FD_ISSET( sr, &temp_mask) ) {

                from_len = sizeof(from_addr);
                recvfrom( sr, feedback_received, sizeof(feedback), 0,  
                          (struct sockaddr *)&from_addr, &from_len );
                from_ip = from_addr.sin_addr.s_addr;

                if(from_ip == send_addr.sin_addr.s_addr) {
                    // printf("received ack: %d, received nacks[0]: %d\n", feedback_received->ack, feedback_received->nacks[0]);
                    if(feedback_received->ack == -2) {
                        /* Receiver is able to accept file, send dest_file_name to receiver */
                        printf( "Start transfer to (%d.%d.%d.%d): \n", 
                                    (htonl(from_ip) & 0xff000000)>>24,
                                    (htonl(from_ip) & 0x00ff0000)>>16,
                                    (htonl(from_ip) & 0x0000ff00)>>8,
                                    (htonl(from_ip) & 0x000000ff) );

                        packet *dest_name = malloc(sizeof(packet));
                        if(dest_name == NULL) {
                            perror("ncp: malloc error");
                            exit(1);
                        }

                        dest_name->index = -2;
                        strcpy(dest_name->data, dest_file_name);
                        sendto_dbg( ss, (char *)dest_name, sizeof(packet), 0, 
                            (struct sockaddr *)&send_addr, sizeof(send_addr) );
                        free(dest_name);
                        connect = 1;
                    } else if(feedback_received->ack == -4 || feedback_received->ack >= -1) {
                        dest_sent = 1;
                        if(feedback_received->ack == -4 && index_counter == -1) {
                            gettimeofday(&start, NULL);
                            last_time_point.tv_sec = start.tv_sec;
                            last_time_point.tv_usec = start.tv_usec;
                        }

                        receiver_ack = feedback_received->ack;
                        if(feedback_received->ack == -4)
                            receiver_ack = -1;

                        if((receiver_ack + 1) * (MAX_MESS_LEN - 2 * sizeof(int)) >= numOf100M * size_Mbytes * 100) {
                            gettimeofday(&end, NULL);
                            difference = diffTime(last_time_point, end);
                            data_Mbytes = (double)((receiver_ack + 1) * (MAX_MESS_LEN - 2 * sizeof(int))) / (double)size_Mbytes;
                            transfer_rate = data_Mbytes * 8 / difference;
                            printf("Data successfully transferred: %f\nAverage transfer rate: %f\n", data_Mbytes, transfer_rate);
                            numOf100M++;
                        }

                        /* Deal with rcv timeout resend mechanism */
                        if((index_counter != -1 && feedback_received->ack == -4) || receiver_ack < index_counter - WINDOW_LEN) {
                            packet *remind = malloc(sizeof(packet));
                            if(remind == NULL) {
                                perror("ncp: malloc error\n");
                                exit(1);
                            }
                            remind->index = -3;
                            sendto_dbg( ss, (char *)remind, sizeof(packet), 0, 
                                (struct sockaddr *)&send_addr, sizeof(send_addr) );
                            free(remind);
                        }

                        if(termination == 1) {
                            if(receiver_ack == last_packet) {
                                /* file transfer finished */
                                packet *term_mess = malloc(sizeof(packet));
                                if(term_mess == NULL) {
                                    perror("ncp: malloc error\n");
                                    exit(1);
                                }
                                term_mess->index = -4;
                                sendto_dbg( ss, (char *)term_mess, sizeof(packet), 0, 
                                    (struct sockaddr *)&send_addr, sizeof(send_addr) );
                                free(term_mess);

                                gettimeofday(&end, NULL);
                                difference = diffTime(start, end);
                                data_Mbytes = (double)(last_packet * (MAX_MESS_LEN - 2 * sizeof(int)) + (last_packet_size - 2 * sizeof(int))) / (double)size_Mbytes;
                                transfer_rate = data_Mbytes * 8 / difference;
                                printf("Size of the file transferred: %fMbytes\nTransfer time: %fsec\nAverage transfer rate: %fMbits/sec\n", data_Mbytes, difference, transfer_rate);
                            } else if(receiver_ack < last_packet && receiver_ack > last_packet - WINDOW_LEN && feedback_received->nacks[0] == -1) {
                                /* deal with nothing to send condition for the last window */
                                for(int i = receiver_ack + 1; i <= last_packet; i++) {
                                    // printf("sending index term: %d, %d\n", sender_buf[i % WINDOW_LEN].index, sender_buf[i % WINDOW_LEN].data_size);
                                    sendto_dbg( ss, (char *)&sender_buf[i % WINDOW_LEN], sizeof(packet), 0, 
                                        (struct sockaddr *)&send_addr, sizeof(send_addr) );
                                }
                            } else {
                                /* Resend necks */
                                int i = 0;
                                while(feedback_received->nacks[i] != -1 && i < WINDOW_LEN) {
                                    if(feedback_received->nacks[i] > receiver_ack && feedback_received->nacks[i] <= receiver_ack + WINDOW_LEN) {
                                        if(feedback_received->nacks[i] > last_packet) 
                                            break;
                                        // printf("Sending nacks, index:%d\n", feedback_received->nacks[i]);
                                        sendto_dbg( ss, (char *)&sender_buf[feedback_received->nacks[i] % WINDOW_LEN], sizeof(packet), 0, 
                                            (struct sockaddr *)&send_addr, sizeof(send_addr) );
                                    }
                                    i++;
                                 }
                            }
                        } else {
                            int status = ReadFileToBuf();
                            if(status < 0) {
                                perror("ncp: read file error\n");
                                exit(1);
                            } else  {
                                int i = 0;
                                while(feedback_received->nacks[i] != -1 && i < WINDOW_LEN) {
                                    if(feedback_received->nacks[i] > receiver_ack && feedback_received->nacks[i] <= receiver_ack + WINDOW_LEN) {
                                        // printf("Sending nacks, index:%d\n", feedback_received->nacks[i]);
                                        sendto_dbg( ss, (char *)&sender_buf[feedback_received->nacks[i] % WINDOW_LEN], sizeof(packet), 0, 
                                            (struct sockaddr *)&send_addr, sizeof(send_addr) );
                                    }
                                    i++;
                                 }
                                if (status > 0) { /* Reach EOF */
                                    for(int i = sender_ack + 1; i <= last_packet; i++) {
                                        // printf("sending index term: %d, %d\n", sender_buf[i % WINDOW_LEN].index, sender_buf[i % WINDOW_LEN].data_size);
                                        sendto_dbg( ss, (char *)&sender_buf[i % WINDOW_LEN], sizeof(packet), 0, 
                                            (struct sockaddr *)&send_addr, sizeof(send_addr) );
                                    }
                                    packet *remind = malloc(sizeof(packet));
                                    if(remind == NULL) {
                                        perror("ncp: malloc error\n");
                                        exit(1);
                                    }
                                    remind->index = -3;
                                    sendto_dbg( ss, (char *)remind, sizeof(packet), 0, 
                                        (struct sockaddr *)&send_addr, sizeof(send_addr) );
                                    free(remind);
                                    termination = 1;
                                } else { /* Transfer in process */                                 
                                    for(int i = sender_ack + 1; i <= index_counter; i++) {
                                        // printf("sending index: %d, %d\n", sender_buf[i % WINDOW_LEN].index, sender_buf[i % WINDOW_LEN].data_size);
                                        sendto_dbg( ss, (char *)&sender_buf[i % WINDOW_LEN], sizeof(packet), 0, 
                                            (struct sockaddr *)&send_addr, sizeof(send_addr) );
                                    }
                                    packet *remind = malloc(sizeof(packet));
                                    if(remind == NULL) {
                                        perror("ncp: malloc error\n");
                                        exit(1);
                                    }
                                    remind->index = -3;
                                    sendto_dbg( ss, (char *)remind, sizeof(packet), 0, 
                                        (struct sockaddr *)&send_addr, sizeof(send_addr) );
                                    free(remind);
                                }
                            }
                        }

                        
                    } else if(feedback_received->ack== -3) {
                        if(wait_counter == 0) {
                            printf("Receiver is busy\n");
                            busy = 1;
                        } else {
                            wait_counter++;
                            if(wait_counter > MAX_WAIT_TIME) {
                                printf("Time out. File transfer failed\n");
                                exit(1);
                            }
                        }
                    } 
                    else if(feedback_received->ack == -5) {
                        printf("File transfer finished\n");
                        free(feedback_received);
                        exit(0);
                    }
                }
            } 
        } else {
            if(!connect) {
                packet *try_to_connect = malloc(sizeof(packet));
                if(try_to_connect == NULL) {
                    perror("ncp: malloc error");
                    exit(1);
                }

                try_to_connect->index = -1;
                strcpy(try_to_connect->data, dest_file_name);
                sendto_dbg( ss, (char *)try_to_connect, sizeof(packet), 0, 
                    (struct sockaddr *)&send_addr, sizeof(send_addr) );
                free(try_to_connect);
            } else if(busy == 1) {
                wait_counter++;
                if(wait_counter > MAX_WAIT_TIME) {
                    printf("Time out. File transfer failed\n");
                    exit(1);
                }
            } else if (!dest_sent) {
                packet *dest_name = malloc(sizeof(packet));
                if(dest_name == NULL) {
                    perror("ncp: malloc error");
                    exit(1);
                }

                dest_name->index = -2;
                strcpy(dest_name->data, dest_file_name);
                sendto_dbg( ss, (char *)dest_name, sizeof(packet), 0, 
                    (struct sockaddr *)&send_addr, sizeof(send_addr) );
                free(dest_name);
            } else if(!termination || (termination == 1 && receiver_ack != last_packet)) {
                /* if the sender didn't hear feedback from receiver until timeout */
                packet *remind = malloc(sizeof(packet));
                if(remind == NULL) {
                    perror("ncp: malloc error\n");
                    exit(1);
                }
                remind->index = -3;
                // printf("Sending remind\n");
                sendto_dbg( ss, (char *)remind, sizeof(packet), 0, 
                    (struct sockaddr *)&send_addr, sizeof(send_addr) );
                free(remind);
            } else {
                packet *term_mess = malloc(sizeof(packet));
                if(term_mess == NULL) {
                    perror("ncp: malloc error\n");
                    exit(1);
                }
                term_mess->index = -4;
                sendto_dbg( ss, (char *)term_mess, sizeof(packet), 0, 
                    (struct sockaddr *)&send_addr, sizeof(send_addr) );
                free(term_mess);
            }
        }
    }
}

/* Read from file to buffer. 
Return 0 means file transfer going on; return 1 means reaching EOF; return -1 means error. */
int ReadFileToBuf() {
    sender_ack = index_counter;
    packet          *current_packet;

    current_packet = malloc(sizeof(packet));
    if(current_packet == NULL) {
        perror("ncp: malloc error");
        exit(1);
    }

    for(int i = sender_ack + 1; i <= receiver_ack + WINDOW_LEN; i++) {
        
        int read_num = fread(current_packet->data, 1, MAX_MESS_LEN - 2 * sizeof(int), fr);

        if(read_num > 0) {
            index_counter++;
            current_packet->index = index_counter;
            current_packet->data_size = read_num;
            sender_buf[i % WINDOW_LEN] = *current_packet;
        }

        if(read_num < MAX_MESS_LEN - 2 * sizeof(int)) {
            if(feof(fr)) {
                last_packet = index_counter;
                last_packet_size = read_num + 2 * sizeof(int);
                fclose(fr);
                return 1;
            } else {
                printf("File read corrupted\n");
                fclose(fr);
                return -1;
            }
        }
    }

    return 0;
}

void PromptForHostName( char *my_name, char *comp_name, size_t max_len ) {
    gethostname(my_name, max_len);
    printf("My host name is %s.\n", my_name);

    if ( comp_name == NULL ) {
        perror("ncp ussage: ncp <loss_rate_percent> <source_file_name> <dest_file_name>@<comp_name>\n");
        exit(1);
    }
    printf( "Sending from %s to %s.\n", my_name, comp_name );
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

    result = diff.tv_sec + (double)diff.tv_usec / 1000000;

    return result;
}
