//********************************************************
// S M A L L  S H E L L
// smallshell.c
// CS 344
// Rebecca Kuensting
// May 23, 2017
//*******************************************************

#include <errno.h>
#include <dirent.h>
#include <signal.h>
#include <termios.h>  //to use tcflush
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>

//*******************************************************
//
// P R E P R O C E S S O R  V A R I A B L E S
//
//*******************************************************

//Exit Codes
#define		SOMETHING_WENT_WRONG	1
#define		THE_GOOD_ENDING			0

//From the assignment:
//Your shell must support command lines with a maximum length of 2048 characters, and a maximum of 512 arguments. You do not need to do any error checking on the syntax of the command line.
#define		MAX_CHARACTERS			2048 
#define		MAX_ARGUMENTS			512
#define		MAX_FILENAME			300 //in windows, this limit is 260, not sure about linux

//*******************************************************
//
// Functions
//
//*******************************************************

//*******************************************************
//
// F I N D  I N  S T R I N G  function
// finds the target substring in a string
// returns an int -- treated as a bool
//
//*******************************************************
int findInString(char *string, char *target){
//strstr() returns a pointer to first substring
  char *subPtr;

  subPtr = strstr(string, target); 

  //if string does not contain target
  if (subPtr == NULL){
  	return 0;
  }
  //if match found!
  else{
  	return 1;
  }
}

//*******************************************************
//
// S H E L L  L O O P
// Keeps looping to receive user commands as long as
// user keeps shell up.
//
//*******************************************************
int shellLoop()
{
	//define variables
	char inputString[MAX_CHARACTERS];
	char errorOutput[50] = "Oops, something went wrong!";
	int keepGoing = 1;			//controls the loop. Treat like a bool
	int statusInt;
	char statusString[MAX_FILENAME];

	while (keepGoing == 1)
	{
		// At the start of each loop, refresh the shell by clearing stdin buffer
		//found here: https://stackoverflow.com/questions/10938882/how-can-i-flush-unread-data-from-a-tty-input-queue-on-a-unix-system
		//also consider tcdrain()?
		tcflush(0, TCIFLUSH);
		fflush(stdout);  //apparently using fflush to clear stdin gives undefined behavior

		// Overwrite past user input with more nothing
		sprintf(inputString, "");

    //(GETTING INPUT -- separate into own function)	
    // Wait for some input from the user. Prompt is a colon, per instructions
		printf(": ");
		fgets(inputString, 50, stdin);
		
		//strtok will assign everything before the newline delimiter to textUntilNewline
		char textUntilNewline[MAX_CHARACTERS];
		textUntilNewline = strtok(inputString, "\n");

		// if there's nothing in textUntilNewline, nothing was entered by user
		if(textUntilNewline == NULL){
			//go back to top of loop and wait some more
			continue;
		}
		
		else{
			// Check first to see if input was a comment
			if (inputString[0] == '#'){
				//If so, do nothing with this line and restart loop
				continue;
			}
			// Check if what the user typed was "exit"
			else if(strcmp(userInput, "exit") == 0){
				exitShell = 1;
				printf("User-initiated exit");
				exit(THE_GOOD_ENDING);
			}
			// if there is usable text, append a null terminator to the end of the line
			int length = (sizeof(inputString)) - 1;
			else if (inputString[length] == '\n'){
				inputString[length] = '\0';
			}
			//if there's no newline, the input was too long. Give an error and continue.
			else if (inputString[length] != '\n'){
				printf("\nYour input was weird. Give it another shot.")
				continue;
			}
		}
	
		// If we encounter the end of a file, exit
		if (feof(stdin))
		{
			exit(THE_GOOD_ENDING);
		}

		//*******************************************************
		//H A N D L I N G  "cd"
		//*******************************************************
		
		//Just "cd" by itself:
		if (strcmp(inputString, "cd") == 0){
			// Get "home" location with getenv()
			char* home[50];
			home = getenv("HOME");

			// Transport the user to resulting directory
			chdir(home);
			
			//head back to top of loop
			continue;
		}

		// cd + a destination name
		if (strncmp("cd ", inputString, 3) == 0){
		
			// figure out the name of destination directory
			char* dirName[MAX_FILENAME];
			dirName = strtok(inputString, " ");
			
			//call strtok again with NULL to hop to first arg
			dirName = strtok(NULL, " ");
			
			//make sure the directory really exists
			//reference: https://stackoverflow.com/questions/12510874/how-can-i-check-if-a-directory-exists
			DIR* directory = opendir(dirName);
			if(directory){
				printf("looking good, we got a real directory");
			}
			else if (ENOENT == errno){
				printf("No such directory. Try again!");
				continue;
			}
			else{
				printf("Something weird happened with your input. Try again");
				continue;
			}

			// go to directory provided by the user
			chdir(dirName);
			continue;
		}

		//*******************************************************
		//H A N D L I N G  "status"
		//this code executes every loop to reset status even if status
		//isn't called
		//*******************************************************
		if (strcmp(inputString, "status") == 0){
			
			// If there's no special string to display, just give statusint
			if (strncmp(statusString, "", 1) == 0){
				printf("exit value %d\n", statusInt);
			}
			// If there is string output from last process, print it
			else{
				printf("%s\n", statusString);
			}

			// Overwrite status message and int
			sprintf(statusString, "");
			statusInt = 0;
			continue;
		}
		// reset status variables
		else{
			sprintf(statusString, "");
			statusInt = 0;
		}

		//*******************************************************
		//H A N D L I N G  a request to run background process
		//*******************************************************
		if (inputString[length] == '&'){
			//get rid of the ampersand at the end of the line
			strncpy(inputString, &inputString[0], length-1);
			//send it to the background function
			backgroundRun(inputString);
			continue;
		}
		
		//*******************************************************
		//H A N D L I N G  a regular foreground command
		//*******************************************************
		statusInt = foregroundRun(userInput, statusString);
	}
}

