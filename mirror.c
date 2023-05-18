#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <ftw.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DEBUG_MODE 1
#define SERVER_PORT 32000
#define MAX_RESPONSE_SIZE 1024
#define MAX_ARGUMENTS 7
#define MAX_BUFFER_SIZE 1024
#define MAX_EXTENSION_COUNT 6
#define RESPONSE_TEXT 1
#define RESPONSE_STRUCT 2
#define RESPONSE_FILE 3
#define HOME_DIR getenv("HOME")
#define TEMP_TAR "temp.tar.gz"
#define TEMP_FILELIST "temp_filelist.txt"

#if DEBUG_MODE == 1
#define DEBUG_LOG(fmt, ...) printf("[LOG] " fmt, ##__VA_ARGS__)
#else
#define DEBUG_LOG(fmt, ...)
#endif

// Struct AddressInfo to tranfer Mirror IP & Port no. 
typedef struct {
    char ip_address[INET_ADDRSTRLEN];
    int port_number;
} AddressInfo; 

// Global Variables
int client_no = 0;

// Function to handle client commands
void processClient(int clientSockfd);

// Functions to handle various commands
void findfile(int clientSockfd, char** arguments);
int dgetfiles(int clientSockfd, char** arguments);
int sgetfiles(int clientSockfd, char** arguments);
int getfiles(int clientSockfd, char** arguments, int argLen);
int gettargz(int clientSockfd, char** arguments, int extension_count);

// Transmit data to client
int sendFileResponse(int clientSockfd, const char* filename);
int sendTextResponse( int clientSockfd, char* buffer);

// Functions for recursive file tree search
int recursiveSearchExt(char *dir_name, char **file_types, int extension_count, int *fileCount);
int recursiveSearchDate(char *root_path, time_t date1, time_t date2, int *fileCount);
int recursiveSearchName(char *dir_name, char **file_names, int filenameCount, int *fileCount);
int recursiveSearchSize(char *dir_name, int size1, int size2, int *fileCount);

// Function to convert input time to unix time
time_t convertDateToUnixTime(const char *time_str, int dateType);

// Function to remove line break from buffer
void removeLineBreak(char buffer[]);

// Main Function
int main(int argc, char *argv[])
{
	int sd, csd, portNumber, status;
	socklen_t len;
	struct sockaddr_in servAdd;	//ipv4
	
	// Create Socket
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("[-] Error Creating Socket\n");
		exit(1);
	}

	servAdd.sin_family = AF_INET;
	servAdd.sin_addr.s_addr = htonl(INADDR_ANY);	
	servAdd.sin_port = htons(SERVER_PORT);

	bind(sd, (struct sockaddr *) &servAdd, sizeof(servAdd));
	listen(sd, 5);
	printf("[~] Listening on Port %d\n", SERVER_PORT);
	while (1)
	{	
		// Increment client number 
		client_no++;
		
		// Connect 
		csd = accept(sd, (struct sockaddr *) NULL, NULL);
		printf("[+] Client %d connected\n", client_no);
		
		// Fork a child process to handle client request 
		if (!fork()){	 
			processClient(csd);
			close(csd);
			exit(0);
		}
		waitpid(0, &status, WNOHANG);

	}
}	

