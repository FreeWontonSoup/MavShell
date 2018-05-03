/*
 Name:  Brandon Carter
 ID:    1001350607
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
// so we need to define what delimits our tokens.
// In this case  white space
// will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 5     // Mav shell only supports five arguments

//stores pids of last 10 processes run
struct pids
{
    pid_t pid;
    struct pids *next;
    int status;
};

//stores pids of all currently suspended processes
struct suspended
{
    pid_t pid;
    struct suspended *next;
};

//stores recent commands
struct cmdHist
{
    int cmdLength;
    char **token;
    struct cmdHist *next;
};

static void handle_signal (int sig)
{
    //printf ("Caught signal %d\n", sig );
    //printf("\n");
    //exit(0);
}

void SearchBin(char* token[], int token_count, pid_t parent_pid);
void ShowHistory(struct cmdHist **head);
int ExtractCommand(struct cmdHist **head, char *token[MAX_NUM_ARGUMENTS]);
void AddCmd(char *token[], struct cmdHist **commandsHead, int tokCount);

/*
 This function is called every time a process is spawned off
 It will add the pid of that process to a linked list
 */
int AddPid(int size, struct pids** head, pid_t child_pid, int status)
{
    //Loop to get to the end of the linked list
    struct pids *curr = *head;
    while(curr->next != NULL)
    {
        curr = curr->next;
    }
    //Add a new node and assign it the pid of the last process spawned
    curr->next = (struct pids*)malloc(sizeof(struct pids*));
    curr = curr->next;
    curr->pid = child_pid;
    curr->status = status;
    curr->next = NULL;
    size++;
    
    //if the size of the linked list is greater than 10
    //remove the oldest node (the head)
    //free its memory and resign the head to the next node
    if(size >= 11)
    {
        struct pids* temp = *head;
        (*head) = (*head)->next;
        free(temp);
    }
    return size;
}

/*
 List the last 10 process pids
 */
void ListPids(struct pids *head)
{
    //loop through the linked list of pids and print all of them out
    int count = 0;
    while(head != NULL)
    {
        printf("%d: %d\n",count,head->pid);
        head = head->next;
        count++;
    }
}

/*
 Determine the length of the input from the command line
 */
int InputLength(char* string[MAX_NUM_ARGUMENTS], int size)
{
    int i, total = 0;
    for(i = 0; i < size-1; i++)
    {
        total += strlen(string[i]);
    }
    return total;
}

/*
 Determine which directory the command issued is in and execute it
 */
void SearchBin(char* token[], int token_count, pid_t parent_pid)
{
    //store the 4 directories i will look for commands in
    char bin1[1] = "";
    char bin2[15] = "/usr/local/bin/";
    char bin3[9] = "/usr/bin/";
    char bin4[5] = "/bin/";
    
    //determine the input size of my command and concatenate it to my directory
    int inputSize = InputLength(token, token_count);
    char input[inputSize];
    strcpy(input,bin1);
    strcat(input,token[0]);
    
    //use execv to execute command
    //make sure to check all directories for the command
    //if it can't be found in any of those directories, they must have entered quit
    //or exit or the command can't be found (it's not a real command)
    if(execv(input, token) == -1)
    {
        //the strcpy replaces input with the directory location
        //strcat concatenates the program typed to be run in that directory
        //example: user types "ls" so input becomes "/user/local/bin/ls"
        strcpy(input,bin2);
        strcat(input,token[0]);
        //if exec results in error, try next directory
        if(execv(input,token) == -1)
        {
            strcpy(input,bin3);
            strcat(input,token[0]);
            if(execv(input,token) == -1)
            {
                //try next directory again
                strcpy(input,bin4);
                strcat(input,token[0]);
                if(execv(input,token) == -1)
                {
                    //tried all directories, if exec still error, cmd doesn't exist
                    printf("%s: Command not found\n",token[0]);
                }
            }
        }
    }
    exit( EXIT_SUCCESS );
}

