///////////////////////////////////////////////
////// Project 2: File Transfer Protocol //////
// Sangjin Lee (sl5583) and Runyao Fan (rf1888)
///////////////////////////////////////////////

#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include <ctype.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include <fcntl.h>  


#define BUFMAX 1024

int port_N = 0;

int tcp(int port_num);
char* add_strings(char *string1, char *string2);
int stor(int sd, char* fn);

int main()
{
    //socket
    int server_sd = socket(AF_INET,SOCK_STREAM,0);
    if(server_sd<0)
    {
        perror("socket:");
        exit(-1);
    }
    //setsock
    int value  = 1;
    setsockopt(server_sd,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value)); //&(int){1},sizeof(int)
    struct sockaddr_in server_addr;
    socklen_t serverz = sizeof(server_addr); // max allowed size
    bzero(&server_addr,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(21);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); //INADDR_ANY, INADDR_LOOP

    //connect
    if(connect(server_sd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0)
    {
        perror("connect");
        exit(-1);
    }
    
    //accept
    char buffer[256];


    // find the port number on which the socket is listening
    getsockname(server_sd, (struct sockaddr *) &server_addr, &serverz);

    // Port N
    int port_initial_N = ntohs(server_addr.sin_port);

    // make a copy for use in while loop
    port_N = port_initial_N;

    int check = 0;
    // sent flag for STOR, RETR, LIST
    
    while(1)
    {   
        int sent = 0;
        char arr[BUFMAX] = {0};

        // receive server message first e.g. "220 Service ready for new user."
        bzero(buffer,sizeof(buffer));
        if (check == 0){int bytes = recv(server_sd,buffer,sizeof(buffer),0);}
        else{check = 0;} // if 
        printf("%s", buffer);
        bzero(buffer,sizeof(buffer));

        // FTP Prompt
        printf("ftp> ");

        fgets(buffer,sizeof(buffer),stdin);
        buffer[strcspn(buffer, "\n")] = 0;  //remove trailing newline char from buffer, fgets does not remove it
        char processedBuffer[32]; 
        strcpy(processedBuffer, buffer);

        int i = 0;
        char *p = strtok(processedBuffer, " ");
        char *input[6];

        // PRE-PROCESSING/PARSING COMMANDLINE VALUES
        while (p != NULL) {
            input[i++] = p;
            p = strtok(NULL, " ");
        }

        char* command[32];
        strcpy(command, input[0]);

        if(buffer[0]=='!')
        {
            if(strcmp(command,"!LIST")==0){system("ls");}
            else if(strcmp(command,"!PWD")==0){system("pwd");}
            else if(strcmp(command,"!CWD")==0){
                char** cwd = malloc(50*sizeof(char*));
                getcwd(*cwd,sizeof(cwd));
                strcat(cwd, input[1]);
                chdir(cwd);
                system("pwd");
                free(cwd);
            }
            else{
                printf("202 Command not implemented.\n");
            }
            check = 1;
            bzero(buffer,sizeof(buffer));   
            continue;
        }


        else if(strcmp(command,"RETR")==0 || strcmp(command,"STOR")==0 || strcmp(command,"LIST")==0)
        {
            //ANNOUNCE A NEW PORT (N+1) NUMBER 
            if(strcmp(command,"STOR")==0){
                // printf("%s\n", buffer);
                char filePath[256]; 
                strcpy(filePath, input[1]);
                printf("%s\n", filePath);

                if (access(filePath, F_OK) != 0) {
                    printf("550 No such file or directory.\n");
                    check = 1;
                    continue;
                }
                
            }

            // Port N+1
            port_N += 1;

            unsigned char p1 = port_N / 256; // higher byte of port
            unsigned char p2 = port_N % 256; // lower byte of port

            char portmsg[30] = "PORT 127,0,0,1,";
            int length = snprintf( NULL, 0, "%i", p1);
            int length2 = snprintf( NULL, 0, "%i", p2);

            // Malloc memory space for p1 and p2
            char* str = malloc( length + 1 );
            char* str2 = malloc( length2 + 1 );
            char comma[2] = ",";
            snprintf(str, length + 1, "%i", p1); // converting int to str
            snprintf(str2, length2 + 1, "%i", p2); // converting int to str

            // Concatenating strings
            char* s = add_strings(portmsg, str); 
            char* s2 = add_strings(s, comma);
            char* s3 = add_strings(s2, str2);

            // Send Port N+1 to server
            if(send(server_sd, s3, strlen(s3), 0)<0){perror("send failed");}

            free(str);
            free(str2);
            free(s);
            free(s2);
            free(s3);

            // receive server response: "200 PORT command successful"
            char arr4[256];
            bzero(arr4, sizeof(arr4));
            if(recv(server_sd, arr4,sizeof(arr4),0)<0){perror("receive failed 1");}
            printf("%s", arr4);

            // if the user is not logged in
            if (strncmp(arr4, "530", 3) == 0){
                check = 1;
                continue;
            }

            // Send RETR, STOR, or LIST commands thru the data channel
            if(send(server_sd, buffer, strlen(buffer),0)<0){perror("send");}

            // receive server response: "150 File Status okay; about to open data connection."
            char arr3[256];
            bzero(arr3, sizeof(arr3));
            if(recv(server_sd, arr3,sizeof(arr3),0)<0){perror("receive failed 2");}
            printf("%s", arr3);
            fflush(stdout);

            if (strncmp(arr3, "550", 3) == 0){
                check = 1;
                continue;
            }

            // Initiate data control and start listening
            int data_socket = tcp(port_N);
            int new_socket = accept(data_socket, (struct sockaddr *)&server_addr, &server_addr); 

            // IF THE CLIENT WANTS TO SEE THE LIST OF FILES IN SERVER DIRECTORY
            if(strcmp(buffer,"LIST")==0){
                if(send(server_sd, buffer, strlen(buffer),0)<0){perror("send");}
                char arr2[1024];
                bzero(arr2, sizeof(arr2));
                if(recv(new_socket, arr2, BUFMAX, 0)<0){perror("receive failed");}
                printf("%s", arr2);
                fflush(stdout);
                char arr1[256];
                bzero(arr1, sizeof(arr1));
                if(recv(server_sd, arr1, BUFMAX, 0)<0){perror("receive failed");}
                printf("%s\n", arr1);
                check = 1;
                printf("GOT HERE %s\n", arr1);
            }

            // IF THE CLIENT WANTS TO UPLOAD/STORE FILES IN THE SERVER DIRECTORY
            if(strcmp(command,"STOR")==0){
                if(stor(new_socket, input[1])<0){
                    printf("\n550 No such file or directory.\n");
                    bzero(buffer,sizeof(buffer));
                    check = 1;
                    continue;
                }
                char port_response[256];
                if(recv(new_socket,port_response,sizeof(port_response),0)<0){perror("receive failed 3");}
                printf("%s", port_response);
            }

            // IF THE CLIENT WANTS TO RETRIEVE/DOWNLOAD FILES FROM SERVER
            if(strcmp(command,"RETR")==0){
                int recsize = 0;
                recv(new_socket, &recsize, sizeof(recsize), 0);
                FILE *recvFile;
                char filePath[256]; 
                strcpy(filePath, input[1]);
                
                if ((recvFile = fopen(filePath, "wb")) == NULL){
                    perror("Opening");
                    continue;
                }
                char recvFileBuffer[1024];
                bzero(recvFileBuffer, sizeof(recvFileBuffer));
                int len;

                int check = 0;
                while (1) {
                    len = recv(new_socket, recvFileBuffer, sizeof(recvFileBuffer), 0);
                    if (len <= 0) {break;}

                    fwrite(recvFileBuffer, sizeof(char), len, recvFile);
                    bzero(recvFileBuffer, sizeof(recvFileBuffer));
                    check += len;
                    if (check >= recsize) {break;}
                }
                fclose(recvFile);
                close(new_socket);
            }

            sent = 1;
            bzero(buffer,sizeof(buffer));
            continue;
        }

        // IF THE CLIENT WANTS TO SEE CURRENT DIRECTORY OR CHANGE DIRECTORY
        else if(strcmp(command,"CWD")==0 || strcmp(buffer,"PWD")==0)
        {
            // SIMPLY SENDING THE COMMANDS SO THAT SERVER CAN PROCESS AND SEND BACK THE BUFFER
            if(send(server_sd, buffer, strlen(buffer),0)<0){perror("send");}
            if(recv(server_sd, arr, BUFMAX, 0)<0){perror("receive failed");}
            printf("%s\n", arr);
            check = 1;
            continue;
        }

        // CLIENT TERMINATES THE PROGRAM
        else if(strcmp(buffer,"QUIT")==0)
        {
            if(send(server_sd, buffer, strlen(buffer),0)<0){perror("send");}
            if(recv(server_sd, arr, BUFMAX, 0)<0){perror("receive failed");}
            printf("%s\n", arr);
            close(server_sd);
            break;
        }

        // USER AUTHENTICATION PART
        // WE CHANGE THE FLAG(=> sent) TO 0 SO THAT THE WHOLE BUFFER IS SENT TO THE SERVER
        else if(strcmp(command,"USER")==0 || strcmp(command,"PASS")==0){
            sent = 0;
        }

        // IF THERE HAS BEEN AN UNRECOGNIZED INPUT IN THE COMMANDLINE
        else{
            bzero(buffer,sizeof(buffer));
            printf("202 Command not implemented.\n");
            check = 1;
            continue;
        }

        // FOR ONLY WHEN THE FLAG HAS BEEN CHANGED PRIOR
        if(sent==0){
            if(send(server_sd,buffer,strlen(buffer),0)<0){perror("send");}
        }

        bzero(buffer,sizeof(buffer));   
    }

    return 0;
}