void processClient(int client_sockfd)
{
	char commandBuffer[MAX_BUFFER_SIZE];
	char responseText[MAX_BUFFER_SIZE];
	int bytesRead = 0;
	
	// Use snprintf() to format the message into the buffer
	snprintf(responseText, sizeof(responseText), "Send commands now");
	send(client_sockfd, responseText, strlen(responseText), 0);

	while (1)
	{	
		
		// Clear the command buffer
		memset(commandBuffer, 0, sizeof(commandBuffer)); 
		
		bytesRead = read(client_sockfd, commandBuffer, MAX_BUFFER_SIZE);
		removeLineBreak(commandBuffer);
		
		// Check if no data received (client closed connection)
		if (bytesRead <= 0) { 
           printf("[-] Client %d disconnected.\n", client_no);
           break; 
		}
		
		DEBUG_LOG("Input from client: %s\n", commandBuffer);
		
		char *arguments[MAX_ARGUMENTS];
		int num_arguments = 0;
		
		// Parse the command received from client
		char* token = strtok(commandBuffer, " "); // Tokenize command using space as delimiter
		char* cmd = token; // Store the first token in cmd
		
		while (token != NULL) {
			token = strtok(NULL, " "); // Get the next token
			if (token != NULL) { // Check if token is not NULL before storing it
				arguments[num_arguments++] = token; // Store the token in the array
			}
		}
		arguments[num_arguments] = NULL; // Set the last element of the array to NULL	
		
		DEBUG_LOG("Parsed command: %s\n",cmd);
		
		// Process the command and generate response
		if (strcmp(cmd, "findfile") == 0)
		{
			// Call the function to handle request
			findfile(client_sockfd, arguments);
		}
		else if (strcmp(cmd, "sgetfiles") == 0)
		{
			// Call the function to handle request
			int resultsgetfiles = sgetfiles(client_sockfd, arguments);
			
			// Check the result of the function call
               if (resultsgetfiles == 1) {
					sendTextResponse(client_sockfd, "An error occurred.");
					printf("An error occurred in process: sgetfiles\n");
				}
		}
		else if (strcmp(cmd, "dgetfiles") == 0)
           {
			   // Call the function to handle request
               int resultdgetfiles = dgetfiles(client_sockfd, arguments);
			   
			   // Check the result of the function call
               if (resultdgetfiles == 1) {
					sendTextResponse(client_sockfd, "An error occurred.");
					printf("An error occurred in process: dgetfiles\n");
				}
		}
		else if (strcmp(cmd, "getfiles") == 0)
		{	
			// Call the function to handle request
			int resultgetfiles = getfiles(client_sockfd, arguments, num_arguments);
			
			// Check the result of the function call
			if (resultgetfiles == 1) {
				sendTextResponse(client_sockfd, "An error occurred.");
				printf("An error occurred in process: getfiles\n");
			}
		}
		else if (strcmp(cmd, "gettargz") == 0)
		{
   			// Call the function to handle request
			int result = gettargz(client_sockfd, arguments, num_arguments);

			// Check the result of the function call
			if (result == 1) {
				sendTextResponse(client_sockfd, "An error occurred.");
				printf("An error occurred in process: gettargz\n");
			}

		}

		else if (strcmp(cmd, "quit") == 0)
		{		
			printf("[~]Client requested to quit.\n");
	
			// Acknoledge quit request and close the socket
			sendTextResponse(client_sockfd, "Server has closed the connection.	");
            close(client_sockfd); 
			printf("[-]Client socket closed.\n");
            break;
            exit(0); 
		}
		
		else
		{	
			// Invalid command, inform client 
			sendTextResponse(client_sockfd, "Invalid command, try again\n");
			continue;	// Continue to next iteration of loop to wait for new command
		}
	}
}


int sendTextResponse(int clientSockfd, char* responseText){
	
	// Send response type = text response
	long responseType = RESPONSE_TEXT;
	send(clientSockfd, &responseType, sizeof(responseType), 0);
	
	DEBUG_LOG("Sending text response to client %d: %s\n", client_no, responseText);
	// Send response text to Client
	send(clientSockfd, responseText, strlen(responseText), 0);
	
	return 0;
}

int sendFileResponse(int new_socket, const char* filename) {
    // Open file to be transferred
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open failed");
        exit(EXIT_FAILURE);
    }

    char buffer[MAX_RESPONSE_SIZE];
    ssize_t valread;

    // Read file contents and send to client
    while ((valread = read(fd, buffer, 1024)) > 0) {
        send(new_socket, buffer, valread, 0);
    }

    // Close file and socket
    close(fd);

    return 0;
}
	

