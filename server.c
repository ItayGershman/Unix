// Server side C program to demonstrate Socket programming 

#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/mman.h>

#define PORT 50000 

int download(int newfd, int amount); 
int upload(int newfd, int amount); 
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa){
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int main(int argc, char *argv[]) { 
	int server_fd, newfd, valread ,action = 0,size = 0,fd,countFiles = 0,amount = 0, ibuf=0;
    void * dst;
    int* fileFD = (int*)malloc(1);
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number 
	struct sockaddr_in address; 
    struct sockaddr_storage remoteaddr; // client address
	int opt = 1; 
	int addrlen = sizeof(address); 
	char* buf = 0; 
    
	// char *hello = "Hello from server"; 
    char remoteIP[INET6_ADDRSTRLEN];
	
	// Creating socket file descriptor 
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { 
		perror("socket failed"); 
		exit(EXIT_FAILURE); 
	} 
	
	// Forcefully attaching socket to the port 8080 
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) { 
		perror("setsockopt"); 
		exit(EXIT_FAILURE); 
	} 
	address.sin_family = AF_INET; 
	address.sin_addr.s_addr = INADDR_ANY; 
	address.sin_port = htons( PORT ); 
	
	// Forcefully attaching socket to the port 8080 
	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) { 
		perror("bind failed"); 
		exit(EXIT_FAILURE); 
	} 
	if (listen(server_fd, 3) < 0) { 
		perror("listen"); 
		exit(EXIT_FAILURE); 
	} 

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    // add the listener to the master set
    FD_SET(server_fd, &master);

    // keep track of the biggest file descriptor
    fdmax = server_fd; // so far, it's this one

    for(;;) {  
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }
        for(int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == server_fd) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
					if ((newfd = accept(server_fd,(struct sockaddr *)&remoteaddr,&addrlen)) == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        printf("selectserver: new connection from %s on socket %d\n",
							inet_ntop(remoteaddr.ss_family,
                            get_in_addr((struct sockaddr*)&remoteaddr),remoteIP, INET6_ADDRSTRLEN),newfd);
                    }
                } else {
                    //recv argc - this way we'll know how many fp we need to download/upload
                    if(recv(i, &amount,sizeof(int),0) < 1){
                        perror("Could not receive amount off fp to download/upload");
                        return -1;
                    }           
                    // handle data from a client
                    if(recv(i, &action,sizeof (int),0) < 1){ //'upload' = 2, 'download' = 1
                        perror("recv");
                        return -1;
                    }    
                    //choose download/upload
                    switch(action){
                        case 1:
                        if(download(newfd,amount)==0){
                            perror("download");
                            return -1;//dowbload failed
                        }
                        else {
                            close(newfd);
                            FD_CLR(i, &master); // remove from master set
                        }
                        break;
                        case 2:
                        if(upload(newfd,amount)==0){
                            perror("upload");
                            return -1;//upload failed
                        }
                        else {
                            close(newfd);
                            FD_CLR(i, &master); // remove from master set                           
                        }
                        break;
                        default:
                            close(newfd);
                            FD_CLR(i, &master); // remove from master set
                            break;
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
}

int download(int newfd, int amount){
    int length = 0,pid;
    int nameLen = 0;
	int myFiles = 2;
	int fdin = 0;
	struct stat statbuf;
	void *src;
    char *buff;
	int i = 0;
    //recieving amount of files into length
	if (recv(newfd, &length, sizeof(int), 0) <= 0){
		perror("recv");
		return 0;
	}
	if(length == 0){
		printf("no Files");
		return 0;
	}
    printf("length: %d\n",length);
	// zip
	char** files = (char**)malloc((length+3)*sizeof(char*));
	memset(files,0,length+3);
	files[0] = (char*)malloc(256*sizeof(char));
	memset(files[0],0,256);
	strcpy(files[0],"zip");
	files[0][256]='\0';
	files[1] = (char*)malloc(256*sizeof(char));
	memset(files[1],0,256);
	strcpy(files[1],"MyFiles.zip");
	files[1][256]='\0';
	//files[0] = "zip";
	//files[1] = "MyFiles.zip";
	//files[2] = "name of file";
	//      .
	//		.
	//		.
	//files[n] = "name of file";

    //get files from client
	for( i=0; i<length;++i){
        //file length
		if (recv(newfd, &nameLen, sizeof(int), 0) <= 0){
			perror("recv");
			return 0;
		}
		if(nameLen == 0){
			perror("Didnt receive data from client !");
			return 0;
		}
        //Allocating memory from nameLen which is the file length
		buff = (char *)malloc((nameLen + 1) * sizeof(char));
		if( buff == NULL){
			perror("could not allocate memory");
			return 0;
		}

		memset(buff, 0, nameLen);
        //get file name
		if (recv(newfd, buff, nameLen, 0) <= 0){
			perror("recv");
			return 0;
		}

		buff[nameLen] = '\0';

		files[myFiles] = (char*)malloc((nameLen+1)*sizeof(char));

		if(files[myFiles] == NULL){
			perror("malloc");
			return -1;
		}
        //set into files[myFiles] - file name
		memset(files[myFiles],0,nameLen);
		files[myFiles][nameLen+1] = '\0';
		strcpy(files[myFiles],buff);
		myFiles++; // in order to get another file...
		free(buff);
	}

	files[length+3] = NULL;

	int flag = 1;
	//checks whether the calling process can access the file pathname
	for(i = 0; i < length; ++i){
		if(access(files[i+2],F_OK ) == -1){
			perror("access");
			flag = -1;
			break;
		}
	}
    //Sending authantication
	if (send(newfd, &flag, sizeof(int), 0) < 0){
		perror("Error could not send data");
		return -1;
	}

	if(flag == -1){
		perror("requested files all do not exist");
		return -1;
	}
	//make new process to run excecvp
	if(pid=vfork() == 0){
		if(length > 1){
			if (execvp("zip",files) == -1){
                perror("execvp");
            }
		}
		else exit(1);
	}
	else {
		//Before wait to child process
		wait(NULL);
		if(length > 1){//if there is more than 1 files
			if ((fdin = open("MyFiles.zip", O_RDONLY)) < 0){
				perror("Could not open FILE with given name: 'MyFiles.zip'\n");
				return -1;
			}
		}
		else{
			if ((fdin = open(files[2], O_RDONLY)) < 0){
				perror("Could not open FILE with given name\n");
				return -1;
			}
		}

		if (fstat(fdin, &statbuf) < 0){
			perror("fstat");
			return -1;
		}
		//send file size
		if (send(newfd, &statbuf.st_size, sizeof(off_t), 0) < 0){
			perror("send");
			return -1;
		}

		if ((src = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, fdin, 0)) == MAP_FAILED){
			perror("mmap");
			return -1;
		}
	
		if (send(newfd, src, statbuf.st_size, 0) < 0){
			perror("send");
			return -1;
		}

		printf("Sent to client\n");

		free(files);

		// using vfork to excecute code
		if(vfork() == 0){
			if(length > 1){
				// removing the zip file in server
				execlp("rm","-rf","MyFiles.zip",NULL);
			}
			exit(1);
		}
	}

	return 1;
} 
int upload(int newfd, int amount){
    int i = 0,length = 0,fd = 0;
    void *mybuf;
	char *buf;
	off_t size = 0;
	
	for (i = 0; i < amount; ++i){
        //recieve file length
		if (recv(newfd, &length, sizeof(int), 0) < 1){
			perror("recv");
			return 0;
		}
		if(length == 0){
			printf("no files");
			return -1;
		}
        char *buf = (char *)malloc((length + 1));
        
		if(buf == NULL){
			perror("malloc");
			return 0;
		}
		memset(buf, 0, length);
        //recv file content
		if (recv(newfd, buf, length, 0) < 1){
			perror("recv");
			return 0;
		}
        buf[length] = '\0';
        
		if(buf[0] == EOF){
			perror("No data");
			return 0;
		}
		//open buf 
		if ((fd = open(buf, O_RDWR | O_CREAT | O_TRUNC, 0777)) < 0){
			perror("open");
			return 0;
		}
		free(buf);

        //fstat info
		if ((recv(newfd, &size, sizeof(size), 0)) < 1){
			perror("receive");
			return 0;
		}
		if(size == 0){
			printf("No data in file");
			return -1;
		}
		//same variable "buf" with differnt purpose
		buf = malloc(size * sizeof(void *));
		if(buf == NULL){
			perror("malloc");
			return 0;
		}
        //set memory for the file im memory map with 0
		memset(buf, 0, size);
		if ((recv(newfd, buf, size, 0)) < 1){
			perror("recv");
			return 0;
		}
        //check if received data to buf
		if(*((int*)buf) == 0){
			perror("no data");
			return 0;
		}

		if (lseek(fd, size - 1, SEEK_SET) == -1){
			perror("lseek");
			return 0;
		}

		if (write(fd, "", 1) != 1){
			perror("write");
			return 0;
		}

		if ((mybuf = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED){
			perror("map");
			return 0;
		}
		memcpy(mybuf, buf, size);
		free(buf);
	}
	return 1;
}