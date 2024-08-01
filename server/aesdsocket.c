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
#include <poll.h>
#include <time.h>
#include <pthread.h>

#include "freeBSDQueue.h"

#define PORT_NUMBER 9000

int SOCKET = -1;
char *OUTPUT_FILE = "/var/tmp/aesdsocketdata";
bool CONTINUE_EXECUTING = true;

typedef struct thread_data
{
    pthread_t thread;
    pthread_mutex_t *outputFileMutex;
    bool completed;
    int socket;
} thread_data;

typedef struct slist_data_s
{
    thread_data *data;
    SLIST_ENTRY(slist_data_s)
    nodes;
} slist_data_s;

// Signal handler for SIGINT
static void
sigint_handler(int signalNumber)
{
    CONTINUE_EXECUTING = false;
}

// Forward declare functions
void readData(int sock, pthread_mutex_t *outputFileMutex);
void openSocket();

int acceptNewConnection(int sock)
{
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

    syslog(LOG_INFO, "Accepted connection from %s\n", clientIP);

    return newSocket;
}

int writeToFile(const char *string, pthread_mutex_t *mutex)
{
    // Acquire mutex
    if (pthread_mutex_lock(mutex) != 0)
    {
        perror("pthread_mutex_lock");
        CONTINUE_EXECUTING = false;
        return -1;
    }
    FILE *outputFile = fopen(OUTPUT_FILE, "a+");
    if (outputFile == NULL)
    {
        perror("fopen");
        CONTINUE_EXECUTING = false;
        pthread_mutex_unlock(mutex);
        return -1;
    }

    int bytesRemaining = strlen(string);

    while (bytesRemaining > 0)
    {
        int bytesWritten = fprintf(outputFile, "%s", string);

        if (bytesWritten < 0)
        {
            perror("fprintf");
            fclose(outputFile);
            CONTINUE_EXECUTING = false;
            pthread_mutex_unlock(mutex);
            return -1;
        }

        bytesRemaining -= bytesWritten;
    }

    fclose(outputFile);
    pthread_mutex_unlock(mutex);
    return 0;
}

void readData(int sock, pthread_mutex_t *outputFileMutex)
{
    const int BUFFER_SIZE = 20000;
    char *buffer = malloc(sizeof(char) * BUFFER_SIZE);
    memset(buffer, 0, BUFFER_SIZE);

    int totalBytesReceived = 0;
    while (CONTINUE_EXECUTING)
    {
        int size = recv(sock, buffer + totalBytesReceived, 1, 0); // Receive one byte at a time
        if (size < 0)
        {
            perror("recv");
            closelog();
            shutdown(sock, SHUT_RDWR);
            remove(OUTPUT_FILE);
            exit(EXIT_FAILURE);
        }
        else if (size == 0)
        {
            // Connection closed by the client
            printf("Closing connection!\n");
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
            printf("Outputting string: %s\n", buffer);

            if (writeToFile(buffer, outputFileMutex) < 0)
            {
                return;
            }

            FILE *outputFile = fopen(OUTPUT_FILE, "r");
            if (outputFile == NULL)
            {
                perror("fopen");
                CONTINUE_EXECUTING = false;
                return;
            }

            char *fileBuffer = malloc(BUFFER_SIZE);
            size_t bytesRead;
            // Read the file and send its contents over the socket
            while ((bytesRead = fread(fileBuffer, 1, BUFFER_SIZE, outputFile)) > 0)
            {
                if (send(sock, fileBuffer, bytesRead, 0) < 0)
                {
                    perror("send");
                    CONTINUE_EXECUTING = false;
                    return;
                }
            }
            free(fileBuffer);
            fclose(outputFile);

            memset(buffer, 0, BUFFER_SIZE);
            totalBytesReceived = 0;
        }
        totalBytesReceived++;
    }

    free(buffer);
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

    if (listen(SOCKET, 32) < 0)
    {
        perror("listen");
        closelog();
        shutdown(SOCKET, SHUT_RDWR);
        exit(EXIT_FAILURE);
    }
}

void *handleNewConnection(void *threadParams)
{
    struct thread_data *threadData = (struct thread_data *)threadParams;
    threadData->thread = pthread_self();
    readData(threadData->socket, threadData->outputFileMutex);
    threadData->completed = true;
    threadData->thread = pthread_self();

    return 0;
}