/*
 Check if the command entered exists externally.
 In other words, check if the command entered requires forking off
 another process. If it does, return 1, else return 0;
 */
int IsExternal(char* token[], struct pids* head,
               struct suspended** head2, struct cmdHist **commandsHead, int token_count)
{
    //add the command to history of commands to keep track of
    if(token[0][0] != '!')
    {
        AddCmd(token, &(*commandsHead), token_count);
    }
    
    //if user enters quit or exit, exit with status 0
    if(strcmp("exit",token[0]) == 0 || strcmp("quit",token[0]) == 0)
    {
        exit(0);
    }
    //if user enters cd/chdir to change directories, do not fork
    //change the directory for parent process and return 0 to loop again
    else if(strcmp("cd",token[0]) == 0 || strcmp("chdir",token[0]) == 0)
    {
        //chdir returns -1, there was an error
        if(chdir(token[1]) == -1)
        {
            printf("cd: %s: No such file or directory\n",token[1]);
        }
        return 0;
    }
    //user requested to show pids
    else if(strcmp("showpids",token[0]) == 0)
    {
        ListPids(head);
        return 0;
    }
    else if(strcmp("bg",token[0]) == 0)
    {
        //if linked list is not empty, take process at the top of the stack
        //which would be the last suspended process and run it in the background
        //by sending a continue signal.
        //adjust the head and free up the spot we just ran since the process
        //is no longer suspended
        if(*head2 != NULL)
        {
            struct suspended* temp = *head2;
            *head2 = (*head2)->next;
            kill(temp->pid,SIGCONT);
            free(temp);
        }
        else
        {
            printf("bg: current: no such job\n");
        }
        return 0;
    }
    //pass head of history linked list
    else if(strcmp("history",token[0]) == 0)
    {
        ShowHistory(&(*commandsHead));
        return 0;
    }
    //check here if the command is of the form "!n"
    else if(token[0][0] == '!')
    {
        //extract what command was used at line "n" when history was displayed.
        //if it returns 1, the user entered valid input of the form "!n"
        //so recursively call to begin the command execution process
        if(ExtractCommand(&(*commandsHead),token) == 1)
        {
            return(IsExternal(token, &(*head), &(*head2), &(*commandsHead), token_count));
        }
        return 0;
    }
    
    //return 1 will occur if command entered is external (requires forking)
    return 1;
}

/*
 Check if a process is suspended. If it is, add it to linked list
 suspended, which keeps track of all suspended processes. The linked list
 is treated like a stack, so every time a suspended process is added, the
 newest one is going to be the head - which is the top of the stack.
 */
void CheckSuspended(struct suspended** head2, pid_t child_pid)
{
    /*
     if we are running this function, it means waitpid returned -1
    inside main - implies a function terminated/suspended
     */
    int status;
    /*
    check the process that was terminated/suspended and see if it was
    SUSPENDED not terminated
     */
    waitpid(child_pid, &status, WUNTRACED);
    /*
     according to the man page, WIFSTOPPED will return a nonzero value
    if the WUNTRACED is specified and the process was stopped by a signal.
    that means we need to add this process to the linked list of
    suspended processes
     */
    if( WIFSTOPPED(status) > 0)
    {
        //run this segment if the linked list is empty. it will create the
        //linked list for suspended processes.
        if(*head2 == NULL)
        {
            *head2 = (struct suspended*)malloc(sizeof(struct suspended*));
            (*head2)->pid = child_pid;
            (*head2)->next = NULL;
        }
        /*
        run this segment if linked list has items already. when we add
        new nodes, we will add at the head so we have a stack
         */
        else
        {
            struct suspended *curr =
            (struct suspended*)malloc(sizeof(struct suspended*));
            curr->pid = child_pid;
            curr->next = *head2;
            *head2 = curr;
        }
        
    }
    
}

/*
 This function adds the most recent command into a linked list
 with a size of max 15.
 */