void findfile(int client_sockfd, char** arguments){
	
	// filename is first argument
	char* filename = arguments[0];
	
	DEBUG_LOG("Starting findfile process\n");
	char response[MAX_RESPONSE_SIZE];
	DEBUG_LOG("Filename: %s\n", filename);
					
    char* home_dir = HOME_DIR; // Get the home directory path
    char* command = (char*) malloc(strlen(home_dir) + strlen(filename) + 27); // Allocate memory for the command string
    sprintf(command, "find %s -name '%s' -print -quit", home_dir, filename); // Construct the find command
	DEBUG_LOG("Executing command: %s\n", command);
    FILE* pipe = popen(command, "r"); // Open a pipe to the command
    if (pipe != NULL) {
        char line[256];
        if (fgets(line, sizeof(line), pipe) != NULL) { // Read the first line of output
            line[strcspn(line, "\n")] = '\0'; // Remove the newline character from the end of the line
            struct stat sb;
            if (stat(line, &sb) == 0) { // Get the file information using stat()
				time_t filetime;
				#ifdef __APPLE__
					filetime = sb.st_birthtime;
				#else
					filetime = sb.st_mtime;
				#endif
				char* time = ctime(&filetime);
				removeLineBreak(time);
                sprintf(response, "%s (%lld bytes, created %s)", line, (long long) sb.st_size, time); // Print the file information
            } else {
                sprintf(response,"Unable to get file information for %s", line);
            }
        } else {
            sprintf(response,"File not found.");
        }
        pclose(pipe); // Close the pipe
    } else {
        DEBUG_LOG("Error opening pipe to command\n");
    }
    free(command); // Free the memory allocated for the command string
	
	// Send response to client
	sendTextResponse(client_sockfd, response);
			
}

int recursiveSearchExt(char *dir_name, char **file_types, int extension_count, int *fileCount) {
    DIR *dir;
    struct dirent *ent;
    char buffer[MAX_BUFFER_SIZE];
    int status;
    int i;
    FILE *fp;

    DEBUG_LOG("Start recursive search\n");

    if ((dir = opendir(dir_name)) == NULL) {
        perror("opendir failed");
        return 1;
    }

    fp = fopen(TEMP_FILELIST, "a");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file\n");
        return 1;
    }

    while ((ent = readdir(dir)) != NULL) {

        if (ent->d_type == DT_DIR && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
            if (dir_name[strlen(dir_name) - 1] == '/') {
                sprintf(buffer, "%s%s", dir_name, ent->d_name);
            } else {
                sprintf(buffer, "%s/%s", dir_name, ent->d_name);
            }

            recursiveSearchExt(buffer, file_types, extension_count, fileCount);
        } else {
            if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;
            DEBUG_LOG("Readdir: %s\n", ent->d_name);
            for (i = 0; i < extension_count; i++) {
                DEBUG_LOG("Matching extension [%s] with [%s]\n", file_types[i], ent->d_name);
                if (fnmatch(file_types[i], ent->d_name, FNM_PATHNAME) == 0) {
                    if (dir_name[strlen(dir_name) - 1] == '/') {
                        sprintf(buffer, "%s%s", dir_name, ent->d_name);
                    } else {
                        sprintf(buffer, "%s/%s", dir_name, ent->d_name);
                    }
                    fprintf(fp, "%s\n", buffer);
					DEBUG_LOG("Saving to %s: %s \n", TEMP_FILELIST, buffer);
                    *fileCount+=1;
                    break;
                }
            }
        }
    }
    closedir(dir);
    fclose(fp);

    return 0;
}


