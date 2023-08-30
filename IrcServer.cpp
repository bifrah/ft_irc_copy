#include "IrcServer.hpp"

#define MAX_CONNECTIONS 1000

std::vector<std::string> IrcServer::tokenize(std::string &message)
{
	std::vector<std::string> tokens;
	if (message.find("\r\n") == std::string::npos)
		return tokens;
	std::string current_command = message.substr(0, message.find("\r\n"));
	message = message.substr(message.find("\r\n") + 2);
	size_t start = current_command.find_first_not_of(" \n");
	size_t end = current_command.find_first_of(" \n", start);
	std::string command = current_command.substr(start, end - start);

	if (command == "PRIVMSG" || command == "TOPIC")
	{
		for (size_t i = 0; start != std::string::npos && i < 2; ++i)
		{
			tokens.push_back(current_command.substr(start, end - start));
			start = current_command.find_first_not_of(" \n", end);
			end = current_command.find_first_of(" \n", start);
		}
		if (start != std::string::npos)
			tokens.push_back(current_command.substr(start));
		return tokens;
	}
	else
	{
		while (start != std::string::npos)
		{
			tokens.push_back(current_command.substr(start, end - start));
			start = current_command.find_first_not_of(" \n", end);
			end = current_command.find_first_of(" \n", start);
		}
	}
	return tokens;
}

void IrcServer::poll_client_connections()
{
	std::vector<pollfd> fds_to_poll;
	pollfd server_poll;
	server_poll.fd = server_socket_;
	server_poll.events = POLLIN;
	fds_to_poll.push_back(server_poll);

	while (true)
	{
		int poll_result = poll(fds_to_poll.data(), fds_to_poll.size(), -1);
		if (poll_result < 0)
		{
			perror("Error : Poll");
			exit(EXIT_FAILURE);
		}

		for (size_t i = 0; i < fds_to_poll.size(); ++i)
		{
			if (fds_to_poll[i].revents & POLLIN)
			{
				if (fds_to_poll[i].fd == server_socket_)
				{
					// Accepter une connexion entrante
					socklen_t client_address_length = sizeof(client_address_);
					int client_socket = accept(server_socket_, reinterpret_cast<sockaddr *>(&client_address_), &client_address_length);
					if (client_socket == -1)
					{
						perror("Error: accepting an incoming connection");
						continue;
					}
					// Set I/O fds to non-blocking
					if (fcntl(client_socket, F_SETFL, O_NONBLOCK) == -1)
					{
						perror("Error: setting client fd to non-blocking failed");
						exit(EXIT_FAILURE);
					}
					// Ajouter le nouveau client_socket au pollfd
					pollfd new_client_fd;
					new_client_fd.fd = client_socket;
					new_client_fd.events = POLLIN;
					fds_to_poll.push_back(new_client_fd);
				}
				else
				{
					int client_state = handle_client_connection(fds_to_poll[i].fd);
					// Supprimer le client_socket du pollfd s'il est fermé
					if (client_state == OFFLINE)
					{
						close(fds_to_poll[i].fd);
						// Supprimer l'utilisateur de la liste des utilisateurs connectés
						users_list.erase(fds_to_poll[i].fd);
						fds_to_poll.erase(fds_to_poll.begin() + i);
						--i;
					}
				}
			}
		}
	}
}

IrcServer::IrcServer(int port, const std::string &password) : port_(port), password_(password), users_list()
{
	// Création du socket
	if ((server_socket_ = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("Erreur lors de la création du socket");
		exit(EXIT_FAILURE);
	}
	if (fcntl(server_socket_, F_SETFL, O_NONBLOCK) == -1)
	{
		perror("Error: setting server fd to non-blocking failed");
		exit(EXIT_FAILURE);
	}
	int opt = 1;
	if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	// Configuration de l'adresse du serveur
	server_address_.sin_family = AF_INET;
	server_address_.sin_addr.s_addr = INADDR_ANY;
	server_address_.sin_port = htons(port_);

	// Liaison du socket à l'adresse du serveur
	if (bind(server_socket_, (struct sockaddr *)&server_address_, sizeof(server_address_)) < 0)
	{
		perror("Erreur lors de la liaison du socket");
		exit(EXIT_FAILURE);
	}

	// Écoute des connexions entrantes
	if (listen(server_socket_, MAX_CONNECTIONS) == -1)
	{
		perror("Erreur lors de l'écoute des connexions entrantes");
		exit(EXIT_FAILURE);
	}
}
