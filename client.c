/* 	
	Authors: Rishabh Pahwa
			 Student ID: 110091353
			 Email: pahwar@uwindsor.ca
			 
			 Tanmay Verma
			 Student ID: 110089806
			 Email: verma46@uwindsor.ca
			 
	Title: COMP8567: Advanced Systems Programming - Final Project 
	
	Submitted to: Dr. Prashanth Ranga
*/
#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

#define DEBUG_MODE 0
#define SERVER_PORT 49152
#define MAX_ARGUMENTS 10
#define BUFFER_SIZE 1024
#define RESPONSE_TEXT 1
#define RESPONSE_STRUCT 2
#define RESPONSE_FILE 3

#if DEBUG_MODE == 0
#define DEBUG_LOG(fmt, ...) printf("[LOG] " fmt, ##__VA_ARGS__)
#else
#define DEBUG_LOG(fmt, ...)
#endif


// Struct AddressInfo to tranfer Mirror IP & Port no. 
typedef struct {
    char ip_address[INET_ADDRSTRLEN];
    int port_number;
} AddressInfo;

// Flag for Unzip and Quit function
int flagUnzip = 0, flagQuit = 0, fileFound = 0;

// Validate Input comand from user
int validateInput(char* buffer);

// Check for Unzip option
void checkUnzip(char buffer[]);

// Remove Line break from input
void removeLineBreak(char buffer[]);

// Validate input date
int validateDate(char date[]);

// Validate input size
int isAllDigits(char* str);

// Generate progress-bar for file transmission
void generateProgressBar(int totalSize, int bytesReceived, time_t startTime);

// Main Function
int main(int argc, char *argv[]) {
    int sock = 0, valread = 0;
    struct sockaddr_in serverAddress;
	struct sockaddr_in mirrorAddress;
    char commandBuffer[BUFFER_SIZE] = {0};
	char responseText[BUFFER_SIZE];
	char responseFile[BUFFER_SIZE];
	char valbuf[BUFFER_SIZE];
    char server_ip[16];
    char *filename = "out.tar.gz";

    if (argc < 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }
    strcpy(server_ip, argv[1]);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error in creating socket\n");
        return 1;
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, server_ip, &serverAddress.sin_addr) <= 0) {
        printf("Invalid Address or Address not supported\n");
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        printf("Connection failed\n");
        return 1;
    }
	
	long initialResponse = 0;
	recv(sock, &initialResponse, sizeof(initialResponse), 0);
	
	DEBUG_LOG("Initial Response type: %ld\n", initialResponse);
	
	// Server sends text (case: normal connection)
	if(initialResponse == RESPONSE_TEXT){
		printf("Connected to the server!\n");
		
		// Read greeting text
		memset(responseText, 0, sizeof(responseText)); // Clear the response text buffer
		read(sock, responseText, sizeof(responseText));
		printf("Message from server: %s\n", responseText);
	}
	
	// Server sends structure (case: redirection)
	else if(initialResponse == RESPONSE_STRUCT)
	{	
		printf("Server redirected to Mirror Server.\n");
		
		AddressInfo mirrorInfo;
		
		// Read structure containing Mirror IP address & Port
		recv(sock, &mirrorInfo, sizeof(AddressInfo), 0);
		
		// Close connection to Main Server
		close(sock);
		
		DEBUG_LOG("Server sent mirror IP: %s\n", mirrorInfo.ip_address);
		DEBUG_LOG("Server sent mirror Port: %d\n", mirrorInfo.port_number);
		
		// Create a new socket
		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0) {
			printf("Failed to create socket\n");
			return 1;
		}
		
		mirrorAddress.sin_family = AF_INET;
		mirrorAddress.sin_port = htons(mirrorInfo.port_number);
		if (inet_pton(AF_INET, mirrorInfo.ip_address, &mirrorAddress.sin_addr) <= 0) {
			printf("Invalid mirror address.\n");
			return 1;
		}
		
		// Connect to Mirror Server
		if (connect(sock, (struct sockaddr *)&mirrorAddress, sizeof(mirrorAddress)) < 0) {
			printf("Connection to mirror failed.\n");
			return 1;
		}
		
		printf("Connected to the mirror server!\n");
		
		// Read greeting message from Mirror Server
		memset(responseText, 0, sizeof(responseText)); // Clear the response text buffer
		read(sock, responseText, sizeof(responseText));
		printf("Message from server: %s\n", responseText);
	}
	
	// Main process to interact with Server or Mirror
    while (1) {
		
		// Clear the command buffer
		memset(commandBuffer, 0, sizeof(commandBuffer)); 
		fileFound = 0;
		
        printf("\nEnter command: ");
        fgets(commandBuffer, 1024, stdin);
		
		// Clean linebreak from input stream 
		removeLineBreak(commandBuffer);
		
		// Check for unzip option
		checkUnzip(commandBuffer);
		
		// Validation on input
		strcpy(valbuf, commandBuffer);
		if(validateInput(valbuf))
			continue;
		
        // send command to server
        send(sock, commandBuffer, strlen(commandBuffer), 0);

		// Receive header to identify response type
        long header;
		valread = read(sock, &header, sizeof(header));

        if (valread > 0) {
			
			// Text response
			if(header == RESPONSE_TEXT){
				
				memset(responseText, 0, sizeof(responseText)); // Clear the response text buffer
				
				// Read and output text response
				read(sock, responseText, sizeof(responseText));
				printf("Received text response: %s\n", responseText);
			}
			
			// File Response
			else{
				
				fileFound = 1;
				
				// File size obtained from header
				long fileSize = header;
				
				memset(responseFile, 0, sizeof(responseFile)); // Clear the response file buffer
				
				FILE *fp = fopen(filename, "wb");
				if (fp == NULL) {
					printf("Error creating physical file.\n");
					return 1;
				}
				
				time_t startTime;
				time(&startTime);
				
				// Receive file data from server
				long total_bytes_received = 0;
				DEBUG_LOG("File Size: %ld Bytes\n",fileSize);
				while (total_bytes_received < fileSize) {
					generateProgressBar(fileSize, total_bytes_received, startTime);
					int bytes_to_receive = BUFFER_SIZE;
					if (total_bytes_received + BUFFER_SIZE > fileSize) {
						bytes_to_receive = fileSize - total_bytes_received;
					}
					int bytes_received = recv(sock, responseFile, bytes_to_receive, 0);
					if (bytes_received < 0) {
						printf("Error receiving file data");
						return 1;
					}
					fwrite(responseFile, sizeof(char), bytes_received, fp);
					total_bytes_received += bytes_received;
					if (total_bytes_received >= fileSize) {
						break;
					}
				}
				generateProgressBar(fileSize, total_bytes_received, startTime);
				printf("\nReceived file: %s\n", filename);
				fclose(fp);
			}
			
            
        } else {
			// Read 0 bytes from server ie. disconnected 
            printf("Server disconnected\n");
            break;
        }
		
		// Handle command quit 
		if(flagQuit){
			printf("Bye.\n");
			break;
		}
		
		// Handle unzip option <-u> on client side
		if (flagUnzip && fileFound) {
			char cmd[BUFFER_SIZE];
			snprintf(cmd, BUFFER_SIZE, "tar -xzf %s", filename);
			system(cmd);
			printf("File unzipped successfully\n");
			flagUnzip = 0;
		}
    }
	
	// Close connection to Server / Mirror
    close(sock);
    return 0;
}