int main(int argc, char *argv[])
{
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

    bool daemonMode = false;
    if (argc > 1 && strcmp(argv[1], "-d") == 0)
    {
        daemonMode = true;
    }

    if (signal(SIGINT, sigint_handler) == SIG_ERR)
    {
        perror("signal");
        closelog();
        exit(EXIT_FAILURE);
    }

    if (signal(SIGTERM, sigint_handler) == SIG_ERR)
    {
        perror("signal");
        closelog();
        exit(EXIT_FAILURE);
    }

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

    pthread_mutex_t outputFileMutex;
    if (pthread_mutex_init(&outputFileMutex, NULL) < 0)
    {
        perror("pthread_mutex_init");
        closelog();
        shutdown(SOCKET, SHUT_RDWR);
        exit(EXIT_FAILURE);
    }

    struct pollfd pollDescriptors[1];
    pollDescriptors[0].fd = SOCKET;
    pollDescriptors[0].events = POLLIN;

    // Set up linked list to keep track of child threads

    SLIST_HEAD(slisthead, slist_data_s);
    struct slisthead head;
    SLIST_INIT(&head);

    // Set up signal mask so that ppoll can be interrupted without error
    sigset_t signalMask;
    sigemptyset(&signalMask);
    sigaddset(&signalMask, SIGINT);
    sigaddset(&signalMask, SIGTERM);

    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 100;

    time_t lastOutputTime;
    time(&lastOutputTime);
    while (CONTINUE_EXECUTING)
    {
        int pollCount = ppoll(pollDescriptors, 1, &timeout, &signalMask);

        if (pollCount < 0)
        {
            perror("poll");
            break;
        }

        if (pollCount > 0)
        {
            int newSocket = acceptNewConnection(SOCKET);
            struct thread_data threadData = {.outputFileMutex = &outputFileMutex, .completed = false, .socket = newSocket};
            printf("Connection accepted!\n");
            pthread_t thread;
            if (pthread_create(&thread, NULL, handleNewConnection, &threadData) != 0)
            {
                perror("pthread_create");
                break;
            }
            printf("Created thread %lu!\n", thread);
            threadData.thread = thread;
            // handleNewConnection((void *)&threadData);
            slist_data_s *threadNode = malloc(sizeof(slist_data_s));
            threadNode->data = &threadData;
            SLIST_INSERT_HEAD(&head, threadNode, nodes);
            printf("Created thread %lu!\n", threadNode->data->thread);
        }

        if (pollCount > 0)
        {
            printf("Out of scope: %lu!\n", SLIST_FIRST(&head)->data->thread);
        }

        slist_data_s *node, *tmp;
        SLIST_FOREACH_SAFE(node, &head, nodes, tmp)
        {
            // printf("Checking thread %lu!\n", node->data->thread);
            if ((*(node->data)).completed)
            {
                // Remove node from linked list and free memory
                pthread_t thread = (*(node->data)).thread;
                printf("Joining thread %lu!\n", thread);
                pthread_join(thread, NULL);
                SLIST_REMOVE(&head, node, slist_data_s, nodes);
                printf("Thread completed: %lu\n", thread);
                free(node);
            }
        }

        time_t now;
        time(&now);
        struct tm *timeInfo;
        timeInfo = localtime(&now);
        if (now - lastOutputTime >= 10)
        {
            const int STRING_SIZE = 80;
            char buffer[STRING_SIZE];
            memset(buffer, 0, STRING_SIZE);
            strftime(buffer, sizeof(buffer), "timestamp:%a, %d %b %Y %T %z\n", timeInfo);
            writeToFile(buffer, &outputFileMutex);
            lastOutputTime = now;
        }
    }

    slist_data_s *node, *tmp;
    SLIST_FOREACH_SAFE(node, &head, nodes, tmp)
    {
        // Remove nodes from linked list and free memory
        printf("Joining thread!\n");
        pthread_join((*(node->data)).thread, 0);
        SLIST_REMOVE(&head, node, slist_data_s, nodes);
        free(node->data);
        free(node);
    }

    closelog();
    shutdown(SOCKET, SHUT_RDWR);
    remove(OUTPUT_FILE);
    return 0;
}