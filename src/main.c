#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <asm-generic/ioctls.h>
#include <time.h>
#include <math.h>

#define NUM_CHILDREN 5
#define BUFFER_SIZE 128
#define READ_END 0
#define WRITE_END 1
#define MAX_WAIT 2
#define RUNTIME 30
#define MAX_INPUT 64
#define MAX_MSG 1000


int fd[NUM_CHILDREN][2]; // file descriptors for the pipe
fd_set inputs;

void normal_child(int);
void weird_child(int);

int main() {
    char read_msg[BUFFER_SIZE];

    // Create the pipe.
    for (size_t i = 0; i < NUM_CHILDREN; i++)
    {
        if (pipe(fd[i]) == -1) {
            fprintf(stderr,"pipe() failed");
            return 1;
        }
    }

    // Spawn five child processes
    for (size_t i = 0; i < NUM_CHILDREN; i++)
    {
        pid_t pid = fork();
        
        // Child process
        if (!pid) {
            if (i < NUM_CHILDREN - 1)
                normal_child((int) i);
            else
                weird_child((int) i);
            break;
        }
        // Error creating child process
        else if (pid < 0) {
            printf("Error: fork() failed to create new child process\n");
            exit(1);
        }
    }

    // Close all unused ends of file descriptors
    for (size_t i = 0; i < NUM_CHILDREN; i++)
    {
        close(fd[i][WRITE_END]);
    }

    char output_stream[MAX_MSG][BUFFER_SIZE];
    int msg_idx = 0;

    // Get starting timestamp
    struct timeval tv;
    struct timeval begin;
    gettimeofday(&begin, NULL);

    // Parent process: read messages from child processes
    for (;;)
    {
        char buffer[BUFFER_SIZE];
        int result, nread;
        
        // Add file descriptors to set
        int maxfd = 0;
        FD_ZERO(&inputs);
        for (size_t i = 0; i < NUM_CHILDREN; i++)
        {
            FD_SET(fd[i][READ_END], &inputs);
            if (fd[i][READ_END] > maxfd) {
                maxfd = fd[i][READ_END];
            }
        }
        
        // Use select() to monitor all file descriptors
        result = select(maxfd + 1, &inputs, (fd_set *) 0, (fd_set *) 0, NULL);

        // Check the results.
        // No input: the program loops again.
        // Got input: print what was typed, then terminate.
        // Error: terminate.
        switch(result) {
            case 0: { // No input
                break;
            }

            case -1: { // error
                break;
            }

            // If, during the wait, we have some action on the file descriptor,
            // we read the input on the file descriptor and echo it whenever an 
            // <end of line> character is received, until that input is Ctrl-D.
            default: { // Got input
                for (size_t i = 0; i < NUM_CHILDREN; i++)
                {
                    if (FD_ISSET(fd[i][READ_END], &inputs)) {
                        ioctl(fd[i][READ_END], FIONREAD, &nread); // read # of bytes available on
                        // the read end of file descriptor and set nread arg to that value
                    
                        if (nread == 0) {
                            goto AFTER_LOOP;
                        }

                        // Get timestamp
                        gettimeofday(&tv, NULL);
                        int sec = ((int) tv.tv_sec) - ((int) begin.tv_sec);
                        double msec = fabs(((double) tv.tv_usec) - ((double) begin.tv_usec)) / 1000;

                        char sec_str[5];
                        sprintf(sec_str, "%d", sec);
                        char msec_str[15];
                        sprintf(msec_str, "%f", msec);
                        char delimiter[5] = ":";
                        char timestamp[25];
                        strcat(timestamp, sec_str);
                        strcat(timestamp, delimiter);
                        strcat(timestamp, msec_str);
                        strcat(timestamp, " | ");

                        char output_msg[BUFFER_SIZE];
                        strcat(output_msg, timestamp);

                        // Read from buffer and print to output file
                        nread = read(fd[i][READ_END], buffer, nread);
                        buffer[nread] = 0;
                        strcat(output_msg, buffer);
                        strcat(output_msg, "\n");
                        strcpy(output_stream[msg_idx++], output_msg);
                        // printf("%s", output_stream[msg_idx]);
                        // fprintf(f, "%s", output_stream[msg_idx++]);
                        
                        // write(fileno(f), output_stream, strlen(output_stream[msg_idx++]));

                        // Reset output message
                        memset(output_msg, 0, strlen(output_msg));

                         // Reset timestamp
                        memset(timestamp, 0, strlen(timestamp));
                    }
                }
                break;
            }
        }
    }

    AFTER_LOOP:
    // Open output file
    char* output_file = "/home/shsieh/coen383/project6/output.txt";
    FILE* f = fopen(output_file, "w");
    if (!f) {
        printf("Error: cannot open output file\n");
        exit(1);
    }

    // Write to output file
    for (size_t i = 0; i < msg_idx; i++)
    {
        fprintf(f, "%s", output_stream[i]);
        sleep(0.1);
    }

    // Close output file
    fclose(f);

    // Close all read ends
    for (size_t i = 0; i < NUM_CHILDREN; i++)
    {
        close(fd[i][READ_END]);
    }
    
    return 0;
}