void generateProgressBar(int totalSize, int bytesReceived, time_t startTime) {
    int progress = (int)((float)bytesReceived / (float)totalSize * 100);
    int numFilledBlocks = (int)((float)progress / 5);
    int numEmptyBlocks = 20 - numFilledBlocks;
    time_t currentTime;
    time(&currentTime);
    double timeDiff = difftime(currentTime, startTime);
    double transferRate = (double)bytesReceived / timeDiff;
    printf("[");
    for (int i = 0; i < numFilledBlocks; i++) {
        printf("#");
    }
    for (int i = 0; i < numEmptyBlocks; i++) {
        printf(" ");
    }
    printf("] %d%% %.2f KB/s", progress, transferRate / 1024);
	fflush(stdout);
	printf("\r"); // move cursor to beginning of line
}


int validateInput(char* commandBuffer){

	// Tokenize input
	char *arguments[MAX_ARGUMENTS];
	int num_arguments = 0;

	// Parse the input commands
	char* token = strtok(commandBuffer, " "); 
	while (token != NULL) {
		arguments[num_arguments++] = token; 
		token = strtok(NULL, " "); 
	}
	
	// Set the last element of the array to NULL
	arguments[num_arguments] = NULL; 
	
	// Check if the command entered is valid
	if (strcmp(arguments[0], "findfile") != 0 &&
	    strcmp(arguments[0], "sgetfiles") != 0 &&
	    strcmp(arguments[0], "dgetfiles") != 0 &&
	    strcmp(arguments[0], "getfiles") != 0 &&
	    strcmp(arguments[0], "gettargz") != 0 &&
	    strcmp(arguments[0], "quit") != 0) {
		printf("Invalid command, try again\n");
		return 1;
	}
	
	// Check that arguments are given along with the command 
	if(num_arguments < 2 && strcmp(arguments[0], "quit") != 0){
		printf("Enter arguments along with command, try again!\n");
		return 1;
	}
	// Check that arguments do not exceed maximum possible
	else if(num_arguments > 7){
		printf("Too many arguments entered, try again!\n");
		return 1;
	}
	
	// Validate arguments for command : findfile
	if( strcmp(arguments[0], "findfile") == 0 && num_arguments > 2){
		printf("Too many arguments entered, try again!\n");
		return 1;
	}
	
	
	// Validate arguments for command : sgetfiles
	if (strcmp(arguments[0], "sgetfiles") == 0) {
		
		if(num_arguments > 3){
			printf("Too many arguments entered, try again!\n");
			printf("Usage: sgetfiles size1 size2 <-u> \n");
			return 1;
		}
		
        if (isAllDigits(arguments[1]) == 0) {
            printf("Size 1 entered is incorrect, please try again!\n");
			printf("Usage: sgetfiles size1 size2 <-u> \n");
            return 1;
        }
        if (isAllDigits(arguments[2]) == 0) {
            printf("Size 2 entered is incorrect, please try again!\n");
			printf("Usage: sgetfiles size1 size2 <-u> \n");
            return 1;
        }

        // Compare dates
        if (atoi(arguments[1]) > atoi(arguments[2])) {
            printf("Size 1 cannot be greater than Size 2!\n");
            return 1;
        }
    }
	
	// Validate arguments for command : dgetfiles
	if (strcmp(arguments[0], "dgetfiles") == 0) {
		if(num_arguments > 3){
			printf("Too many arguments entered, try again!\n");
			printf("Usage: dgetfiles date1 date2 <-u>\n");
			return 1;
		}
        if (validateDate(arguments[1]) == 0) {
            printf("Date 1 entered is incorrect, please try again!\n");
			printf("Usage: dgetfiles date1 date2 <-u>\n");
            return 1;
        }
        if (validateDate(arguments[2]) == 0) {
            printf("Date 2 entered is incorrect, please try again!\n");
			printf("Usage: dgetfiles date1 date2 <-u>\n");
            return 1;
        }

		struct tm tm1 = { 0 };
		int year, month, day;
		if (sscanf(arguments[1], "%d-%d-%d", &year, &month, &day) == 3) {
			tm1.tm_year = year - 1900;
			tm1.tm_mon = month - 1;
			tm1.tm_mday = day;
		}
		time_t date1 = mktime(&tm1);
		
		struct tm tm2 = { 0 };
		if (sscanf(arguments[2], "%d-%d-%d", &year, &month, &day) == 3) {
			tm2.tm_year = year - 1900;
			tm2.tm_mon = month - 1;
			tm2.tm_mday = day;
		}
		time_t date2 = mktime(&tm2);
		
		// Compare dates
		if (date1 > date2) {
			printf("Date 1 cannot be greater than Date 2!\n");
			return 1;
		}
    }	
	
	// Prevent unzip with findfile and quit
	if(( strcmp(arguments[0], "findfile") == 0 ||
			strcmp(arguments[0], "quit") == 0 ) && 
			flagUnzip == 1){
		flagUnzip = 0;
		flagQuit = 0;
		printf("Nothing to Unzip: <-u> cannot be used with 'findfile' or 'quit'.\n");
		return 1;
	}
	
	// Flag quit
	if (strcmp(arguments[0], "quit") == 0) {
		flagQuit = 1;
	}

return 0;
	
}