void AddCmd(char *token[MAX_NUM_ARGUMENTS], struct cmdHist **head, int tokCount)
{
    //Get to the end of the linked list so we can append a new node
    struct cmdHist *temp = *head;
    int size = (*head)->cmdLength;
    
    while(temp->next != NULL)
    {
        temp = temp->next;
    }
    
    //Add a new node at the end of linked list
    temp->next = (struct cmdHist*)malloc(sizeof(struct cmdHist));
    temp = temp->next;
    temp->next = NULL;
    
    /*
     dynamically allocate a pointer to a pointer of chars.
     essentially this is an array of pointer to strings. this allows
     me to store the string command the user entered dynamically
     without knowing the size since i can't specify a length in
     the struct. tokCount represents the number of individual "words"
     that are spaced out in the command.
     */
    
    /*
    int tokSize = 0;
    while(token[tokSize] != NULL)
    {
        tokSize++;
    }
    tokSize++;
     */
    
    temp->token = (char**)malloc(sizeof(char*)*(tokCount-1));
    
    int i;
    //Copy each word from command entered and allocate memory for that string
    for(i = 0; i < tokCount-1; i++)
    {
        temp->token[i] = (char*)malloc(sizeof(char)*strlen(token[i]));
        strcpy(temp->token[i],token[i]);
    }
    //store the number of words in the command so we can print it later
    temp->cmdLength = tokCount-1;
    
    //If size is greater than 15, remove oldest element (head->next) and reassign
    if(size == 15)
    {
        temp = (*head)->next;
        (*head)->next = temp->next;
        free(temp);
        return;
    }
    (*head)->cmdLength++;
}

/*
 Show the last 15 commands entered by the user
 */
void ShowHistory(struct cmdHist **head)
{
    int i = 1;
    struct cmdHist *temp = *head;
    //skip over head since head contains no actual data
    temp = temp->next;
    //if size is 16, we need to skip one extra since we always store one extra
    //command in case the user requests the oldest command as it gets removed
    if((*head)->cmdLength >= 17)
    {
        temp = temp->next;
    }
    //loop through linked list and print contents
    while(temp != NULL)
    {
        printf("%d: %s",i,temp->token[0]);
        int j;
        //linked list stores a double pointer that points to single pointers
        //which point to strings, so need a nested loop.
        //one loop to go through linked list and the inner to go through each strings
        for(j = 1; j < temp->cmdLength; j++)
        {
            printf(" %s",temp->token[j]);
        }
        temp = temp->next;
        i++;
        printf("\n");
    }
    
}

/*
 Extract the command string in history linked list (user entered !n)
 */
int ExtractCommand(struct cmdHist **head, char *token[MAX_NUM_ARGUMENTS])
{
    //length represents the length of command entered if they entered '!'
    int length = (int)strlen(token[0])-1;
    int cmdNum = 0;
    char digit0 = token[0][1];
    char digit1 = token[0][2];
    
    /*
    if our input is "!n", we check here that "n" is a number between 0 - 15.
     if it's not, the format for using "!" is incorrect. if it is ok, we
     convert the input into an integer
     */
    if(length == 1 && digit0 > '0' && digit0 <= '9')
    {
        cmdNum = digit0 - 48;
    }
    else if(length == 2 && digit0 == '1' && digit1 > '0' && digit1 <= '5')
    {
        cmdNum = (digit0 - 48) * 10;
        cmdNum += digit1 - 48;
    }
    else
    {
        printf("Command not in history.\n");
        return 0;
    }
    
    //one last check, make sure the number the user enters does not exceed size
    if(cmdNum > (*head)->cmdLength)
    {
        printf("Command not in history.\n");
        return 0;
    }
    
    /*
     the cmdlength inside head represents the size of the linked list.
     if the size is 16, we need to skip ahead an extra node because
     as the command was entered, a node was removed which messes up
     the index of the command the user wanted. also, the reason why the limit is
     16 instead  because i keep an extra node in case the user request
     "!0" since that will be the node that is removed when the size limit (15)
     is reached.
     */
    struct cmdHist *temp = *head;
    int i = 1;
    //skip over the head node, it doesn't contain any command data
    temp = temp->next;
    if((*head)->cmdLength >= 16)
    {
        temp = temp->next;
    }
    //i is counter. when we reach the i'th node where i = cmdNum, that's the node
    //the user requested
    while(i != cmdNum)
    {
        temp = temp->next;
        i++;
    }
    
    //we have the command in history the user wants, assign token to it
    //so when we get back we can execute the command in token
    for(i = 0; i < temp->cmdLength; i++)
    {
        token[i] = temp->token[i];
    }
    
    return 1;
}