int gettargz(int client_sockfd, char **extensions, int extension_count) {
    DIR *dir;
    struct dirent *ent;
    char *home_dir = HOME_DIR;
    char buffer[MAX_BUFFER_SIZE];
    char *tar_command_format = "tar -czf %s -T %s";
    int status, i;
	char *file_types[extension_count];
	
    DEBUG_LOG("Home directory: %s\n", home_dir);
    DEBUG_LOG("%d Extensions parsed:", extension_count);

    for (i = 0; i < extension_count; i++) {
        DEBUG_LOG(" [%s]", extensions[i]);
    }
	DEBUG_LOG("\n");
	
    if ((dir = opendir(home_dir)) == NULL) {
        DEBUG_LOG("opendir failed");
        return 1;
    }
    for (i = 0; i < extension_count; i++) {
        file_types[i] = malloc(strlen(extensions[i]) + 2);
        sprintf(file_types[i], "*.%s", extensions[i]);
    }
    int fileCount = 0;
    if (recursiveSearchExt(home_dir, file_types, extension_count, &fileCount) != 0) {
		DEBUG_LOG("Error findind files");
        return 1;
    }
	
	DEBUG_LOG("Files found: %d\n", fileCount);
	
    if (fileCount == 0) {
		
		// Send text response
		DEBUG_LOG("No files found.\n");
		sendTextResponse(client_sockfd, "No files found.");
        return 0;
    }
    sprintf(buffer, tar_command_format, TEMP_TAR, TEMP_FILELIST);
    DEBUG_LOG("Executing command: %s\n", buffer);
    status = system(buffer);
    if (status != 0) {
        DEBUG_LOG("Error creating tar file\n");
        return 1;
    }
    // Get the size of the tar file
    FILE *fp;
    long int file_size;
    fp = fopen(TEMP_TAR, "rb");
    if (fp == NULL) {
        DEBUG_LOG("Error opening file\n");
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
	
    // Send the size of the file as a long int
    send(client_sockfd, &file_size, sizeof(file_size), 0);
	
    // Transfer the file
    if (sendFileResponse(client_sockfd, TEMP_TAR) != 0) {
        DEBUG_LOG("Error transferring file\n");
		return 1;
    }
    else {
        DEBUG_LOG("File transferred successfully\n");
    }
    fclose(fp);
	
	// Delete temp_filelist.txt
    status = remove(TEMP_FILELIST);
    if (status != 0) {
        DEBUG_LOG("Error deleting temp_filelist.txt\n");
    }
    
    // Delete temp.tar.gz
    status = remove(TEMP_TAR);
    if (status != 0) {
        DEBUG_LOG("Error deleting temp.tar.gz\n");
    }
	
    return 0;
}

int recursiveSearchName(char *dir_name, char **file_names, int file_count, int *fileCount) {
    DIR *dir;
    struct dirent *ent;
    char buffer[MAX_BUFFER_SIZE];
    int status;
    int i;
	FILE *fp;
	
	DEBUG_LOG("Start recursive search\n");
	
    if ((dir = opendir(dir_name)) == NULL) {
        perror("opendir failed");
        return 1;
    }
	
	fp = fopen(TEMP_FILELIST, "a");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file\n");
        return 1;
    }
	
    while ((ent = readdir(dir)) != NULL) {
		
        if (ent->d_type == DT_DIR && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
            if (dir_name[strlen(dir_name) - 1] == '/') {
				sprintf(buffer, "%s%s", dir_name, ent->d_name);
			} else {
				sprintf(buffer, "%s/%s", dir_name, ent->d_name);
			}
			
            recursiveSearchExt(buffer, file_names, file_count, fileCount);
        } else {
			if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
				continue;
			DEBUG_LOG("Readdir: %s\n", ent->d_name);
            for (i = 0; i < file_count; i++) {
				DEBUG_LOG("Matching input file [%s] with physical file [%s]\n", file_names[i], ent->d_name);
                if (strcmp(file_names[i], ent->d_name) == 0) {

					if (dir_name[strlen(dir_name) - 1] == '/') {
						sprintf(buffer, "%s%s", dir_name, ent->d_name);
					} else {
						sprintf(buffer, "%s/%s", dir_name, ent->d_name);
					}
					fprintf(fp, "%s\n", buffer);
					DEBUG_LOG("Saving to %s: %s \n", TEMP_FILELIST, buffer);
                    *fileCount+=1;
                    break;
                }
            }
        }
    }
    closedir(dir);
	fclose(fp);
	
    return 0;
}