//*******************************************************
//
// F O R E G R O U N D  P R O C E S S  function
//
//*******************************************************
int foregroundRun(char input[MAX_CHARACTERS], char statusStr[MAX_FILENAME]){
	//using -5 because of lectures
	pid_t fgPid = -5;
	int statusInt = 0;
	int where = -1;
	
	//refer here: http://man7.org/linux/man-pages/man2/sigaction.2.html
	struct sigaction act;
	
	char filename[MAX_CHARACTERS] = {" "};

	// Determine if we have any redirects
	int hasOutputRedirect = ContainsString(userCommand, ">");
	int hasInputRedirect = ContainsString(userCommand, "<");

	//going to use an array to keep track of all user arguments
	char *arguments[MAX_ARGUMENTS];

	// search input for redirect symbols
	// treat these like bools
	int redirectInput = ContainsString(input, "<");
	int redirectOutput = ContainsString(input, ">");

	// If we need to redirect some input or output:
	//OUTPUT:
	if (redirectOutput == 1){
		GetFileName(input, fileName);
		where = open(fileName, O_WRONLY|O_TRUNC|O_CREAT, 0644);
	}
	//INPUT:
	else if (redirectInput == 1){
		GetFileName(userCommand, fileName);
		where = open(fileName, O_RDONLY, 0644);
	}
	//User didn't use a redirect symbol
	else{
		//could maybe just use "null" for this?
		where = open("/dev/null", O_RDONLY);
	}

	//loop through "input" to tokenize all arguments
	//push them into argument array
	char* tempArg[300];
	tempArg = strtok(inputString, " ");
	int i = 0;
	while(tempArg != NULL){
		arguments[i] = tempArg;
		tempArg = strtok(NULL, " ");
		i++;
	}
	
	//testing print statement
	printf("Arguments pushed to array within fg function. Test args here");

	// fork off a child process	
	fgPid = fork();

		//act based on the Pid we got from forking:
	if(fgPid == -1){
		//something went wrong with the fork, because this was our starting value
		printf("All Forked up (FG)!");
		exit(SOMETHING_WENT_WRONG);
	}
	else if(fgPid == 0){
		//we have a child process -- we're inside of it
		//redirect standard out to the user-specified file with dup2
		//refer here: http://codewiki.wikidot.com/c:system-calls:dup2
		if(redirectOutput && dup2(where, 1) < 0){
			//if result of dup2 < 0, exit badly
			printf("ERROR (FG): failed to open %s\n", filename);
			return(SOMETHING_WENT_WRONG);
		}
		//this is the same for input redirect and no redirect, because "where" already points to the right place.
		else if(dup2(where, 0) < 0){
			//if result of dup2 < 0, exit badly
			printf("ERROR: failed to open %s\n", filename);
			return(SOMETHING_WENT_WRONG);
		}
		else{
			//leave it alone because everything is fine
		}
		
		//close the file
		close(where);
		
		// EXECUTE THE COMMAND
		//execvp, from http://www.csl.mtu.edu/cs4411.ck/www/NOTES/process/fork/exec.html
		//The first argument is a character string that contains the name of a file to be executed.
		//The second argument is a pointer to an array of character strings. More precisely, its type is char **, which is exactly identical to the argv array used in the main program:
	  	int runSuccess = execvp(arguments[0], arguments);
		if (runSuccess < 0){
			printf("%s: no such file or directory\n", argv[0]);
			exit(SOMETHING_WENT_WRONG);
		}
	}

	// back inside the parent

	close(where);
			
	// Set up the signal handler for the parent process to ignore termination messages
	act.sa_handler = SIG_DFL;  //default
	act.sa_handler = SIG_IGN;  //ignore
	sigaction(SIGINT, &act, NULL);

	// Wait for child to finish up -- THIS AVOIDS ZOMBIFICATION!
	// refer here: https://linux.die.net/man/2/waitpid
	waitpid(fgPid, &statusInt, 0);
	
	//need to save original status code
	int saveStatusInt = statusInt;
	
	//Overwriting status with exit status int
	if(WIFEXITED(statusInt)){
		statusInt = WEXITSTATUS(statusInt);
	}
	else{
		printf("Something crazy happened and waitpid didn't work (FG)");
		exit(SOMETHING_WENT_WRONG);
	}

	// get the error message if there was one
	if(WIFSIGNALED(saveStatusInt)){
		int signalInt = WTERMSIG(saveStatusInt);
			//convert signalInt to string:
			char signalString[16];
			sprintf(signalString, "%d", signalInt);

   		// Print error message and save it for next time "status" is called in shell
		char fullErrMsg[MAX_FILENAME];
		sprintf(fullErrMsg, "Terminated by signal: %s", signalString);
		//print to console:
		printf("%s\n", fullErrMsg);
		strcpy(statusString, fullErrMsg);
	}
	else{
		//overwrite statusString with nothing
		strcpy(statusString, "");
	}

	return statusInt;  //return only the exit status number
}

