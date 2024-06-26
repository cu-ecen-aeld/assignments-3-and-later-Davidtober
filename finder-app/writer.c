#include <syslog.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);
    if (argc <= 2)
    {
        syslog(LOG_ERR, "Not enough arguments provided\n");
        return 1;
    }

    char *writeFile = argv[1];
    char *writeString = argv[2];

    syslog(LOG_DEBUG, "Writing %s to %s\n", writeString, writeFile);

    FILE *filePointer = fopen(writeFile, "w");

    if (filePointer == NULL)
    {
        syslog(LOG_ERR, "Could not open file.\n");
        closelog();
        return 1;
    }
    int numBytes = fprintf(filePointer, "%s\n", writeString);

    if (numBytes <= 0)
    {
        syslog(LOG_ERR, "Failed to write to file.\n");
        closelog();
        fclose(filePointer);
        return 1;
    }

    fclose(filePointer);
    closelog();
    return 0;
}