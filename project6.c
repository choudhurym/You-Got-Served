/*******************************************************
*
*	Project Name:	Project 6 - You Got Served
*	Description:	Implementation of a simple web server
*	Filenames:	project6.c
*	Date:		Friday, May 4, 2018
*	Authors: 	Muntabir Choudhury & Rory Dudley
*
*******************************************************/

/* imports */
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <netdb.h>
#include <fcntl.h>

/* constants */
#define LO_PORT 1025
#define HI_PORT 65535

/* function prototypes */
int fileNotFound(char* fileBuffer, char* filePath, char* protocol);
void sendHeader(char* ctype, int fileSize, int otherSocket);
void processFilePath(char* filePath);
char* contentType(char* filePath);

/*
	main:		main function of the program

	params:		argc - number of commandline arguments
				argv - strings of the commandline argmuments entered by the user

	returns:	0 - clean exit
				1 - some error was detected (incorrect # of params, could not change directory, bad port)
*/
int main(int argc, char** argv)
{
	//Closes the program given there isn't the appropriate command line arguments.
	if(argc != 3)
	{
		printf("Usage: %s <port> <path to root>\n", argv[0]);
		return 1;
	}

	int file; // file descripter for reading files
	int otherSocket; // client socket
	int fileSize; // file size of specified resource
	int readFile = 1; // holds number of bytes read from a file
	int received = 1; // holds number of bytes received from a client

	char buffer[1024]; //h hold's all of the client's sent information
	char request[255]; // holds the client's request (GET/HEAD)
	char filePath[255]; // holds the specified file path/name
	char protocol[255]; // holds the client's protocol
	char fileBuffer[1024]; // holds the file's information
	char* ctype; // holds the content/MIME type of specified resource

	// retrieves the arguments from the command line
	char* portNum = argv[1];
	const char* path = argv[2];

	struct stat st; // declaration of a struct 'fstat' for gathering file size info

	// closes the program if the file directory cannot be changed
	if(chdir(path))
	{
		printf("Could not change to directory: %s\n", path);
		return 1;
	}

	int port = atoi(portNum); // convert the portnumber entered as program arg from a string to an int

	// creates the server-side socket
	int sockFD = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = INADDR_ANY;

	// enables port re-use
	int value = 1;
	setsockopt(sockFD, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

	// checks if the the port is in the correct range, then trys to bind the port to a socket
	if((port < LO_PORT || port > HI_PORT) || (bind(sockFD, (struct sockaddr *)&address, sizeof(address)) == -1))
	{
		printf("Bad Port: %d\n", port);
		return 1;
	}

	listen(sockFD, 1); // istens for incoming requests

	// continuously runs the server
	while(1)
	{
		// continuously accepts incoming requests as long as the server is running
		struct sockaddr_storage otherAddress;
		socklen_t otherSize = sizeof(otherAddress);
		otherSocket = accept( sockFD, (struct sockaddr *)&otherAddress, &otherSize);

		// notifies the server if a client has been accepted
		if(otherSocket != 0)
				printf("\nGot a client: \n");

		received = recv(otherSocket, buffer, sizeof(buffer)-1, 0); // receives requests and store them into 'buffer'

		buffer[received] = '\0'; // adds the null character to the end of the client's sent information

		sscanf(buffer, "%s %s %s", request, filePath, protocol); // stores the request type, filepath, and protcol from buffer

		// only do anything if a 'GET' or 'HEAD' request was sent to the server
		if(strcmp("GET", request) == 0 || strcmp("HEAD", request) == 0)
		{
			processFilePath(filePath); // make any necessary modifications to the filepath
			printf("\t %s %s %s\n", request, filePath, protocol);

			file = open(filePath, O_RDONLY); // opens the file for reading only using low-level i/o

			fstat(file, &st); // calls the fstat function to get file info

			fileSize = st.st_size; // retrieves the file size stored in the stat struct

			if(file != -1)
			{
				ctype = contentType(filePath); // determines and stores the file's content type

				// only sends the 'HEAD' information upon request
				if(strcmp("HEAD", request) == 0)
				{
					sendHeader(ctype, fileSize, otherSocket);
					printf("Sent file: %s\n", filePath);
				}
				else
				{
					sendHeader(ctype, fileSize, otherSocket); // send the header information first for the requested file
					
					do
					{
						// reads the bytes from the file into 'fileBuffer' and send it to the client socket
						readFile = read(file, fileBuffer, sizeof(fileBuffer));
						send(otherSocket, fileBuffer, readFile, 0);
					} while(readFile == sizeof(fileBuffer)); // executes until the size of the file has been reached

					printf("Sent file: %s\n", filePath);
				}
					close(file); // closes the file after reading
			}
			else
			{
				printf("File not found: %s\n", filePath); // gives a 404 error if the file does not exist
				send(otherSocket, fileBuffer, fileNotFound(fileBuffer, filePath, protocol), 0); // send a 404 error to the client (utilizes the fileNotFound function to store 404 error info)
			}
		}
			close(otherSocket); // closes the socket connection after all processing was done
	}

	return 0;
}

/*
	processFilePath:	function for appropriate file path processing
						edits the filePath string appropriately according to project specifications

	params:				filePath - the full directory of the file being accessed

	returns:			void
*/
void processFilePath(char* filePath)
{
	// if the filepath/name ends with the '/' character, add 'index.html' to the end of the filepath
	if(filePath[strlen(filePath)-1] == '/')
		strcat(filePath, "index.html");

	// remove the character '/' from the beginning of the file name
	if(filePath[0] == '/')
		memmove(filePath, filePath+1, strlen(filePath));
}

/*
	fileNotFound:	function for generating a buffer to send a 404 error to a client if the requested page was not found

	params:			fileBuffer - a buffer to hold the info to be sent
					filePath - path the page the user initially tried to access
					protocol - the client's protocol

	returns:		strlen(fileBuffer) - the length of the data to be sent to the user
*/
int fileNotFound(char* fileBuffer, char* filePath, char* protocol)
{
	// only sends the HTTP format given there is an HTTP protocol
	if(strstr(protocol,"HTTP/"))
		sprintf(fileBuffer, "HTTP/1.0 404 Not Found\nContent-type: text/html\n\n<html>\n<body>\n<h1>Not Found</h1>\n<p>The requested URL %s was not found on this server.</p>\n</body>\n</html>\n", filePath);
	else
		sprintf(fileBuffer, "HTTP/1.0 404 Not Found\n");

	return strlen(fileBuffer); // returns the appropriate length for the parameter of the send function.
}

/*
	contentType:	determines the MIME type of a file based off it's file extension

	params:			filePath - path to the resource the client is trying to access, it's file extension will be examined

	returns:		a string of the appropriate MIME type
*/
char* contentType(char* filePath)
{
	char* fileExtension = strrchr(filePath, '.'); // find the last occurrence of the dot character in order to get the file extension

	// determine the MIME type
	if(!strcmp(fileExtension, ".html") || !strcmp(fileExtension, ".htm"))
		return "text/html";
	else if(!strcmp(fileExtension, ".jpg") || !strcmp(fileExtension, ".jpeg"))
		return "image/jpeg";
	else if(!strcmp(fileExtension, ".gif"))
		return "image/gif";
	else if(!strcmp(fileExtension, ".png"))
		return "image/png";
	else if(!strcmp(fileExtension, ".txt") || !strcmp(fileExtension,".c") || !strcmp(fileExtension,".h"))
		return "text/plain";
	else if(strstr(fileExtension, ".pdf"))
		return "application/pdf";

	return ""; // return nothing if MIME type is unknown (unsupported)
}

/*
	sendHeader:	send HTTP header info to a client

	params:		ctype - the content type (i.e. text/html, image/gif, etc)
				fileSize - the size of the data being sent
				otherSocket - the client socket to send the data to

	returns:	void
*/
void sendHeader(char* ctype, int fileSize, int otherSocket)
{
	char fileBuffer[255]; // make a file buffer to hold the HTTP exchange information.
	sprintf(fileBuffer, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n", ctype, fileSize); // writes http exchange info to file buffer (including content type and length)
	send(otherSocket, fileBuffer, strlen(fileBuffer), 0); // sends the header informaion.
}
