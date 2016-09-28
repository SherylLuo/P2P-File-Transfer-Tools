#include "net_include.h"

#define NAME_LENGTH 80

double diffTime(struct timeval left, struct timeval right);

int main(int argc, char *argv[])
{
    struct sockaddr_in host;
    struct hostent     h_ent, *p_h_ent;

    char               host_name[NAME_LENGTH];
    char               *c;
    int                s;
    int                ret;
    char               mess_buf[MAX_MESS_LEN];
    FILE               *fr;
    int                read_num;
    struct timeval     start;
    struct timeval     end;
    struct timeval     last_time_point;
    double             difference;
    int                numOf100M = 1;
    int                size_Mbytes = 1024 * 1024;
    double             transfer_rate;
    double             data_bytes = 0;
    int                dest_file_len;

    if(argc != 3) {
        perror("t_ncp ussage: t_ncp <source_file_name> <dest_file_name>\n");
        exit(1);
    }

    s = socket(AF_INET, SOCK_STREAM, 0); /* Create a socket (TCP) */
    if (s<0) {
        perror("t_ncp: socket error");
        exit(1);
    }

    host.sin_family = AF_INET;
    host.sin_port   = htons(PORT);

    /* Server */
    printf("Enter the server name:\n");
    if ( fgets(host_name,NAME_LENGTH,stdin) == NULL ) {
        perror("t_ncp: Error reading server name.\n");
        exit(1);
    }
    c = strchr(host_name,'\n'); /* remove new line */
    if ( c ) *c = '\0';
    c = strchr(host_name,'\r'); /* remove carriage return */
    if ( c ) *c = '\0';
    printf("Your server is %s\n",host_name);

    p_h_ent = gethostbyname(host_name);
    if ( p_h_ent == NULL ) {
        printf("t_ncp: gethostbyname error.\n");
        exit(1);
    }

    /* Open file for reading */
    if((fr = fopen(argv[1], "r")) == NULL) {
        perror("t_ncp: cannot open file");
        exit(1);
    }

    memcpy( &h_ent, p_h_ent, sizeof(h_ent) );
    memcpy( &host.sin_addr, h_ent.h_addr_list[0],  sizeof(host.sin_addr) );

    ret = connect(s, (struct sockaddr *)&host, sizeof(host) ); /* Connect! */
    if( ret < 0)
    {
        perror( "t_ncp: could not connect to server"); 
        exit(1);
    }
    printf("Connected!\n");

    dest_file_len = strlen(argv[2]);

    ret = send(s, &dest_file_len, sizeof(int), 0);
    if(ret != sizeof(int)) {
        perror( "t_ncp: error");
        exit(1);
    }

    ret = send(s, argv[2], dest_file_len, 0);
    if(ret != dest_file_len) {
        perror( "t_ncp: error");
        exit(1);
    }

    gettimeofday(&start, NULL);
    last_time_point.tv_sec = start.tv_sec;
    last_time_point.tv_usec = last_time_point.tv_usec;


    for(;;)
    {    
        read_num = fread(mess_buf, 1, MAX_MESS_LEN, fr);

        if(read_num > 0) {
            ret = send(s, mess_buf, read_num, 0);
            data_bytes += read_num;
            if(data_bytes >= numOf100M * size_Mbytes * 100) {
                gettimeofday(&end, NULL);
                difference = diffTime(last_time_point, end);
                transfer_rate = ((double)data_bytes / (double)size_Mbytes) * 8 / difference;
                printf("Data successfully transferred: %fMbytes\nAverage transfer rate: %fMbits/sec\n", (double)data_bytes / (double)size_Mbytes, transfer_rate);
                numOf100M++;
                last_time_point.tv_sec = end.tv_sec;
                last_time_point.tv_usec = end.tv_usec;
            }

            if(ret != read_num) 
            {
                perror( "t_ncp: error");
                exit(1);
            }
        }

        if(read_num < MAX_MESS_LEN) {
            if(feof(fr)) {
                fclose(fr);
                printf("File transfer finished!\n");
                gettimeofday(&end, NULL);
                difference = diffTime(start, end);
                transfer_rate = ((double)data_bytes / (double)size_Mbytes) * 8 / difference;
                printf("Size of the file transferred: %fMbytes\nTransfer time: %fsec\nAverage transfer rate: %fMbits/sec\n",
                    (double)data_bytes / (double)size_Mbytes, difference, transfer_rate);
                exit(0);
            } else {
                printf("File read corrupted\n");
                fclose(fr);
                exit(1);
            }
        }
    }

    return 0;
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

    result = diff.tv_sec + (double)diff.tv_usec / (double)1000000;

    return result;
}

