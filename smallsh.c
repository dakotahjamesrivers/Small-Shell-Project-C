#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdlib.h>

//Global variable. Didn't want to overcomplicate my already faulty interupt signal function.
bool foregroundOnly = false;

/*
FUNCTION: getUserInput()
INPUT:None
OUTPUT:A string
DESRIPTION: This function uses getLine to grab standard input from the user and returns a modified string. First, the string grabs user input with getline(). The function then checks if it's empty, or a comment line with "#", in these two cases, it frees the buffer and returns
and empty buffer. If the string is not empty or a comment, it then removes the newline character from the end. Finally, the function parses the string for any instance of "$$" in which it replaces this with the process ID of the shell.
*/
char *getUserInput() {
    char* buffer;
    size_t line;
    size_t bufferSize = 2048;
    while(1){
        //This function displays the prompt, automatically ignores comments and blanks, then returns a full string input
        printf(": ");
        fflush(stdout);
        buffer = malloc(sizeof(char)*bufferSize);
        clearerr(stdin);
        getline(&buffer, &bufferSize, stdin);
        //getline grabs all as one string
        if(bufferSize == 0){
            free(buffer);
        }
        else if(buffer[0] == '#' || strcmp(buffer, "\n") == 0){;
            //Detects if string is comment line or newline, ignores
            free(buffer);
        }
        else{
		int len = strlen(buffer);
		if(buffer[len-1]=='\n'){
		buffer[len-1] = 0;
		    } 
            //Now we expand the $$ into process ID
            for(int i = 0; i < (len-1);i++){
                if(buffer[i] == '$' && buffer[i+1] == '$'){
                    //Here we see at some point, we detected a $ before the last character in the string.
                    //We check the next character to see if that's also a $. If so, we have our index for $$.
                    int expansionNum = getpid();    //Grab the pid
                    char* expansion = malloc(sizeof(char)*512);     //make a string
                    sprintf(expansion, "%d", expansionNum);     //Convert to string
                    buffer[i] = '\0';       //Set new end of string for buffer at index of $$
                    strcat(buffer, expansion);
                    break;
                }
            }
            return buffer;
        }
    }
}

/*
FUNCTION: CatchSIGTSTP()
INPUT: the signal number
OUTPUT: None
DESRIPTION: This function catches any instace where the user presses CTRL-Z. This changes the entire shell into foreground only mode, which is used later. This can be undone by pressing CTRL-Z again.
*/
void catchSIGTSTP(int signo) {
    //This catches ctrl z
    // The shell then enters a state where subsequent commands can no longer be run in the background.
    // In this state, the & operator should simply be ignored, i.e., all such commands are run as if they were foreground processes.
    if (!foregroundOnly) {
        //If foreground only = false
        foregroundOnly = true;
        //Printf doesn't work here
        write(1, "Entering foreground-only mode (& is now ignored)\n",49);
        fflush(stdout);
    } 
    
    else {
        // Display another informative message (see below) immediately after any currently running foreground process terminates
        // The shell then returns back to the normal condition where the & operator is once again honored for subsequent commands,
        // allowing them to be executed in the background.
        foregroundOnly = false;
        write(1, "Exiting foreground-only mode\n",29);
        fflush(stdout);
    }
}

/*
FUNCTION: MAIN
INPUT: NONE
OUTPUT: NONE
DESRIPTION: So i decided to compress most of the functionality of the program to my main function. I found putting this all in a do while loop drastically simplified the program. Originally, i did try to make a struct
for the shell like the assignment page suggest, but it become very complicated passing variables all over the place. I know modularity is prefferred, but in this program, i felt like a stripped down and more simpler 
design made more sense to me as a programmer. The program does not do anything vastly complicated, and creating structs and passing variables all over the place wreaking havoc on the stack felt unneccessary to me
*/