void checkUnzip(char buffer[]) {
    int length = strlen(buffer);
    if (length >= 3 && strcmp(buffer + length - 3, " -u") == 0) {
        flagUnzip = 1;
        buffer[length - 3] = '\0';
    }
}

void removeLineBreak(char buffer[]) {
    int length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
    }
}

int validateDate(char date[]) {
    int year, month, day;

    // Parse year, month, and day from the input string
    if (sscanf(date, "%d-%d-%d", &year, &month, &day) != 3) {
        // Failed to parse year, month, and day
        return 0;
    }

    // Check if year is between 1000 and 9999
    if (year < 1000 || year > 9999) {
        return 0;
    }

    // Check if month is between 1 and 12
    if (month < 1 || month > 12) {
        return 0;
    }

    // Check if day is between 1 and 31 (depending on the month)
    if (day < 1 || day > 31) {
        return 0;
    }

    // Check for months with 30 days
    if ((month == 4 || month == 6 || month == 9 || month == 11) && day > 30) {
        return 0;
    }

    // Check for February
    if (month == 2) {
        // Leap year
        if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
            if (day > 29) {
                return 0;
            }
        }
        // Non-leap year
        else {
            if (day > 28) {
                return 0;
            }
        }
    }

    // If all checks pass, the date is valid
    return 1;
}

int isAllDigits(char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (!isdigit(str[i])) {
            return 0;  // Not all characters are digits
        }
    }
    return 1;  // All characters are digits
}

