#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <fstream>
#include <iostream>

#define BUFLEN 256

using namespace std;

void error(string msg)
{
    perror((char*)msg.c_str());
    exit(0);
}
string last_login;

int main(int argc, char *argv[])
{
    int sockfd, udp_socket, n;
    struct sockaddr_in serv_addr,udp_addr;
	bool udp_conn = false;
	bool already_logged = false;

    fd_set read_fds;  //multimea de citire folosita in select()
    fd_set tmp_fds;    //multime folosita temporar
    int fdmax;     //valoare maxima file descriptor din multimea read_fds

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
	string file_name = "client-"+ to_string(getpid())+ ".log";

	//file to write to it
	ofstream f(file_name.c_str());
    char buffer[BUFLEN];
    if (argc < 3) {
       fprintf(stderr,"Usage %s server_address server_port\n", argv[0]);
       exit(0);
    }

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	udp_socket = socket (AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0 || udp_socket < 0)
	   error("ERROR opening socket");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &serv_addr.sin_addr);

	udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &udp_addr.sin_addr);

    if (connect(sockfd,(struct sockaddr*) &serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    FD_SET(sockfd, &read_fds);
	FD_SET(udp_socket, &read_fds);
    FD_SET(0, &read_fds);

    fdmax = sockfd < udp_socket ? udp_socket : sockfd;

    int i;
     // main loop
    while (1) {
        tmp_fds = read_fds;
        if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1){
			error("ERROR in select");
		}
         for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp_fds)) {
                if(i != 0 && i != udp_socket)
                {
                    memset(buffer, 0, BUFLEN);
                    if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
                        close(i);
                        FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul pe care
                    }
                    else{
						if(strncmp(buffer,"IBANK> Welcome",14) == 0)
							already_logged = true;
						f << buffer << '\n';
						f.flush();
                        puts(buffer);

						if(strcmp(buffer,"IBANK> Serverul se inchide") == 0)
						{
							close(sockfd);
							close(udp_socket);
							f.close();
							return 0;
						}
                    }
                }
				else if(i == udp_socket){
					struct sockaddr_in from_station ;
					socklen_t len;
					memset(buffer, 0 , BUFLEN);
					recvfrom (udp_socket, &buffer, BUFLEN, 0, (struct sockaddr*) &from_station, &len);
					if(strcmp(buffer, "UNLOCK> Card deblocat") ==  0 ||
						strcmp(buffer, "UNLOCK> −7 : Deblocare esuata") == 0  ||
					    strcmp(buffer, "UNLOCK> -1 : Cod eroare") == 0  ||
						strcmp(buffer, "UNLOCK> -6 : Operatie esuata") == 0) {
						udp_conn = false;
					}
					puts(buffer);
					f << buffer << '\n';
					f.flush();
				}
                else if(i == 0)
                {
                    memset(buffer, 0 , BUFLEN);
                    fgets(buffer, BUFLEN-1, stdin);
					f << buffer;
					f.flush();
					socklen_t len;
					char oper[20];
					sscanf(buffer, "%s %*s",oper);
					if(strncmp(oper, "unlock", 6) == 0)
					{
						udp_conn = true;
						char aux[20];
						sscanf(last_login.c_str(), "%*s %s %*s", aux);
						string new_string(aux);
						sscanf(buffer, "%s", buffer);
						sprintf(buffer,"%s %s", buffer, aux);
						struct sockaddr_in from_station ;
						sendto (udp_socket, &buffer, BUFLEN, 0, (struct sockaddr*) &udp_addr, sizeof(udp_addr));
					}
					else if(udp_conn == true)
					{
						struct sockaddr_in from_station ;
						char aux[20];
						sscanf(last_login.c_str(), "%*s %s %*s", aux);
						string numar_card(aux);
						string parola(buffer);
						string response = numar_card + " " + parola;
						sendto (udp_socket, (response.c_str()), BUFLEN, 0, (struct sockaddr*) &udp_addr, sizeof(udp_addr));
					}

					else
					{
						//cout << already_logged << ' ' << buffer << '\n';
						if(strncmp(buffer,"login", 5) == 0 && already_logged == true)
						{
							puts("−2 : Sesiune deja deschisa");
							f << buffer;
							f.flush();
						}
						else if(strncmp(buffer,"logout", 6) == 0 && already_logged == false)
						{
							puts("−1 : Clientul nu este autentificat");
							f << buffer;
							f.flush();
						}
						else
						{
							if(strncmp(buffer,"logout",6) == 0)
							{
								already_logged = false;
							}
							if(strncmp(buffer,"login", 5) == 0)
							{
								string new_string(buffer);
								last_login = new_string;
							}
                    		n = send(sockfd,buffer,strlen(buffer), 0);
                    		if (n < 0)
							{
								error("ERROR writing to socket");
							}

						}
					}

					if(strncmp(buffer, "quit",4) == 0)
					{
						close(sockfd);
						close(udp_socket);
						f.close();
						return 0;
					}

                }
            }
        }
    }
	f.close();
    return 0;
}