int main(int argc, char* argv[]){
    char* userInput = "";   //String to be modifed for later tokenization
    bool isExit = false;    //Bool value for do while to detect exit
    int maxNumberOfArguments = 512;
    bool backgroundTrigger;     //This is important, this value dictates (as long as CTRL-Z is correclty in place) whether the command is in background mode or not
    int exitStatus = 0;
    pid_t spawnPid = -5;
    int sourceFD, targetFD, result, fd;

    //Signal hanglers
    //CTRL Z Handler
    //out out SIGTSTP action struct
    //I will admit these are a bit of a mystery to me. I took this class last semester and this was in the instructor's slides If there's any issue with my program, it lies in my interrupt handling here.
    struct sigaction SIGTSTP_action = {0};
	SIGTSTP_action.sa_handler = catchSIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = 0;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);
    //CTRL C Handler
	struct sigaction SIGINT_action = {0};
	SIGINT_action.sa_handler = SIG_IGN;
	sigfillset(&SIGINT_action.sa_mask);
	SIGINT_action.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_action, NULL);


    do{
        //We not progress into the do while loop.
        char inFile[512] = "";      //Potential input redirection file
        char outFile[512]= "";      //Potential output redirection file
        int maxNumberOfArguments = 512;
        char** arguments = malloc(maxNumberOfArguments * sizeof(char));     //This string array will contain relavant arguments to later be passed to execvp
        int count = 0;
        int maxSizeOfToken = 1024;
        int currentTokenCount = 0;
        char delim[2] = " ";    //strok delimiter
        int k = 0;  //Index for relevant arguments
        userInput = getUserInput();     //Here we get the string from the user's command
        char *token1 = strtok(userInput, delim);    //Begin tokenizing the command
        //Notice that in this tokenizing block, tokens after </> aren't added to the list of commands, as smallsh controls file redireciton, not the shell underneath.. This threw my design off for a long time. Thanks Eunjin for helping me spot this issue
        while(token1 != NULL){
            if(strcmp(token1, "<") == 0 || strcmp(token1, ">") == 0){
                if(strcmp(token1, "<")==0){
			    //If redirection is present, grab the next token
                token1 = strtok(NULL, delim);
			    //Copy that token to the input file
                strcpy(inFile, token1);
	//		    printf("Input file Name: %s\n", inFile);
			    //Continue the process
			    token1 = strtok(NULL, delim);
                    }
                else{
                    token1 = strtok(NULL, delim);
                    strcpy(outFile, token1);
			    //printf("Output file Name: %s\n", outFile);
			token1 = strtok(NULL, delim);
                }
            }
            else if(strcmp(token1, "&")==0){
                //Detect if the user wants a command to run in the background
                printf("This process will run in the background\n");
                backgroundTrigger = true;
                token1 = strtok(NULL, delim);
            }
            else if(strcmp(token1, "$$")==0){
                printf("Detected $$ symbol\n");
                token1=strtok(NULL, delim);
                //This block is a bit redudant since expansion has already been handled, but i left it in anywas. 
            }
            else{
                //Finally, the token is confirmed to be an actual relevant argument. Using the extremely handy ane illusive function strdup, a copy of the token is created in memory and a pointer is returned to the arguments at the proper index. Extremely handy little niche tool.
                arguments[k] = strdup(token1);
                k++;
                token1 = strtok(NULL, delim);
            }
        }
	//Now make sure it is null terminated, this ensures no funny business with the string.
	arguments[k] = NULL;
        int numOfArguments = k+1;
        //We now have a string array of the tokenized input and the amount of arguments saved into count
        //Time to check for one of the three built in functions
        if(strcmp(arguments[0], "cd") == 0){
            if(!arguments[1]){
                //No other arguments detected, grab the HOME from environment
                chdir(getenv("HOME"));
            }
            else if(chdir(arguments[1]) == -1){
                //Another argument detected, but there was an error opening it
                printf("No directory with that name\n"); fflush(stdout);
            }
            else if(chdir(arguments[1]) != -1){
                //Another argument detected, no error opening it.
                chdir(arguments[1]);
            }
        }
        else if(strcmp(arguments[0], "status")==0){
            //Print Status function. Again, the status codes are a bit confusing at times, but my code seems to function properly here.
            if(WIFEXITED(exitStatus) == 1){
                //Child terminated normally
                printf("Child %d terminated normally with status %d\n", spawnPid, WEXITSTATUS(exitStatus)); fflush(stdout);
                }
            else{
                printf("Child %d terminated absnormally with status %d\n", spawnPid, WTERMSIG(exitStatus)); fflush(stdout);
             }
        }
        else if(strcmp(arguments[0], "exit") == 0){
            //Here if the only argument is exit, we know the user wants to exit (Who would have guessed). The boolean value is caught before the system forks.
            isExit = true;
        }
        else{
            //Now we finally reach the time to fork.
            //So time to fork....
            spawnPid = fork();
            switch(spawnPid){
                case -1:
                    //error
                    perror("error with fork()\n"); fflush(stdout);
                    exit(1);
                    break;
                case 0:
                    //Sweet Child O' Mine
                    SIGINT_action.sa_handler = SIG_DFL;
			        sigaction(SIGINT, &SIGINT_action, NULL);
                    //As per assignment directions, redirection is done in child process


                    //IF INPUT REDIRECTION IS PRESENT
                    if(strcmp(inFile, "")){
                        //File Redirection
                        sourceFD = open(inFile, O_RDONLY);
                        if (sourceFD == -1){
                            perror("Cannot open input file\n"); fflush(stdout);
                            exit(1);
                        }
                        if((dup2(sourceFD, 0)) == -1){
                            perror("Problem with dup2()\n");fflush(stdout);
                            exitStatus = 1;
                            exit(1);
                        }
                        //close the stream
                        fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
                        }
                        
                    //IF OUTPUT REDIRECTION IS PRESENT
                    if(strcmp(outFile, "")){
                        //File redirection
                        fd = open(outFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                        if(fd == -1){
                            perror("Can't open output file\n"); fflush(stdout);
                            exitStatus = 1;
				            exit(1);
                        }
                        if((dup2(fd,1)) == -1){
                            perror("Source dup2(). Can't find output file\n"); fflush(stdout);
                            exitStatus = 1;
				            exit(1);
                            }
                        }

                        //IF BACKGROUND MODE and no file redirection has been specified already
                        if(backgroundTrigger == true && foregroundOnly == false){
                            if(!strcmp(inFile,"")){
                               //User has not specified redirection while in background mode. input redirection
                                sourceFD = open("/dev/null", O_RDONLY);
                                if (sourceFD == -1){
                                    perror("Cannot open input file\n");fflush(stdout);
                                    exitStatus = 1;
                                    exit(1);
                                }
                            }
                            if(!strcmp(outFile,"")){
                                //User has not specified redireciton while in background mode. Output redirection
                                fd = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0666);
                                if(fd == -1){
                                    perror("Can't open output file\n"); fflush(stdout);
                                    exitStatus = 1;
                                    exit(1);
                                }
                                if(dup2(fd,1) == -1){
                                    perror("Source dup2(). Can't find output file\n"); fflush(stdout);
                                    exitStatus = 1;
                                    exit(1);
                                }
                            }
                        }
                        //Close the stream
                        fcntl(fd, F_SETFD, FD_CLOEXEC);
                        fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
			
                    //FINALLY TIME TO EXECUTE
                    execvp(arguments[0], arguments);
                    perror("Error with execvp\n"); fflush(stdout);
                    exitStatus = 1;
                    exit(1);
                    break;
                default:
                    //Parent
                    if(backgroundTrigger == 1 && foregroundOnly == false){
                        //The process is in the background. Use WNOHANG
                        waitpid(spawnPid, &exitStatus, WNOHANG);
                        printf("Background PID number: %d\n", spawnPid); fflush(stdout);
                    }
                    else{
                        waitpid(spawnPid, &exitStatus, 0);
                    }
            }
        }
        free(userInput);
        free(arguments);
    //Last thing to do is check the exit status before reprompting. Pulled this directly from skeleton code
    while ((spawnPid = waitpid(-1, &exitStatus, WNOHANG)) > 0){
        //Terminated!
        printf("Child process %d terminated!\n", spawnPid); fflush(stdout);
        //Copied from Module 4
        if (WIFEXITED(exitStatus) == 1){
            //Child terminated normally
            printf("Child %d terminated normally with status %d\n", spawnPid, WEXITSTATUS(exitStatus)); fflush(stdout);
        }
        else{
            printf("Child %d terminated absnormally with status %d\n", spawnPid, WTERMSIG(exitStatus)); fflush(stdout);
        }
    }
    backgroundTrigger = false;  //Make sure background trigger is off and ready for next command prop, where background mode ISN'T the default.
    }while(isExit != true);
    return(0);
}