void normal_child(int child_idx) {
    // Close unused ends of the pipe
    for (size_t i = 0; i < NUM_CHILDREN; i++)
    {
        close(fd[i][READ_END]);
        if (i != child_idx) {
            close(fd[i][WRITE_END]);
        }
    }

    // Initalize segments of the write message
    char child_num[5];
    sprintf(child_num, "%d", child_idx + 1);
    char child_msg[10] = " Child ";
    strcat(child_msg, child_num);

    int msg_num = 1;

    clock_t start;
    start = clock();
    float time_elapsed = 0;
    float runtime = (float) RUNTIME;
    int total_wait = 0;
    struct timeval tv;
    struct timeval begin;
    gettimeofday(&begin, NULL);
    
    // Continue running until time hits 30 seconds
    while (time_elapsed < runtime)
    {
        // Find out how long to wait: 0, 1, or 2 seconds
        int wait_time = rand() % (MAX_WAIT + 1);
        total_wait += wait_time;

        // Sleep the process
        sleep(wait_time);
        
        // Concatenate segments of the write message
        char msg_num_str[5];
        sprintf(msg_num_str, "%d", msg_num++);
        char msg_str[15] = " message ";
        strcat(msg_str, msg_num_str);
        
        // Get timestamp
        gettimeofday(&tv, NULL);
        int sec = ((int) tv.tv_sec) - ((int) begin.tv_sec);
        double msec = fabs(((double) tv.tv_usec) - ((double) begin.tv_usec)) / 1000;

        char sec_str[5];
        sprintf(sec_str, "%d", sec);
        char msec_str[15];
        sprintf(msec_str, "%f", msec);
        char delimiter[5] = ":";
        char timestamp[25];
        strcat(timestamp, sec_str);
        strcat(timestamp, delimiter);
        strcat(timestamp, msec_str);
        strcat(timestamp, delimiter);

        char write_msg[BUFFER_SIZE];
        strcat(write_msg, timestamp);
        strcat(write_msg, child_msg);
        strcat(write_msg, msg_str);
        
        // Send message over pipe
        write(fd[child_idx][WRITE_END], write_msg, strlen(write_msg) + 1);

        // Reset written message
        memset(write_msg, 0, strlen(write_msg));

        // Reset timestamp
        memset(timestamp, 0, strlen(timestamp));
        
        // Update running time
        time_elapsed = ((float) total_wait) + ((float)(clock() - start)) / CLOCKS_PER_SEC;
    }
    
    // Close the write end of the file descriptor and return
    close(fd[child_idx][WRITE_END]);
}

void weird_child(int child_idx) {
    // Close unused ends of the pipe
    for (size_t i = 0; i < NUM_CHILDREN; i++)
    {
        close(fd[i][READ_END]);
        if (i != child_idx) {
            close(fd[i][WRITE_END]);
        }
    }

    // Initalize segments of the write message
    char child_num[5];
    sprintf(child_num, "%d", child_idx + 1);
    char child_msg[10] = " Child ";
    strcat(child_msg, child_num);

    clock_t start;
    start = clock();
    float time_elapsed = 0;
    float runtime = (float) RUNTIME;
    struct timeval tv;
    struct timeval begin;
    gettimeofday(&begin, NULL);
    
    // Continue running until time hits 30 seconds
    while (time_elapsed < runtime)
    {   
        // Get timestamp
        gettimeofday(&tv, NULL);
        int sec = ((int) tv.tv_sec) - ((int) begin.tv_sec);
        double msec = fabs(((double) tv.tv_usec) - ((double) begin.tv_usec)) / 1000;

        // Prompt user for message
        char user_input[MAX_INPUT];
        printf("Enter message: ");
        scanf("%[^\n]", user_input);
        char newline;
        scanf("%c", &newline);

        char sec_str[5];
        sprintf(sec_str, "%d", sec);
        char msec_str[15];
        sprintf(msec_str, "%f", msec);
        char delimiter[5] = ":";
        char timestamp[25];
        strcat(timestamp, sec_str);
        strcat(timestamp, delimiter);
        strcat(timestamp, msec_str);
        strcat(timestamp, delimiter);

        char write_msg[BUFFER_SIZE];
        strcat(write_msg, timestamp);
        strcat(write_msg, child_msg);
        strcat(write_msg, delimiter);
        strcat(write_msg, " ");
        strcat(write_msg, user_input);
        
        // Send message over pipe
        write(fd[child_idx][WRITE_END], write_msg, strlen(write_msg) + 1);

        // Reset written message
        memset(write_msg, 0, strlen(write_msg));

        // Reset timestamp
        memset(timestamp, 0, strlen(timestamp));
        
        // Update running time
        time_elapsed = ((float)(clock() - start)) / CLOCKS_PER_SEC;
    }
    
    // Close the write end of the file descriptor and return
    close(fd[child_idx][WRITE_END]);
}