//*******************************************************
//
// B A C K G R O U N D  P R O C E S S  function
//
//*******************************************************
int backgroundRun(char input[MAX_CHARACTERS]){
	//using -5 because lecture said to
	pid_t bgPid = -5;
	char pidString[16];
	int where = -1;
	char filename[MAX_FILENAME];
	
	//going to use an array to keep track of all user arguments
	char *arguments[MAX_ARGUMENTS];

	// search input for redirect symbols
	// treat these like bools
	int redirectInput = ContainsString(input, "<");
	int redirectOutput = ContainsString(input, ">");

	// If we need to redirect some input or output:
	//OUTPUT:
	if (redirectOutput == 1){
		GetFileName(input, fileName);
		where = open(fileName, O_WRONLY|O_TRUNC|O_CREAT, 0644);
	}
	//INPUT:
	else if (redirectInput == 1){
		GetFileName(userCommand, fileName);
		where = open(fileName, O_RDONLY, 0644);
	}
	//User didn't use a redirect symbol
	else{
		//could maybe just use "null" for this?
		where = open("/dev/null", O_RDONLY);
	}

	//loop through "input" to tokenize all arguments
	//push them into argument array
	char* tempArg[300];
	tempArg = strtok(inputString, " ");
	int i = 0;
	while(tempArg != NULL){
		arguments[i] = tempArg;
		tempArg = strtok(NULL, " ");
	}
	
	//testing print statement
	printf("Arguments pushed to array within bg function. Test args here");

	// fork off a child process	
	bgPid = fork();

	//act based on the Pid we got from forking:
	if(bgPid == -1){
		//something went wrong with the fork, because this was our starting value
		printf("All Forked up!");
		exit(SOMETHING_WENT_WRONG);
	}
	else if(bgPid == 0){
		//we have a child process -- we're inside of it
		//redirect standard out to the user-specified file with dup2
		//refer here: http://codewiki.wikidot.com/c:system-calls:dup2
		if(redirectOutput && dup2(where, 1) < 0){
			//if result of dup2 < 0, exit badly
			printf("ERROR: failed to open %s\n", filename);
			return(SOMETHING_WENT_WRONG);
		}
		//this is the same for input redirect and no redirect, because "where" already points to the right place.
		else if(dup2(where, 0) < 0){
			//if result of dup2 < 0, exit badly
			printf("ERROR: failed to open %s\n", filename);
			return(SOMETHING_WENT_WRONG);
		}
		else{
			//leave it alone because everything is fine
		}
		
		//close the file
		close(where);
		
		// EXECUTE THE COMMAND
		//execvp, from http://www.csl.mtu.edu/cs4411.ck/www/NOTES/process/fork/exec.html
		//The first argument is a character string that contains the name of a file to be executed.
		//The second argument is a pointer to an array of character strings. More precisely, its type is char **, which is exactly identical to the argv array used in the main program:
	  	int runSuccess = execvp(arguments[0], arguments);
		if (runSuccess < 0){
			printf("%s: no such file or directory\n", argv[0]);
			exit(SOMETHING_WENT_WRONG);
		}
	}
	
	//back in parent process
	//close the file
		close(where);

	//print the pid
	// Output the process ID message for background processes
		sprintf(pidString, "%d", bgPid);
		printf("The background pid is %s\n", pidString);

	return THE_GOOD_ENDING;
}