int getfiles(int client_fd, char **filenames, int filenamecount)
{	
	DIR *dir;
    char *tar_cmd = malloc(sizeof(char) * MAX_BUFFER_SIZE);
	char *tar_command_format = "tar -czf %s -T %s";
    char *home_dir = HOME_DIR;
	int i, status;
    char files[MAX_BUFFER_SIZE-21];
	
    memset(files, 0, sizeof(files));
	
    DEBUG_LOG("Home directory: %s\n", home_dir);
    DEBUG_LOG("%d File names parsed:", filenamecount);

    for (i = 0; i < filenamecount; i++) {
        DEBUG_LOG(" [%s]", filenames[i]);
    }
	DEBUG_LOG("\n");
	
    if ((dir = opendir(home_dir)) == NULL) {
        DEBUG_LOG("opendir failed");
        return 1;
    }

    int fileCount = 0;
    if (recursiveSearchName(home_dir, filenames, filenamecount, &fileCount) != 0) {
		DEBUG_LOG("Error findind files");
        return 1;
    }

    if (fileCount == 0)
    {
        // None of the files exist
		sendTextResponse(client_fd, "No file found");
        return 0;
    }

    // Create tar command to compress files into temp.tar.gz
    sprintf(tar_cmd, tar_command_format, TEMP_TAR, TEMP_FILELIST);;
	
	DEBUG_LOG("Files found: %d\n", fileCount);
    DEBUG_LOG("Executing command: %s\n", tar_cmd);

    // Execute tar command
    system(tar_cmd);

    // Send compressed file to client
    FILE *fileptr;
    char *buffer;
    long filelen;
    long int file_size_getfiles;

    fileptr = fopen(TEMP_TAR, "rb"); // Open the file in binary mode

    if (fileptr == NULL)
    {
        DEBUG_LOG("Error opening file\n");
        return 1;
    }

    fseek(fileptr, 0, SEEK_END);
    file_size_getfiles = ftell(fileptr);
    fseek(fileptr, 0, SEEK_SET);

    buffer = (char *)malloc((filelen + 1) * sizeof(char)); // Allocate memory for the file data
    fread(buffer, filelen, 1, fileptr);                   // Read the file data into the buffer

    // Send the size of the file as a long int
    send(client_fd, &file_size_getfiles, sizeof(file_size_getfiles), 0);
    // Transfer the file
    int file_trans_result = sendFileResponse(client_fd, TEMP_TAR);
    if (file_trans_result != 0)
    {
        DEBUG_LOG("Error transferring file\n");
		return 1;
    }
    else
    {
        DEBUG_LOG("File transferred successfully\n");
    }
	
	// Delete temp_filelist.txt
    status = remove(TEMP_FILELIST);
    if (status != 0) {
        DEBUG_LOG("Error deleting temp_filelist.txt\n");
        return 0;
    }
    
    // Delete temp.tar.gz
    status = remove(TEMP_TAR);
    if (status != 0) {
        DEBUG_LOG("Error deleting temp.tar.gz\n");
        return 0;
    }

    fclose(fileptr); // Close the file

    return 0;
}

int recursiveSearchSize(char *dir_name, int size1, int size2, int *fileCount)
{

    DIR *dir;
    struct dirent *ent;
    char buffer[MAX_BUFFER_SIZE];
	FILE *fp;
	
	// Clear the buffer
	memset(buffer, 0, sizeof(buffer));
	
	DEBUG_LOG("Start recursive search by Size\n");
	
    // open root directory
    if ((dir = opendir(dir_name)) == NULL) {
        DEBUG_LOG("Error opening directory.\n");
        return 1;
    }
	
	// open temporary file list
	fp = fopen(TEMP_FILELIST, "a");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file\n");
        return 1;
    }
    
     while ((ent = readdir(dir)) != NULL)
    {
        // skip . and ..
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
        {
            continue;
        }
		
		DEBUG_LOG("Readdir: %s\n", ent->d_name);
		
		if (dir_name[strlen(dir_name) - 1] == '/') {
				sprintf(buffer, "%s%s", dir_name, ent->d_name);
		} else {
			sprintf(buffer, "%s/%s", dir_name, ent->d_name);
		}
		
        // get file/directory stats
        struct stat statbuf;
        if (stat(buffer, &statbuf) == -1)
        {
            continue;
        }
		
		// if directory, recursively search it
		if (ent->d_type == DT_DIR)
        {
            recursiveSearchSize(buffer, size1, size2, fileCount);
        }
        // if file, check if it's between size 1 and size 2 and print name
       else 
       {
            DEBUG_LOG("File %s with Size %ld\n", ent->d_name, statbuf.st_size);

        // check if file size is between size1 and size2
           if (S_ISREG(statbuf.st_mode) && statbuf.st_size >= size1 && statbuf.st_size <= size2 && size1 <= size2) 
           {
             
				if (dir_name[strlen(dir_name) - 1] == '/') {
					sprintf(buffer, "%s%s", dir_name, ent->d_name);
				} else {
					sprintf(buffer, "%s/%s", dir_name, ent->d_name);
				}
				
				fprintf(fp, "%s\n", buffer);
				DEBUG_LOG("Saving to %s: %s \n", TEMP_FILELIST, buffer);
				*fileCount+=1;

           }
        }
    }
    closedir(dir);
	fclose(fp);
    return 0;

}

