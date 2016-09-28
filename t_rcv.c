#include "net_include.h"
double diffTime(struct timeval left, struct timeval right);


int main(int argc, char **argv)
{
    struct sockaddr_in name;
    int                s;
    fd_set             mask;
    int                recv_s[10];
    int                valid[10];
    fd_set             dummy_mask,temp_mask;
    int                i,j,num;
    int                mess_len;
    char               mess_buf[MAX_MESS_LEN];
    long               on=1;
    
    /*File*/
    FILE               *fw; 
    int                bytes;
    int 			   title = 0;
    int 			   titleSize;
    int 			   receivedSize;

    /*time parameters*/

    struct timeval        start;
    struct timeval        end;
    struct timeval        lastEnd;
    double        		  difference;
    int                   data_100m = 100*1024*1024;
    int                   numOf100m = 1;
    int                   receivedMb = 0;
    double				  totalSize = 0;
    


    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s<0) {
        perror("Net_server: socket");
        exit(1);
    }

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
    {
        perror("Net_server: setsockopt error \n");
        exit(1);
    }

    name.sin_family = AF_INET;
    name.sin_addr.s_addr = INADDR_ANY;
    name.sin_port = htons(PORT);

    if ( bind( s, (struct sockaddr *)&name, sizeof(name) ) < 0 ) {
        perror("Net_server: bind");
        exit(1);
    }

    if (listen(s, 4) < 0) {
        perror("Net_server: listen");
        exit(1);
    }

    i = 0;
    FD_ZERO(&mask);
    FD_ZERO(&dummy_mask);
    FD_SET(s,&mask);
    for(;;)
    {
        temp_mask = mask;
        num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, NULL);
        if (num > 0) 
        {
            if ( FD_ISSET(s,&temp_mask) ) 
            {
                recv_s[i] = accept(s, 0, 0);
                FD_SET(recv_s[i], &mask);
                valid[i] = 1;
                i++;
            }
            for(j=0; j<i ; j++)
            {   

            	if (valid[j])
                if ( FD_ISSET(recv_s[j],&temp_mask) ) 
                {

                    if (title == 0) 
                    {
                        
                       	bytes = recv(recv_s[j],&titleSize,sizeof(int),0);
                        receivedMb += bytes;
                        receivedSize = titleSize;
                        title = 1;

                    }
                    else 
                    {
                        if( (mess_len = recv(recv_s[j],mess_buf,receivedSize,0)) > 0) 
                        {
                            if (title == 1) 
                            {
                                mess_buf[receivedSize] = '\0';
                                
                                if((fw = fopen(mess_buf, "w")) == NULL) 
                                {
                                  perror("fopen");
                                  exit(1);
                                }

                                /*clock*/

		                    	gettimeofday(&start, NULL);
		                    	end.tv_sec = start.tv_sec;
		                    	end.tv_usec = start.tv_usec;
                               
                                receivedMb += receivedSize;
                                receivedSize = MAX_MESS_LEN;
                                title = -1;
                            } else                          
                            {
                                bytes = fwrite(mess_buf, 1, mess_len, fw);
                                receivedMb += bytes;
                                
                                /*clock*/
                       		
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
                        }else
                        {
                        	/*clock*/

                            printf("FINISHED RECEIVING FILE\n");
                            totalSize = (double)receivedMb/(double)(1024 * 1024);
	                        printf("Size of file transfered: %f Mbytes\n", totalSize);
	                        gettimeofday(&end, NULL);
	                        difference = diffTime(start, end);
	                        printf("Time of Transmission: %f sec.\n", difference);
	                        printf("Average transfer speed %f Mbits/sec\n", totalSize * 8 / difference );

                            FD_CLR(recv_s[j], &mask);
                            close(recv_s[j]);
                            valid[j] = 0;
                            fclose(fw);
                            exit(0);
                        }
                    }
                }
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

    result = (double)diff.tv_sec + (double)diff.tv_usec / (double)1000000;

    return result;
}
