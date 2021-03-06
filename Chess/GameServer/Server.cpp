#include "Server.h"

#define MY_PORT "4006"
#define BACKLOG 10
#define BUF_LEN (2 << 16)

void printHostName(){
	char hostname[20];
	gethostname(hostname, 20);
	printf("Host name : %s\n\n", hostname);
}

void printPeerInfo(struct sockaddr* peer_sockaddr, uint* peer_addrlen){
	void *addr, *port;
	char* ipver;
	char ipstr[INET6_ADDRSTRLEN];

	if (peer_sockaddr->sa_family == AF_INET) { // IPv4
       struct sockaddr_in* ipv4_socket = (struct sockaddr_in*) peer_sockaddr;
       addr = &(ipv4_socket->sin_addr);
       ipver = (char*) "IPv4";
			 port = &(ipv4_socket->sin_port);
			 inet_ntop(AF_INET, addr, ipstr, INET_ADDRSTRLEN);
  	}
	else { // IPv6
        struct sockaddr_in6 *ipv6_socket = (struct sockaddr_in6 *) peer_sockaddr;
        addr = &(ipv6_socket->sin6_addr);
        ipver = (char*) "IPv6";
				port = &(ipv6_socket->sin6_port);
				inet_ntop(AF_INET6, addr, ipstr, INET6_ADDRSTRLEN);
  	}
	std::cout << "IP               IP protocol     Port       Address Length\n";
	std::cout << ipstr << " " << ipver << "        	 " << port << "  " << peer_addrlen << "\n";
}



