#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <iomanip>

#define MAX_CLIENTS	20
#define BUFLEN 256

using namespace std;

//structura unde imi retin clientii
typedef struct client   {
	char nume[13];
	char prenume[13];
	char nume_card[7];
	char pin[5];
	char parola[20];
	double sold;
	int blocat;
	pair< struct client*, double> transfer;
} Client;

void error(char *msg)
{
    perror(msg);
    exit(1);
}

//retin useri deja logati intr-un map : Primul e socketul pe care s-au logat si cheia este datele respective
unordered_map<int, Client> useri_logati;
unordered_map<int, pair <Client, int> > last_login_socket;
vector<pair<int, Client> > clienti_blocati;


//parsare fisierului in structura
vector<Client> read_from_file(char *nume_fisier)
{
	char str[BUFLEN];
	int number_of_lines;
	FILE * file;
	vector<Client> clienti;

	file = fopen( nume_fisier, "r");
	fgets(str, 1000, file);
	sscanf(str, "%d", &number_of_lines);

	for(int i = 0; i < number_of_lines; i++ )
	{
		Client client;
		client.transfer = {NULL, 0};
		client.blocat = 0;
		if(fgets(str, 1000, file) == NULL)
			break;
		if(sscanf(str, "%s %s %s %s %s %lf", client.nume, client.prenume
			, client.nume_card, client.pin, client.parola, &client.sold) < 6 )
			puts("-10 : Eroare la apel parsare");

		clienti.push_back(client);
	}
	fclose(file);
	return clienti;
}

bool is_user_blocked(int socket, Client client)
{
	for(pair<int, Client> clt : clienti_blocati)
	{
		if(clt.first == socket && strcmp(clt.second.nume_card,client.nume_card) == 0)
			return true;
	}
	return false;
}
void remove_from_blocked(Client client)
{
	for(size_t i = 0; i < clienti_blocati.size(); i++)
	{
		if(strcmp(clienti_blocati.at(i).second.nume_card, client.nume_card) == 0)
		{
			last_login_socket[clienti_blocati.at(i).first].second = 0;
			clienti_blocati.erase(clienti_blocati.begin() + i);
			break;
		}
	}
}
string try_to_login(char *credentiale, int socket,const vector<Client>& clienti)
{
	char pin[10];
	char nr_card[10];
	string return_message;
	sscanf(credentiale, "%*s %s %s", nr_card, pin);
	for(Client client : clienti)
	{
		if(strcmp(client.nume_card, nr_card) == 0)
		{
			if(is_user_blocked(socket, client))
			{
				return_message = "IBANK> −5 : Card blocat";
				return return_message;
			}
			else if(strcmp(client.pin, pin) == 0)
			{

				for(pair<int, Client> it : useri_logati)
				{
					if(strcmp(it.second.nume_card, nr_card) == 0 && strcmp(it.second.pin, pin) == 0 )
					{
						return_message = "IBANK> −2 : Sesiune deja deschisa";
						return return_message;
					}
				}

				string nume(client.nume);
				string prenume(client.prenume);
				last_login_socket[socket].second = 0;
				useri_logati.insert(make_pair(socket, client));
				return_message = "IBANK> Welcome " + nume + " " + prenume;
				return return_message;
			}
			else
			{
				//nu a mai incercat nimeni sa se logheze pe socketul respectiv
				if(last_login_socket.find(socket) == last_login_socket.end())
				{
					last_login_socket.insert({ socket, {client,1}});
				}
				//daca a incercat pe alt card
				else if(strcmp(last_login_socket[socket].first.nume_card, client.nume_card) != 0)
				{
					last_login_socket[socket] = {client, 1};
				}
				else
				{
					last_login_socket[socket].second++;
					if(last_login_socket[socket].second > 2 && !is_user_blocked(socket, client))
					{
						clienti_blocati.push_back({socket, client});
					}
				}

				return_message = "IBANK> −3 : Pin gresit";
				return return_message;
			}
		}
	}
	return_message = "IBANK> −4 : Numar card inexistent";
	return return_message;
}