int sgetfiles(int client_sockfd, char** arguments) {

    size_t size1 = atoi(arguments[0]);
    size_t size2 = atoi(arguments[1]);
    char *root_path = HOME_DIR;	// get home directory path
    char buffer[MAX_BUFFER_SIZE];
	char *tar_command_format = "tar -czf %s -T %s";
	int status, fileCount = 0;
	
	DEBUG_LOG("Starting sgetfiles process.\n");
	DEBUG_LOG("Size 1: %ld\n",size1);
	DEBUG_LOG("Size 2: %ld\n",size2);
	
	// Get files
    recursiveSearchSize(root_path, size1, size2, &fileCount);
    
    DEBUG_LOG("Files found: %d\n",fileCount);
    sprintf(buffer, tar_command_format, TEMP_TAR, TEMP_FILELIST);

    if (fileCount > 0) {
		DEBUG_LOG("Executing: %s\n", buffer);
        int status = system(buffer);
        if (status == 0) {
            DEBUG_LOG("Command executed successfully\n");
            // Send the compressed file to the client
            FILE *fileptr;
            char *buffer;
            long file_size;
            long int file_size_getfiles;

            fileptr = fopen(TEMP_TAR, "rb"); // Open the file in binary mode

            if (fileptr == NULL)
            {
                DEBUG_LOG("Error opening file\n");
                return 1;
            }

            fseek(fileptr, 0, SEEK_END);
            file_size = ftell(fileptr);
            fseek(fileptr, 0, SEEK_SET);

            buffer = (char *)malloc((file_size + 1) * sizeof(char)); // Allocate memory for the file data
            fread(buffer, file_size, 1, fileptr);                   // Read the file data into the buffer

            // Send the size of the file as a long int
            send(client_sockfd, &file_size, sizeof(file_size), 0);
            // Transfer the file
            int file_trans_result = sendFileResponse(client_sockfd, TEMP_TAR);
            if (file_trans_result != 0)
            {
                DEBUG_LOG("Error transferring file\n");
            }
            else
            {
                DEBUG_LOG("File transferred successfully\n");
            }

            fclose(fileptr); // Close the file
        } else {
            DEBUG_LOG("Error executing command\n");
			return 1;
        }
    } else {
        // Send text response
		DEBUG_LOG("No files found.\n");
		sendTextResponse(client_sockfd, "No files found.");
    }
	
	// Delete temp_filelist.txt
    status = remove(TEMP_FILELIST);
    if (status != 0) {
        DEBUG_LOG("Error deleting temp_filelist.txt\n");
    }
    
    // Delete temp.tar.gz
    status = remove(TEMP_TAR);
    if (status != 0) {
        DEBUG_LOG("Error deleting temp.tar.gz\n");
    }
	
    return 0;
}

int recursiveSearchDate(char *dir_name, time_t date1, time_t date2, int *fileCount)
{
	DIR *dir;
    struct dirent *ent;
    char buffer[MAX_BUFFER_SIZE];
	FILE *fp;
	
	// Clear the buffer
	memset(buffer, 0, sizeof(buffer));
	
	DEBUG_LOG("Start recursive search (date)\n");
	
    // Open root directory
    if ((dir = opendir(dir_name)) == NULL) {
        DEBUG_LOG("Error opening directory.\n");
        return 1;
    }
	
	// Open temp file list 
	fp = fopen(TEMP_FILELIST, "a");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file\n");
        return 1;
    }

    
    while ((ent = readdir(dir)) != NULL)
    {
        // Skip . and ..
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
        {
            continue;
        }
		
		DEBUG_LOG("Readdir: %s\n", ent->d_name);
		
		if (dir_name[strlen(dir_name) - 1] == '/') {
				sprintf(buffer, "%s%s", dir_name, ent->d_name);
		} else {
			sprintf(buffer, "%s/%s", dir_name, ent->d_name);
		}
		
		// If Directory, recursively search it
		if (ent->d_type == DT_DIR)
        {
            recursiveSearchDate(buffer, date1, date2, fileCount);
        }
		
        // If file, check if it's between dates and print name
        else{
			
			// Get file/directory stats
			struct stat statbuf;
			if (stat(buffer, &statbuf) == -1)
			{
				continue;
			}
			
			time_t filetime;
			#ifdef __APPLE__
				filetime = statbuf.st_birthtime;
			#else
				filetime = statbuf.st_mtime;
			#endif
			
			DEBUG_LOG("File %s with date %ld\n", ent->d_name, filetime);
            if (filetime >= date1 && filetime <= date2)
            {	

				if (dir_name[strlen(dir_name) - 1] == '/') {
					sprintf(buffer, "%s%s", dir_name, ent->d_name);
				} else {
					sprintf(buffer, "%s/%s", dir_name, ent->d_name);
				}
				fprintf(fp, "%s\n", buffer);
				DEBUG_LOG("Saving to %s: %s \n", TEMP_FILELIST, buffer);
                *fileCount+=1;
            }
        }
    }
    closedir(dir);
	fclose(fp);
	
	return 0;
}