//Create addrinfo and initialise 'res' and bind 'sockfd_listener'
int initialiseListener(struct addrinfo hints, struct addrinfo* res, int* sockfd_listener){
	int err_status, optval = 1;
	//Set res to point to server addrinfo
	if ((err_status = getaddrinfo(NULL, MY_PORT, &hints, &res ) != 0)) {
		fprintf(stderr, "listener creation failed: %s\n", gai_strerror(err_status));
		return 2;
	}
	//Create sockfd for the server
	*sockfd_listener = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	setsockopt(*sockfd_listener, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	//Bind the server's sockfd to the server's addrinfo
	if ((err_status = bind(*sockfd_listener, res->ai_addr, res->ai_addrlen)) < 0){
		perror("Server - bind() failed");
		exit(EXIT_FAILURE);
	}
	//Set server to listen on that sockfd
	listen(*sockfd_listener, BACKLOG);
	std::cout << "Waiting for connection\n";

	return 1;
}

int receiveClient(int client_sockfd, char* buffer){
	int n = recv(client_sockfd, buffer, BUF_LEN, 0);
	switch(n){
		case (-1) : perror("Server - recv failed\n");
								exit(EXIT_FAILURE);
								break;
	  	case (0)	: return 0;
		default		: return 1;
	}
}

//Pass everything by reference
void acceptClient(	int* client_sockfd,
					int* server_sockfd,
					struct sockaddr_storage* client_addresses,
					socklen_t* addr_size){
	if ( (*client_sockfd =	accept(*server_sockfd, (struct sockaddr*) &(*client_addresses), &(*addr_size))) < 0){
		perror("Server - accepting client failed\n");
		exit(EXIT_FAILURE);
	}
	std::cout << "New client connected with sockfd " << *client_sockfd << "\n";
}

void gameDisconnect(std::vector<Player>& players, 
						fd_set& connections_fd, 
						int client_fd){

	close(client_fd);
	FD_CLR(client_fd, &connections_fd);
	for(auto it = players.begin(); it != players.end(); it++){
		if(it->fd == client_fd){
			//Refresh opponent, if exists
			int opponent_fd = it->opponent_fd;
			if(opponent_fd != -1){
				for(auto itOpponent = players.begin(); itOpponent != players.end(); itOpponent++){
					if(itOpponent->fd == opponent_fd){
						itOpponent->state = PlayerState::InLobby;
						itOpponent->opponent_fd = -1;
						break;
					}
				}
			}
			players.erase(it);
			return;
		}
	}
}

void matchRoom(	std::vector<Player>& players, 
				int opponent_fd,
				std::string hostName){
	for(auto itHost = players.begin(); itHost != players.end(); itHost++){
		//Found host
		if(itHost->name == hostName){
			Player host = *itHost;
			itHost->state = PlayerState::Playing;
			itHost->opponent_fd = opponent_fd;
			for(auto itOpponent = players.begin(); itOpponent != players.end(); itOpponent++){
				if(itOpponent->fd == opponent_fd){
					itOpponent->state = PlayerState::Playing;
					itOpponent->opponent_fd = itHost->fd;
					//Confirm connection
					std::string confirmGameHost = "3#" + itOpponent->name;
					std::string confirmGameOpponent  = "3#" + hostName;
					send(host.fd, confirmGameHost.c_str(), 
						strlen(confirmGameOpponent.c_str()) + 1, 0);
					send(opponent_fd, confirmGameOpponent.c_str(), 
						strlen(confirmGameOpponent.c_str()) + 1, 0);
					return;
				}
			}
			break;
		}
	}				
}


void notifyRoom(std::vector<Player>& players,
				Player host){
	//Update host state
	for(auto it = players.begin(); it != players.end(); it++){
		if(it->fd == host.fd){
			it->state = PlayerState::Hosting;
		}
	}
	//Notify lobby of new room				
	std::string roomClientName = "2#" + host.name;		
	for(auto it = players.begin(); it !=players.end(); it++){
		if(it->state != PlayerState::Playing){
			send(it->fd, roomClientName.c_str(), 
				strlen(roomClientName.c_str()) + 1, 0);
		}
	}
	//Confirm hosting to hoster				
	send(host.fd, roomClientName.c_str(), 
		strlen(roomClientName.c_str()) + 1, 0);

	printf("New host room created for fd %d\n", host.fd);
}

Player findPlayerByName(std::string name, std::vector<Player> players){
	for(auto it = players.begin(); it != players.end(); it++){
		if(it->name == name){
			return *it;
		}
	}
	return Player(-1, "");
}

Player findPlayerByFd(int fd, std::vector<Player> players){
	for(auto it = players.begin(); it != players.end(); it++){
		if(it->fd == fd){
			return *it;
		}
	}
	return Player(-1, "");
}

void matchFlag(std::vector<Player>& players, int fd, char* buffer){
	std::string clientMessage(buffer);
	char* flag = buffer;
	printf("%s\n", clientMessage.c_str());
	//New client
	//Receive Format = 0#<clientName>
	if(*flag == '0' ){
		if(findPlayerByFd(fd, players).fd == -1){
			std::string clientName = clientMessage.erase(0, 2);
			Player newPlayer(fd, clientName); 
			players.push_back(newPlayer);
			//Send connection confirmation
			//Send Format = 1#<clientName>
			std::string response = "1#" + clientName;
			send(fd, response.c_str() , (int) sizeof(response) + 1, 0);
		}
	}
	//Make a room request
	//Receive Format = 1#<anything>
	else if(*flag == '1'){
		Player host = findPlayerByFd(fd, players);
		if(host.fd != -1){
			notifyRoom(players, host);
		}
	}
	//Connect a room request
	//Receive Format = 2#<hostname>
	else if(*flag == '2'){
		std::string hostName = clientMessage.erase(0, 2);
		Player host = findPlayerByName(hostName, players);
		if(host.fd != -1){
			matchRoom(players, fd, hostName);
		}
	}
	//Game playing
	//Any format
	else {
		Player isInGame = findPlayerByFd(fd, players);
		if(isInGame.fd != -1 && isInGame.opponent_fd != -1){
			std::cout << "Sending game move " << clientMessage << std::flush;
			send(isInGame.opponent_fd, clientMessage.c_str(), 
				strlen(clientMessage.c_str()) + 1, 0);
		}
	}
}

int main(int argc, char *argv[])
{
	// Create file descriptor sets, one for reading and one for updating the next read
	fd_set connections_fd, reads_fd;
	int max_fd;

	// Client storage
	std::vector<Player> players;
	struct sockaddr_storage client_addresses;
	socklen_t addr_size = sizeof client_addresses;

	// Server variables
	struct addrinfo hints, *addrinfo_server;
	int sockfd_server;	//Points to socket descriptor
	char* buffer = (char*) malloc(sizeof(char));

	// Initialise addrinfo restrictions
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

	// Set up server
	initialiseListener(hints, addrinfo_server, &sockfd_server);
	FD_ZERO(&connections_fd);
	FD_ZERO(&reads_fd);
	// Add server file descriptor to fd set
	FD_SET(sockfd_server, &connections_fd);
	max_fd = sockfd_server;
	printHostName();

	// Allocate empty peer fields for temporary storage of connected clients
	struct sockaddr* 	client_sockaddr = (struct sockaddr*) malloc(sizeof(struct sockaddr));
	uint* 				client_addrlen 	= (uint*) malloc(sizeof(uint));
	int					client_sockfd	= -1;

	while(1){
		reads_fd = connections_fd;
		//Updates 'reads_fd' to indicate which socket file descriptors are ready to read
		if (select(max_fd + 1, &reads_fd, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
    	}
		//Iterate through client filedescriptors (ws connections)
		for(int i = 0; i < max_fd + 1; i++){
			//Server file descriptor (ws) receives new I/O operation
			if(FD_ISSET(i, &reads_fd)){
				//Fd for Server listener receives ws request
				if (i == sockfd_server){
					acceptClient(&client_sockfd, &sockfd_server, &client_addresses, &addr_size);
					FD_SET(client_sockfd, &connections_fd);
					max_fd = (max_fd < client_sockfd) ? client_sockfd : max_fd;
					std::string welcomeMessage = "0#Awaiting Name\n";
					send(client_sockfd, welcomeMessage.c_str(), (int) sizeof(welcomeMessage), 0);
				}
				else{
					//Client closed ws
					if(!receiveClient(i, buffer)){
						std::cout << "0#Disconnected : " + findPlayerByFd(i, players).name << std::flush;
						gameDisconnect(players, connections_fd, i);
					}
					//Fd corresponding to client receives new I/O operation
					else{
						matchFlag(players, i, buffer);
					}
					memset(buffer, 0, 50*sizeof(char));
				}
			}
		}
	}
	return 0;
}





























//