inline bool is_not_logged(int id_client)
{
	return useri_logati.find(id_client) == useri_logati.end();
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd,udp_socket, portno;
	 unsigned int clilen;
     char buffer[BUFLEN];
     struct sockaddr_in serv_addr,udp_addr, cli_addr;
     int n, i;

     fd_set read_fds;	//multimea de citire folosita in select()
     fd_set tmp_fds;	//multime folosita temporar
     int fdmax;		//valoare maxima file descriptor din multimea read_fds

     if (argc < 3) {
         fprintf(stderr,"Usage : %s port\n", argv[0]);
         exit(1);
     }

	 //parsam multimea in structura
	 vector<Client> clienti = read_from_file(argv[2]);

     //golim multimea de descriptori de citire (read_fds) si multimea tmp_fds
     FD_ZERO(&read_fds);
     FD_ZERO(&tmp_fds);

	 //socketul inactiv al servarului
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
	 udp_socket = socket (AF_INET, SOCK_DGRAM, 0);
     if (sockfd < 0 || udp_socket < 0)
	 {
		string mesg = "-10 : Eroare la apel SOCKET";
		error((char*)mesg.c_str());
	}

     portno = atoi(argv[1]);

	 // socketul pasiv al TCP
     memset((char *) &serv_addr, 0, sizeof(serv_addr));
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;	// foloseste adresa IP a masinii
     serv_addr.sin_port = htons(portno);

	 //socketul activ UDP
	 memset((char *) &udp_addr, 0, sizeof(udp_addr));
	 udp_addr.sin_family = AF_INET;
	 udp_addr.sin_addr.s_addr = INADDR_ANY;	// foloseste adresa IP a masinii
	 udp_addr.sin_port = htons(portno);

	 //facem bind pe socketul pasiv al serverului
     if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
	 {
	   string mesg = "-10 : Eroare la apel BIND";
	   error((char*)mesg.c_str());
     }

	 //facem bind pentrul socketul de udp
	 if (bind(udp_socket, (struct sockaddr *) &udp_addr, sizeof(struct sockaddr)) < 0)
	 {
		 string mesg = "-10 : Eroare la apel BIND";
		 error((char*)mesg.c_str());
	 }
	 // initializem stiva de clienti
     listen(sockfd, MAX_CLIENTS);

     //adaugam noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
     FD_SET(sockfd, &read_fds);
	 FD_SET(udp_socket, &read_fds);
	 FD_SET(0, &read_fds);
     fdmax = sockfd < udp_socket ? udp_socket : sockfd;


	 for(;;) {

		tmp_fds = read_fds;

		//toate conexiunile care vin pe socketul inactiv
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
		{
			string mesg = "-10 : Eroare la apel SELECT";
			error((char*)mesg.c_str());
		}

		for(i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if(i == 0)
				{
					memset(buffer, 0 , BUFLEN);
                    fgets(buffer, BUFLEN-1, stdin);
					if(strncmp(buffer, "quit",4) == 0)
					{
						char mesaj[]= "IBANK> Serverul se inchide";
						for(pair<int ,Client> conex : useri_logati)
						{
                    		n = send(conex.first, mesaj,strlen(mesaj), 0);
                    		if (n < 0){
								string mesg = "-10 : Eroare la apel SEND";
								error((char*)mesg.c_str());
							}

						}
						puts("Severul se inchide");
						close(sockfd);
				        return 0;
					}

				}
				else if(i == udp_socket)
				{
					struct sockaddr_in from_station ;
					socklen_t len;
					string response ;
					if(recvfrom (udp_socket, &buffer, BUFLEN, 0, (struct sockaddr*) &from_station, &len) < 0)
					{
						puts("nu merge recv");
					}
					char aux[20];

					sscanf(buffer, "%s %*s", aux);
					if(strncmp(aux,"unlock",6) == 0)
					{
						char aux[20];
						sscanf(buffer,"%*s %s", aux);
						bool gasit = false;
						bool blocat = false;
						for(Client client : clienti)
						{
							if(strcmp(aux, client.nume_card))
							{
								gasit = true;
								break;
							}

						}
						for(pair<int, Client> client : clienti_blocati)
						{
							if(strcmp(aux, client.second.nume_card) == 0)
							{
								blocat = true;
								break;
							}
						}
						response = "UNLOCK> Trimite parola secreta";
						if(gasit == false)
						{
							response = "UNLOCK> -1 : Cod eroare";
						}
						else if(blocat == false)
						{
							response = "UNLOCK> -6 : Operatie esuata";
						}
						else {
							response = "UNLOCK> Trimite parola secreta";
						}
						memset(buffer,0,sizeof(buffer));
						sprintf(buffer, "%s", (char*)response.c_str());
						int readbytes = -1;
						for(int i = 0 ; i < 5 && readbytes == -1; i++) {
							readbytes = sendto (udp_socket, buffer, 1000, 0, (struct sockaddr*) &from_station, len);
					    }
						if(readbytes == -1)
							puts("-10 : Eroare la apel sendto -> te rog sa mai introduci inca o data comanada unlock");
					}
					else
					{
						char nume_card[20];
						char parola[20];
						sscanf(buffer ,"%s %s",nume_card, parola);
						for(pair<int, Client> client : clienti_blocati)
						{
							if(strcmp(nume_card, client.second.nume_card) == 0)
							{

								if(strcmp(parola, client.second.parola) == 0)
								{
									remove_from_blocked(client.second);
									response = "UNLOCK> Card deblocat";
									sendto (udp_socket, response.c_str(), BUFLEN, 0, (struct sockaddr*) &from_station, sizeof(from_station));
								}
								else
								{
									response = "UNLOCK> −7 : Deblocare esuata";
									sendto (udp_socket, response.c_str(), BUFLEN, 0, (struct sockaddr*) &from_station, sizeof(from_station));
								}
								break;
							}
						}
					}
				}
				else if (i == sockfd) {
					// a venit ceva pe socketul inactiv(cel cu listen) = o noua conexiune
					// actiunea serverului: accept()
					clilen = sizeof(cli_addr);
					if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen)) == -1) {
						string mesg = "-10 : Eroare la apel ACCEPT";
						error((char*)mesg.c_str());
					}
					else {
						//adaug noul socket intors de accept() la multimea descriptorilor de citire
						FD_SET(newsockfd, &read_fds);
						if (newsockfd > fdmax) {
							fdmax = newsockfd;
						}
					}

				}
				else {
					memset(buffer, 0, BUFLEN);
					if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
						if (n == 0) {
							if(useri_logati.find(i) != useri_logati.end()) {
								useri_logati.erase(useri_logati.find(i));
								close(i);
								FD_CLR(i, &read_fds);
								printf("Userul %d a fost delogat", i);
							}
						} else {
							string mesg = "-10 : Eroare la apel RECV";
							error((char*)mesg.c_str());
						}
						close(i);
						FD_CLR(i, &read_fds);
					}

					else { //recv intoarce >0

						char operatie[100];
						string response;
						sscanf(buffer, "%s %*s", operatie);
						if(!is_not_logged(i) && useri_logati[i].transfer.first != NULL)
						{
							if(buffer[0] == 'y')
							{
								for(Client& client : clienti)
								{
									if(strcmp(client.nume_card,useri_logati[i].nume_card) == 0)
									{
										client.sold -= useri_logati[i].transfer.second;
									}
									else if(strcmp(client.nume_card,(*useri_logati[i].transfer.first).nume_card) == 0)
									{
										client.sold += useri_logati[i].transfer.second;
									}
								}
								useri_logati[i].transfer.first = NULL;
								useri_logati[i].transfer.second = 0;
								response = "IBANK> Transfer realizat cu succes";
							}
							else
							{
								useri_logati[i].transfer.first = NULL;
								useri_logati[i].transfer.second = 0;
								response = "IBANK> −9 : Operatie anulata";
							}
						}
						else if(strcmp(operatie, "login") == 0)
						{
							response = try_to_login(buffer, i,clienti);
						}
						else if(strcmp(operatie, "logout") == 0)
						{
							if(is_not_logged(i))
							{
								response = "IBANK> −1: Clientul nu este autentificat";
							}
							else
							{
								useri_logati.erase(useri_logati.find(i));
								response = "IBANK> Clientul a fost deconectat";
							}
						}
						else if(strcmp(operatie, "listsold") == 0)
						{
							if(is_not_logged(i))
							{
								response = "IBANK> −1: Clientul nu este autentificat";
							}
							else
							{
								for(Client client : clienti)
								{
									if(strcmp(client.nume_card, useri_logati[i].nume_card) == 0)
									{
										char aux[10];
										sprintf(aux, "IBANK> %.2lf",client.sold);
										string new_string(aux);
										response = new_string;
										break;
									}
								}

							}
						}
						else if(strcmp(operatie, "transfer") == 0)
						{
							if(is_not_logged(i))
							{
								response = "IBANK> −1: Clientul nu este autentificat";
							}
							else
							{
								double sold_cerut;
								char nr_card_cautat[10];
								sscanf(buffer, "%*s %s %lf ", nr_card_cautat, &sold_cerut);

								if(sold_cerut > useri_logati[i].sold)
								{
									response = "IBANK> −8 : Fonduri insuficiente";
								}
								else
								{
									bool gasit = false;
									for(size_t j = 0; j < clienti.size(); j++)
									{
										if(strcmp(clienti.at(j).nume_card, nr_card_cautat) == 0)
										{
											useri_logati[i].transfer.first = &clienti[j];
											useri_logati[i].transfer.second = sold_cerut;
											char aux[10];
											sprintf(aux, "%.2lf",sold_cerut);
											string new_string(aux);
											response = "Transfer "+ new_string +" catre " + clienti.at(j).nume +" "+
											clienti.at(j).prenume + " "+ "? [y/n]";
											gasit = true;
											break;
										}
									}
									if (gasit == false)
									{
										response = "IBANK> −4 : Numar card inexistent";
									}
								}
							}
						}
						else {
							response = "IBANK> -11 : Comanda invalida";
						}

						if(FD_ISSET(i, &read_fds)) {
							send(i, response.c_str(), 100, 0);
						}
						else{
							char error[] = "Clientul nu e conectat\n";
							send(i,error, strlen(error), 0);
							puts(error);
						}
						memset(buffer, 0, BUFLEN);
						memset(operatie, 0, sizeof(operatie));
					}
				}
			}
		}
     }


     close(sockfd);
	 close(udp_socket);
     return 0;
}