int stor(int sd, char* fn){
    if (access(fn, F_OK) == 0) {
        char filebuff[BUFMAX];
        int size;
        FILE *fp = fopen(fn, "rb");
        if(fp == NULL){
            perror("File");
            return -1;
        }
        fseek(fp, 0, SEEK_END); // seek to end of file
        size = ftell(fp); // get current file pointer
        fseek(fp, 0, SEEK_SET); // seek back to beginning of file
        send(sd, &size, sizeof(size), 0);
        int t;
        while((t = fread(filebuff, 1, sizeof(filebuff), fp))>0){
            send(sd, filebuff, t, 0);
        }
        fclose(fp);
        return 0;
    }
    else{
        return -1;    
    }
}


// Function to create a socket and initiate a TCP listen
int tcp(int port_num) {

    // socket
    int server_sd2 = socket(AF_INET,SOCK_STREAM,0);
    if(server_sd2<0)
    {
        perror("socket failed");
        exit(-1);
    }

    // create a new setsock
    int value = 1;
    setsockopt(server_sd2,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value)); //&(int){1},sizeof(int)
    struct sockaddr_in server_addr2;
    socklen_t serverz2 = sizeof(server_addr2); // max allowed size
    bzero(&server_addr2,sizeof(server_addr2));
    server_addr2.sin_family = AF_INET;
    server_addr2.sin_port = htons(port_num);
    server_addr2.sin_addr.s_addr = inet_addr("127.0.0.1"); //INADDR_ANY, INADDR_LOOP

    // bind2
    if (bind(server_sd2, (struct sockaddr*) &server_addr2, sizeof(server_addr2)) < 0) {
        perror("bind failed");
        exit(-1);
    }

    // listen2
    if (listen(server_sd2, 5) < 0) {
        perror("listen failed");
        close(server_sd2);
        exit(-1);
    }
    return server_sd2;
}


// FUNCTION TO ADD TWO CHARACTER ARRAYS TOGETHER
char* add_strings(char *string1, char *string2)
{
    char *added = malloc(strlen(string1) + strlen(string2) + 1); // +1 is for '\0'
    strcpy(added, string1);
    strcat(added, string2);
    return added;
}