int main()
{
    //initialize linked list of that stores recent commands
    struct cmdHist *commandsHead = (struct cmdHist*)malloc(sizeof(struct cmdHist));
    commandsHead->next = NULL;
    //head will not represent a command, it will contain size of the linked list
    commandsHead->cmdLength = 0;
    
    //this will keep track of the size of the pids linked list
    int size = 0;
    
    //create the head of the pids linked list
    struct pids *head = (struct pids*)malloc(sizeof(struct pids));
    head->next = NULL;
    head->pid = getpid();
    
    //instantiate the head of the suspended processes linked list
    struct suspended *head2 = NULL;
    
    struct sigaction act;
    
    /*
     Zero out the sigaction struct
     */
    memset (&act, '\0', sizeof(act));
    
    /*
     Set the handler to use the function handle_signal()
     */
    //act.sa_handler = &handle_signal;
    act.sa_handler = &handle_signal;
    
    /*
     Install the handler and check the return value.
     */
    if (sigaction(SIGTSTP , &act, NULL) < 0)
    {
        perror ("sigaction: ");
        return 1;
    }
    else if (sigaction(SIGINT , &act, NULL) < 0)
    {
        perror ("sigaction: ");
        return 1;
    }
    

    
    char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );
    
    while( 1 )
    {
        // Print out the msh prompt
        printf ("msh> ");
        
        // Read the command from the commandline.  The
        // maximum command that will be read is MAX_COMMAND_SIZE
        // This while command will wait here until the user
        // inputs something since fgets returns NULL when there
        // is no input
        while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );
        
        /* Parse input */
        char *token[MAX_NUM_ARGUMENTS];
        
        int   token_count = 0;
        
        // Pointer to point to the token
        // parsed by strsep
        char *arg_ptr;
        
        char *working_str  = strdup( cmd_str );
        
        // we are going to move the working_str pointer so
        // keep track of its original value so we can deallocate
        // the correct amount at the end
        char *working_root = working_str;
        
        // Tokenize the input stringswith whitespace used as the delimiter
        while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) &&
               (token_count<MAX_NUM_ARGUMENTS))
        {
            token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
            if( strlen( token[token_count] ) == 0 )
            {
                token[token_count] = NULL;
            }
            token_count++;
        }
        
        //if user doesn't enter anything, restart loop
        if(token[0] == NULL)
        {
            continue;
        }
        
        //first check if the command entered requires forking (external cmd)
        //if it doesn't, we will execute the command in the parent and
        //restart the loop, skipping over the fork
        if(IsExternal(token,head,&head2,&commandsHead, token_count) == 0)
        {
            continue;
        }
        
        pid_t parent_pid = getpid();
        //fork off a child to begin executing command line arguments
        pid_t child_pid = fork();
        
        int status;
        //if we're in child, let's try to execute that command (searchbin does this)
        if( child_pid == 0)
        {
            SearchBin(token, token_count, parent_pid);
        }
        
        //parent will wait for child here and child will end here
        //if waitpid returns -1, the process was terminated or stopped
        //call checksuspended to see which it is
        if(waitpid( child_pid, &status, 0) == -1)
        {
            CheckSuspended(&head2, child_pid);
        }
        
        //we know that if we reached this spot that a child process ran.
        //add the pid of that process to the linked list of pids
        //and save the size after adding it
        size = AddPid(size, &head, child_pid, status);
        
        free( working_root );
        
        //return 0;
        
    }
    return 0;
}