int dgetfiles(int client_sockfd, char **arguments)
{
    time_t date1 = convertDateToUnixTime(arguments[0], 1);
    time_t date2 = convertDateToUnixTime(arguments[1], 2);
    char *root_path = HOME_DIR;	// get home directory path
    char buffer[MAX_BUFFER_SIZE];
	char *tar_command_format = "tar -czf %s -T %s";
	
	DEBUG_LOG("Starting dgetfiles process.\n");
	DEBUG_LOG("Date 1: %ld\n",date1);
	DEBUG_LOG("Date 2: %ld\n",date2);

	int status, fileCount = 0;
	
	// get files
    recursiveSearchDate(root_path, date1, date2, &fileCount);
	DEBUG_LOG("Files found: %d\n",fileCount);
    sprintf(buffer, tar_command_format, TEMP_TAR, TEMP_FILELIST);

    if (fileCount > 0) {
		DEBUG_LOG("Executing: %s\n", buffer);
        int status = system(buffer);
        if (status == 0) {
            DEBUG_LOG("Command executed successfully\n");
            // Send the compressed file to the client
            FILE *fileptr;
            char *buffer;
            long file_size;
            long int file_size_getfiles;

            fileptr = fopen(TEMP_TAR, "rb"); // Open the file in binary mode

            if (fileptr == NULL){
                DEBUG_LOG("Error opening file\n");
                return 1;
            }

            fseek(fileptr, 0, SEEK_END);
            file_size = ftell(fileptr);
            fseek(fileptr, 0, SEEK_SET);

            buffer = (char *)malloc((file_size + 1) * sizeof(char)); // Allocate memory for the file data
            fread(buffer, file_size, 1, fileptr);                   // Read the file data into the buffer

            // Send the size of the file as a long int
            send(client_sockfd, &file_size, sizeof(file_size), 0);
            // Transfer the file
            int file_trans_result = sendFileResponse(client_sockfd, TEMP_TAR);
            if (file_trans_result != 0){
                DEBUG_LOG("Error transferring file\n");
            } else{
                DEBUG_LOG("File transferred successfully\n");
            }

            fclose(fileptr); // Close the file
        } else {
            DEBUG_LOG("Error executing command\n");
			return 1;
        }
    } else {
        // Send text response
		DEBUG_LOG("No files found.\n");
		sendTextResponse(client_sockfd, "No files found.");
		return 0;
    }
	
	// Delete temp_filelist.txt
    status = remove(TEMP_FILELIST);
    if (status != 0) {
        DEBUG_LOG("Error deleting temp_filelist.txt\n");
    }
    
    // Delete temp.tar.gz
    status = remove(TEMP_TAR);
    if (status != 0) {
        DEBUG_LOG("Error deleting temp.tar.gz\n");
    }

    return 0;
}

void removeLineBreak(char buffer[]) {
    int length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
    }
}

time_t convertDateToUnixTime(const char *time_str, int dateType)
{
    struct tm tm;
    int year, month, day;
    if (sscanf(time_str, "%d-%d-%d", &year, &month, &day) != 3)
    {
        return (time_t)-1;
    }
    tm.tm_year = year - 1900;  // adjust for year starting at 1900
    tm.tm_mon = month - 1;     // adjust for month starting at 0
    tm.tm_mday = day;
	if(dateType == 1){
		tm.tm_hour = 0;
		tm.tm_min = 0;
		tm.tm_sec = 0;
	}
	else{
		tm.tm_hour = 23;
		tm.tm_min = 59;
		tm.tm_sec = 59;
	}
    tm.tm_isdst = -1;  // let mktime determine DST
    time_t t = mktime(&tm);
    if (t == (time_t)-1)
    {
        return (time_t)-1;
    }
    return t;
}
