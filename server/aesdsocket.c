#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT_NUMBER 9000

int SOCKET = -1;
char *OUTPUT_FILE = "/var/tmp/aesdsocketdata";

// Signal handler for SIGINT
static void sigint_handler(int signalNumber)
{
    if (SOCKET != -1)
    {
        shutdown(SOCKET, SHUT_RDWR);
    }
    remove(OUTPUT_FILE);
    exit(EXIT_SUCCESS);
}

// Forward declare functions
void readData(int sock);
void openSocket();

void acceptNewConnection(int sock)
{
    printf("Listening for connection!\n");
    if (listen(SOCKET, 32) < 0)
    {
        perror("listen");
        shutdown(SOCKET, SHUT_RDWR);
        remove(OUTPUT_FILE);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);
    int newSocket = accept(sock, (struct sockaddr *)&clientAddress, &clientAddressLength);
    if (newSocket < 0)
    {
        perror("accept");
        shutdown(sock, SHUT_RDWR);
        remove(OUTPUT_FILE);
        exit(EXIT_FAILURE);
    }

    // Log accepted connection from (IP Address)
    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddress, clientIP, INET_ADDRSTRLEN);

    char outputString[27 + INET_ADDRSTRLEN];
    sprintf(outputString, "Accepted connection from %s\n", clientIP);
    syslog(LOG_INFO, "%s", outputString);

    readData(newSocket);
}

void readData(int sock)
{
    // TODO read data from the connection and append to file /var/tmp/aesdsocketdata
    const int BUFFER_SIZE = 20000;
    char *buffer = malloc(sizeof(char) * BUFFER_SIZE);
    memset(buffer, 0, BUFFER_SIZE);

    FILE *outputFile = fopen(OUTPUT_FILE, "a+");
    if (outputFile == NULL)
    {
        perror("fopen");
        closelog();
        shutdown(sock, SHUT_RDWR);
        remove(OUTPUT_FILE);
        exit(EXIT_FAILURE);
    }

    int totalBytesReceived = 0;
    while (1)
    {
        int size = recv(sock, buffer + totalBytesReceived, 1, 0); // Receive one byte at a time
        if (size < 0)
        {
            perror("recv");
            closelog();
            fclose(outputFile);
            shutdown(sock, SHUT_RDWR);
            remove(OUTPUT_FILE);
            exit(EXIT_FAILURE);
        }
        else if (size == 0)
        {
            // Connection closed by the client
            break;
        }

        if (totalBytesReceived == BUFFER_SIZE)
        {
            // Resize buffer to prevent overflow
            printf("Resizing to: %lu\n", sizeof(char) * totalBytesReceived * 2);
            char *newBuffer = malloc(sizeof(char) * totalBytesReceived * 2);
            memcpy(newBuffer, buffer, totalBytesReceived - 1);
            char *oldBuffer = buffer;
            buffer = newBuffer;
            free(oldBuffer);
        }

        if (buffer[totalBytesReceived] == '\n')
        {
            printf("Received new line!\n");
            // Newline character received, output string to file
            int numBytes = fprintf(outputFile, "%s", buffer);

            if (numBytes <= 0)
            {
                perror("fprintf");
                closelog();
                fclose(outputFile);
                shutdown(sock, SHUT_RDWR);
                remove(OUTPUT_FILE);
                exit(EXIT_FAILURE);
            }

            // TODO read in entire contens of file for sending
            // fseek(outputFile, 0, SEEK_END);
            // long length = ftell(outputFile);
            fseek(outputFile, 0, SEEK_SET);
            char *fileBuffer = malloc(BUFFER_SIZE);
            size_t bytesRead;
            // Read the file and send its contents over the socket
            while ((bytesRead = fread(fileBuffer, 1, BUFFER_SIZE, outputFile)) > 0)
            {
                if (send(sock, fileBuffer, bytesRead, 0) < 0)
                {
                    perror("send");
                    closelog();
                    fclose(outputFile);
                    shutdown(sock, SHUT_RDWR);
                    remove(OUTPUT_FILE);
                    exit(EXIT_FAILURE);
                }
            }
            free(fileBuffer);

            memset(buffer, 0, BUFFER_SIZE);
            totalBytesReceived = 0;
        }
        totalBytesReceived++;
    }

    fclose(outputFile);
    free(buffer);
    acceptNewConnection(SOCKET);
}

void openSocket()
{
    SOCKET = socket(AF_INET, SOCK_STREAM, 0);

    if (SOCKET < 0)
    {
        perror("socket");
        closelog();
        exit(EXIT_FAILURE);
    }

    int value = 1;
    if (setsockopt(SOCKET, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0)
    {
        perror("setsockopt");
        closelog();
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT_NUMBER);

    if (bind(SOCKET, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind");
        closelog();
        shutdown(SOCKET, SHUT_RDWR);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

    bool daemonMode = false;
    if (argc > 1 && strcmp(argv[1], "-d") == 0)
    {
        printf("Got argument: %s\n", argv[1]);
        daemonMode = true;
    }

    if (signal(SIGINT, sigint_handler) == SIG_ERR)
    {
        perror("signal");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGTERM, sigint_handler) == SIG_ERR)
    {
        perror("signal");
        exit(EXIT_FAILURE);
    }
    remove(OUTPUT_FILE);

    openSocket();

    if (daemonMode)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            closelog();
            shutdown(SOCKET, SHUT_RDWR);
            exit(EXIT_FAILURE);
        }
        else if (pid != 0)
        {
            // Parent can exit now
            exit(EXIT_SUCCESS);
        }
    }

    acceptNewConnection(SOCKET);

    return 0;
}