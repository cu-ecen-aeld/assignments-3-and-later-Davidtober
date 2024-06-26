#include <syslog.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
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
        return 1;
    }
    fprintf(filePointer, "%s\n", writeString);

    return 0;
}