//*******************************************************
//
// S I G N A L  H A N D L E R   function
// specifically for child processes
// this gets called from main
//
//*******************************************************
static void childSignalHandler(int signal)
{
	int childStatus;
	pid_t childProcessID;

	// wait for child processes (this avoids ZAMBIES!)
	//reference for waitpid usage: If successful, waitpid returns the process ID of the terminated process whose status was reported. If unsuccessful, a -1 is returned.  https://support.sas.com/documentation/onlinedoc/sasc/doc/lr2/waitpid.htm
	childProcessID = waitpid(-1, &childStatus, WNOHANG);
	
	while (childProcessID > -1){
		
		char childError[MAX_FILENAME]; 
		
		//Make the process ID into a string
		char pidToString[16];
		sprintf(pidToString, "%d", childProcessID);
		printf("Got a PID! %s", pidToString);

		// Make an error message like assignment example:
		sprintf(childError, "\nbackground pid %s has terminated: ", pidToString);
	
		//Figure out if an error killed child	
		if(WIFSIGNALED(childStatus)){
			int signalInt = WTERMSIG(childStatus);

			// Print error message and save it for next time "status" is called in shell
			char fullErrMsg[MAX_FILENAME];
			sprintf(fullErrMsg, "Terminated by signal: %d", signalInt);
			//print to console:
			printf("%s\n", fullErrMsg);
			strcpy(statusString, fullErrMsg);
		}
		//if no errors:
		else
		{
			char statusInt[16];
			statusInt = WEXITSTATUS(childStatus);
			sprintf(childError, "exit value\t%d\n", statusInt);

			//may need to replace sprintf with more secure one and printf with write if these are unstable
			
			// print the non-error message
			printf(childError);
		}

		continue;
	}
}


//*******************************************************
//
// M A I N  function
//
//*******************************************************
int main()
{
	printf("BEGINNING OF MAIN");
	
	// Make a signal handler
	struct sigaction act;
	//point the struct to my handler function
	act.sa_handler = childSignalHandler;
	//tell it what to listen for
	sigaction(SIGCHLD, &act, NULL);

	// Get the main shell loop going
	shellLoop();

	return(THE_GOOD_ENDING);
}














