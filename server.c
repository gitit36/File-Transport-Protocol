#include <stdio.h> 
#include <string.h>   
#include <stdlib.h> 
#include <errno.h> 
#include <unistd.h>   
#include <arpa/inet.h>   
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <sys/time.h> 
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h> 


int main() {
	struct credential {
		char usrname[20];
		char password[20];
	};

	struct credential credentials[10];
	// // store the file descriptor linked to a client's socket
	// int clientFd[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
	int userNameProvided[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	int loggedIn[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	FILE *fp = fopen("users.txt", "r");

	// count number of users whose info is in the txt
	int usrCnt = 0;
	int c;
	while (!feof(fp)) {
		c = fgetc(fp);
		if (c == '\n') {
			usrCnt++;
		}
	}

    // go back to start of file to read credentials
	fseek(fp, 0, SEEK_SET);

	for (int i = 0; i < usrCnt; i++) {
		fscanf(fp, "%s %s", credentials[i].usrname, credentials[i].password);
	}

	// have server listening on port 21
	//socket
	int server_sd = socket(AF_INET,SOCK_STREAM,0);
	printf("Server fd = %d \n",server_sd);
	if (server_sd < 0) {
		perror("socket:");
		exit(-1);
	}

	//setsock
	int value  = 1;
	setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(21);
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); //INADDR_ANY, INADDR_LOOP

	//bind
	if (bind(server_sd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		perror("bind failed");
		exit(-1);
	}

	//listen
	if (listen(server_sd, 5) < 0) {
		perror("listen failed");
		close(server_sd);
		exit(-1);
	}

	fd_set full_fdset;
	fd_set read_fdset;
	FD_ZERO(&full_fdset);

	int max_fd = server_sd;

	FD_SET(server_sd,&full_fdset);

	while (1) {
		FD_COPY(&full_fdset,&read_fdset);

		if (select(max_fd + 1, &read_fdset, NULL, NULL, NULL) < 0) {
			perror("select");
			exit (-1);
		}

		for (int fd = 3; fd <= max_fd; fd++) {
			if (FD_ISSET(fd, &read_fdset)) {
				if (fd == server_sd) {
					int client_sd = accept(server_sd, 0, 0);
					printf("Client Connected fd = %d \n", client_sd);
					char readyMsg[] = "220 Service ready for new user.\n";
					send(client_sd, readyMsg, strlen(readyMsg), 0);
					FD_SET(client_sd, &full_fdset);
					
					if (client_sd > max_fd)	
						max_fd = client_sd;
				} else {
					char buffer[256];
					bzero(buffer,sizeof(buffer));
					int bytes = recv(fd,buffer,sizeof(buffer),0);
					if (bytes == 0) {  //client has closed the connection
						printf("connection closed from client side \n");
						close(fd);
						FD_CLR(fd, &full_fdset);
						if (fd == max_fd) {
							for (int i = max_fd; i >= 3; i--) {
								if (FD_ISSET(i,&full_fdset)) {
									max_fd = i;
									break;
								}
							}
						}
					}
					
					if (strncmp(buffer, "USER", 4) == 0) {
						char usrname[20];
						int userExists = 0;
						strcpy(usrname, buffer + 5);
						// check username against loaded username

						for (int i = 0; i < sizeof(credentials)/sizeof(credentials[0]); i++) {
							
							if (strcmp(credentials[i].usrname, usrname) == 0) {
								if (strcmp(usrname, "") == 0) {
									break;
								}
								char usrMsg[] = "331 Username OK, need password.\n";
								printf("%d: 331 Username OK, need password.\n", fd);
								send(fd, usrMsg, strlen(usrMsg), 0);
								userExists = 1;
								userNameProvided[i] = fd;
								break;
							}
						}

						if (!userExists) {
							char usrMsg[] = "332 Need account for login.\n";
							printf("%d: 332 Need account for login.\n", fd);
							send(fd, usrMsg, strlen(usrMsg), 0);
						}

					} else if (strncmp(buffer, "PASS", 4) == 0) {
						char password[20];
						int userLoggedIn = 0;
						// printf("%s \n", buffer);
						strcpy(password, buffer + 5);
						// check username against loaded username
						for (int i = 0; i < sizeof(credentials)/sizeof(credentials[0]); i++) {
							
							if (strcmp(credentials[i].password, password) == 0) {
								// if the password is in the record, we check if the password
								// is indeed for the client with the current file descriptor
								if (userNameProvided[i] == fd) {
									char passMsg[] = "230 User logged in, proceed.\n";
									printf("%d: 230 User logged in, proceed.\n", fd);
									send(fd, passMsg, strlen(passMsg), 0);
									userLoggedIn = 1;
									loggedIn[i] = fd;
									break;
								} else {
									continue;
								}
							}
						}

						if (!userLoggedIn) {
							char passMsg[] = "530 Not logged in.\n";
							printf("%d: 530 Not logged in.\n", fd);
							send(fd, passMsg, strlen(passMsg), 0);
						}
					} else if (strncmp(buffer, "PORT", 4) == 0) {
						// if (fork() == 0) {
							
							// check if the user is logged in
							// CHANGE THIS BACK TO 0 WHEN DONE
							int userLoggedIn = 0;
							// printf("%s\n", buffer);
							
							for (int i = 0; i < 10; i++) {
								if (loggedIn[i] == fd) {
									userLoggedIn = 1;
									break;
								}
							}

							if (!userLoggedIn) {
								char passMsg[] = "530 Not logged in.\n";
								printf("%d: 530 Not logged in.\n", fd);
								send(fd, passMsg, strlen(passMsg), 0);
							} else {
								char portMsg[] = "200 PORT command successful.\n";
								printf("%d: 200 PORT command successful.\n", fd);
								send(fd, portMsg, strlen(portMsg), 0);

								char processedBuffer[32]; 
								strcpy(processedBuffer, buffer + 5);

							    int i = 0;
							    char *p = strtok(processedBuffer, ",");
							    char *array[6];

							    while (p != NULL) {
							        array[i++] = p;
							        p = strtok(NULL, ",");
							    }

							    // getting client ip address
							    char client_ip[256]; 
							    
							    strcpy(client_ip, array[0]);
							    strcat(client_ip, ".");
							    strcat(client_ip, array[1]);
							    strcat(client_ip, ".");
							    strcat(client_ip, array[2]);
							    strcat(client_ip, ".");
							    strcat(client_ip, array[3]);

							    // printf("%s\n", client_ip);
							    int p1, p2;

								sscanf(array[4], "%d", &p1);
								sscanf(array[5], "%d", &p2);

		                        int ftp_client_port = p1 * 256 + p2;

		                        printf("PORT:%d\n", ftp_client_port);

		                        // get the STOR/RETR/LIST command from server
								bzero(buffer,sizeof(buffer));
								int bytes = recv(fd,buffer,sizeof(buffer),0);
								// if command is RETR, check that the file exists before returning a 150
								if (strncmp(buffer, "RETR", 4) == 0) {
									// printf("%s\n", buffer);
									char filePath[256]; 
									strcpy(filePath, buffer + 5);
									printf("%s\n", filePath);

									if (access(filePath, F_OK) == 0) {
									    // file exists
									    char fileStatusMsg[] = "150 File status okay; about to open data connection.\n";
										printf("%d: 150 File status okay; about to open data connection.\n", fd);
										send(fd, fileStatusMsg, strlen(fileStatusMsg), 0);
									} else {
									    // file doesn't exist
									    char fileStatusMsg[] = "550 No such file or directory.\n";
										printf("%d: 550 No such file or directory.\n", fd);
										send(fd, fileStatusMsg, strlen(fileStatusMsg), 0);
										continue;
									}

								} else {
									char fileStatusMsg[] = "150 File status okay; about to open data connection.\n";
									printf("%d: 150 File status okay; about to open data connection.\n", fd);
									send(fd, fileStatusMsg, strlen(fileStatusMsg), 0);
								}
							// if (fork() == 0) {

		                        //socket
								int ftp_client_sd = socket(AF_INET,SOCK_STREAM,0);
								if (ftp_client_sd < 0) {
									perror("socket:");
									exit(-1);
								}
								//setsock
								setsockopt(ftp_client_sd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)); 
								struct sockaddr_in ftp_client_addr;
								bzero(&ftp_client_addr, sizeof(ftp_client_addr));
								ftp_client_addr.sin_family = AF_INET;
								ftp_client_addr.sin_port = htons(ftp_client_port);
								ftp_client_addr.sin_addr.s_addr = inet_addr(client_ip); 


								//Following code will specify the local port number (20) which will be 
								//used for the connection to the remote machine (e.g. ftp client). 
								struct sockaddr_in my_addr;
								bzero(&my_addr, sizeof(my_addr));
								my_addr.sin_family = AF_INET;
								my_addr.sin_port = htons(20);
								bind(ftp_client_sd, (struct sockaddr *)(&my_addr), sizeof(my_addr));

								if (connect(ftp_client_sd, (struct sockaddr*)&ftp_client_addr, sizeof(ftp_client_addr)) < 0) {
							        perror("connect");
							        exit(-1);
							    }

							   



							    // receiving file if command is STOR
							    if (strncmp(buffer, "RETR", 4) == 0) {
							        char filebuff[1024];
								    int size;
								    char filePath[256]; 
									strcpy(filePath, buffer + 5);
									printf("%s\n", filePath);
								    FILE *fp = fopen(filePath, "rb");
								    if(fp == NULL){
								        perror("File");
								        continue;
								        // return;
								    }
								    fseek(fp, 0, SEEK_END); // seek to end of file
								    size = ftell(fp); // get current file pointer
								    //int converted_number = htonl(size);
								    // printf("FILE SIZE: %d\n", size);
								    fseek(fp, 0, SEEK_SET); // seek back to beginning of file
								    send(ftp_client_sd, &size, sizeof(size), 0);
								    int t;
								    while((t = fread(filebuff, 1, sizeof(filebuff), fp))>0){
								        // printf("%s\n", filebuff);
								        send(ftp_client_sd, filebuff, t, 0);
								    }
								    fclose(fp);
								    char completeMsg[] = "226 Transfer completed.\n";
									printf("%d: 226 Transfer completed.\n", fd);
									send(fd, completeMsg, strlen(completeMsg), 0);
								 
							    } else if (strncmp(buffer, "STOR", 4) == 0) {
							    	int recsize = 0;
							    	recv(ftp_client_sd, &recsize, sizeof(recsize), 0);
							    	// printf("SIZE::: %d\n", recsize);
							    	FILE *recvFile;
							    	char filePath[256]; 
									strcpy(filePath, buffer + 5);
									printf("%s\n", filePath);
									
									if ((recvFile = fopen(filePath, "wb")) == NULL){
								        perror("Opening");
								        continue;
								    }
								    char recvFileBuffer[1024];
								    bzero(recvFileBuffer, sizeof(recvFileBuffer));
								    int len;
								    // len = recv(ftp_client_sd, recvFileBuffer, sizeof(recvFileBuffer), 0);
								    // printf("LMAO%s\n", recvFileBuffer);
								    int check = 0;
								    while (1) {
								    	len = recv(ftp_client_sd, recvFileBuffer, sizeof(recvFileBuffer), 0);
								    	
								    	if (len <= 0) {break;}

								    	fwrite(recvFileBuffer, sizeof(char), len, recvFile);
								    	bzero(recvFileBuffer, sizeof(recvFileBuffer));
								    	check += len;
								    	if (check >= recsize) {break;}
								    }
								 

								    fclose(recvFile);
	        						close(ftp_client_sd);
	        						char completeMsg[] = "226 Transfer completed.\n";
									printf("%d: 226 Transfer completed.\n", fd);
									send(fd, completeMsg, strlen(completeMsg), 0);

							    } else if (strncmp(buffer, "LIST", 4) == 0) {
							    	char listBuffer[1024];
							    	bzero(listBuffer, sizeof(listBuffer));
							    	DIR *d;
									struct dirent *dir;
									char* tmp = "\n";
									d = opendir(getcwd(NULL,0));
									if (d) {
									  while ((dir = readdir(d)) != NULL) {
									    
									    strcat(listBuffer, dir->d_name);
									    strcat(listBuffer, tmp);
									  }
									  closedir(d);
									}
									// printf("%s\n", listBuffer);
									send(ftp_client_sd, listBuffer, 1024, 0);
									bzero(listBuffer, sizeof(listBuffer));
									char completeMsg[] = "226 Transfer completed.\n";
									printf("%d: 226 Transfer completed.\n", fd);
									send(fd, completeMsg, strlen(completeMsg), 0);
									
							    }
							    close(ftp_client_sd);
							}

						// 	return 0;
						// }
						
					} else if (strncmp(buffer, "PWD", 3) == 0) {
						int userLoggedIn = 0;
						printf("%s\n", buffer);
						
						for (int i = 0; i < 10; i++) {
							if (loggedIn[i] == fd) {
								userLoggedIn = 1;
								break;
							}
						}

						if (!userLoggedIn) {
							char passMsg[] = "530 Not logged in.\n";
							printf("%d: 530 Not logged in.\n", fd);
							send(fd, passMsg, strlen(passMsg), 0);
						} else {
							char currDir[256];
							strcpy(currDir, "257 pathname.\n");
							strcat(currDir, getcwd(NULL,0));
							
							printf("%s\n", currDir);
							send(fd, currDir, strlen(currDir), 0);
						}
						
            			// printf("%s\n", getcwd(NULL,0));
					} else if (strncmp(buffer, "CWD", 3) == 0) {
						int userLoggedIn = 0;
						printf("%s\n", buffer);
						
						for (int i = 0; i < 10; i++) {
							if (loggedIn[i] == fd) {
								userLoggedIn = 1;
								break;
							}
						}

						if (!userLoggedIn) {
							char passMsg[] = "530 Not logged in.\n";
							printf("%d: 530 Not logged in.\n", fd);
							send(fd, passMsg, strlen(passMsg), 0);
						} else {
							char newDir[256];
							strcpy(newDir, buffer + 4);
							chdir(newDir);

							char currDir[256];
							strcpy(currDir, "200 directory changed to ");
							strcat(currDir, getcwd(NULL,0));
							
							printf("%s\n", currDir);
							send(fd, currDir, strlen(currDir), 0);
						}

					} else if (strncmp(buffer, "QUIT", 4) == 0) {
						char quitMsg[] = "221 Service closing control connection.\n";
						printf("%d: 221 Service closing control connection.\n", fd);
						send(fd, quitMsg, strlen(quitMsg), 0);
						close(fd);
					}
				}
			}
		}
	}



    return 0;
}


    


    


    