/***************************************************************************
 *  Title: Runtime environment 
 * -------------------------------------------------------------------------
 *    Purpose: Runs commands
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.3 $
 *    Last Modification: $Date: 2009/10/12 20:50:12 $
 *    File: $RCSfile: runtime.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: runtime.c,v $
 *    Revision 1.3  2009/10/12 20:50:12  jot836
 *    Commented tsh C files
 *
 *    Revision 1.2  2009/10/11 04:45:50  npb853
 *    Changing the identation of the project to be GNU.
 *
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.6  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.5  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.4  2002/10/21 04:49:35  sempi
 *    minor correction
 *
 *    Revision 1.3  2002/10/21 04:47:05  sempi
 *    Milestone 2 beta
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __RUNTIME_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

/************Private include**********************************************/
#include "runtime.h"
#include "io.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/

#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))
/*
typedef struct bgjob_l
{
  pid_t pid;
  struct bgjob_l* next;
} bgjobL;
*/
int jobNum = 1;
/* the pids of the background processes */
bgjobL *bgjobs = NULL;
/* the pid of fore ground process, if the shell is in fore ground */ 
pid_t fgjob = 0;

status_t fgStatus;
char* fgCmd;

/************Function Prototypes******************************************/
/* run command */
static void
RunCmdFork(commandT*, bool);
/* runs an external program command after some checks */
static void
RunExternalCmd(commandT*, bool);
/* resolves the path and checks for exutable flag */
static bool
ResolveExternalCmd(commandT*);
/* forks and runs a external program */
static void
Exec(commandT*, bool);
/* runs a builtin command */
static void
RunBuiltInCmd(commandT*);
/* checks whether a command is a builtin command */
static bool
IsBuiltIn(char*);
/* find the full path of a command */
char * 
findCommand(commandT* cmd);
/* I/O redirection */
static void
IoRedirection(commandT* cmd);
/* pipe symbol command */
int
pipeCommand(commandT* cmd, int head);
/* find path */
/************External Declaration*****************************************/

/**************Implementation***********************************************/

  int pp[100][2];
  int command = 0;

/*
 * RunCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs the given command.
 */
void
RunCmd(commandT* cmd)
{
	if (strcmp(cmd->argv[cmd->argc-1], "&") == 0)
		RunCmdFork(cmd, FALSE);
	else
  	RunCmdFork(cmd, TRUE);
} /* RunCmd */


/*
 * RunCmdFork
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool fork: whether to fork
 *
 * returns: none
 *
 * Runs a command, switching between built-in and external mode
 * depending on cmd->argv[0].
 */
void
RunCmdFork(commandT* cmd, bool fork)
{
  if (cmd->argc <= 0)
    return;
  if (IsBuiltIn(cmd->argv[0]))
    {
      RunBuiltInCmd(cmd);
    }
  else
    {
      RunExternalCmd(cmd, fork);
    }
} /* RunCmdFork */


/*
 * RunCmdBg
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs a command in the background.
 */
void
RunCmdBg(commandT* cmd)
{
  // TODO
} /* RunCmdBg */


/*
 * RunCmdPipe
 *
 * arguments:
 *   commandT *cmd1: the commandT struct for the left hand side of the pipe
 *   commandT *cmd2: the commandT struct for the right hand side of the pipe
 *
 * returns: none
 *
 * Runs two commands, redirecting standard output from the first to
 * standard input on the second.
 */
void
RunCmdPipe(commandT* cmd1, commandT* cmd2)
{
} /* RunCmdPipe */


/*
 * RunCmdRedirOut
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   char *file: the file to be used for standard output
 *
 * returns: none
 *
 * Runs a command, redirecting standard output to a file.
 */
void
RunCmdRedirOut(commandT* cmd, char* file)
{
} /* RunCmdRedirOut */


/*
 * RunCmdRedirIn
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   char *file: the file to be used for standard input
 *
 * returns: none
 *
 * Runs a command, redirecting a file to standard input.
 */
void
RunCmdRedirIn(commandT* cmd, char* file)
{
}  /* RunCmdRedirIn */


/*
 * RunExternalCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool fork: whether to fork
 *
 * returns: none
 *
 * Tries to run an external command.
 */
static void
RunExternalCmd(commandT* cmd, bool fork)
{
  if (ResolveExternalCmd(cmd))
    Exec(cmd, fork);
}  /* RunExternalCmd */


/*
 * findCommand
 * arguments:
 *   commandT * cmd: the command
 * returns: 
 *   full path to the command   - if found
 *   NULL                       - otherwise
 * This function only finds standalone commands
 * Built in commands should be tested separately
 */ 

char * 
findCommand(commandT* cmd)
{
  char * fullPath = NULL;

  if (cmd->name[0] == '.')  
    {
      /* command start with . are searched within local folder  */
      fullPath = getcwd(NULL, PATHBUFSIZE);
      strcat(fullPath, "/");
      strcat(fullPath, cmd->name);
      if (access(fullPath, X_OK) != 0) 
	{
	  free(fullPath);
	  fullPath = NULL;
	}
    }
  else if (cmd->name[0] == '/') 
    {
      /* command start with / are searched from root path  */
      fullPath = (char *) malloc(PATHBUFSIZE);
      strcpy(fullPath, cmd->name);
      if(access(fullPath, X_OK) != 0)
	{
	  free(fullPath);
	  fullPath = NULL;
	}
    }
  else
    {
      /* other commands are searched in PATH*/
      bool found = FALSE;
      char * path = malloc(PATHBUFSIZE);
      strcpy(path, getenv("PATH"));
      char * pathItem = strtok(path, ":");

      fullPath = malloc(PATHBUFSIZE);

      while (pathItem != NULL) 
	{
	  strcpy(fullPath, pathItem);
	  strcat(fullPath, "/");
	  strcat(fullPath, cmd->name);
	  if(access(fullPath, X_OK) == 0)
	    {
	      found = TRUE;
	      break;
	    }
	  pathItem = strtok(NULL, ":");
	}
      
      if (found == FALSE) 
	{
	  free(fullPath);
	  fullPath = NULL;
	}
      free(path);
    }
  return fullPath;
} /* findCommand */


/*
 * ResolveExternalCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: bool: whether the given command exists
 *
 * Determines whether the command to be run actually exists.
 */
static bool
ResolveExternalCmd(commandT* cmd)
{
  char * fullPath;
  if ((fullPath = findCommand(cmd)) == NULL) 
    {
#ifdef DEBUG_OUTPUT
      printf("%s does not exist\n", cmd->name);
#endif
      printf("%s: line %d: %s: command not found\n", "/bin/bash", lineNum + 2, cmd->name);
      return FALSE;
    } 
  else
    {
#ifdef DEBUG_OUTPUT
      printf("%s exists\n", fullPath);
#endif
      free(fullPath);
      return TRUE;
    }
} /* ResolveExternalCmd */


/*
 * Exec
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool forceFork: whether to fork
 *
 * returns: none
 *
 * Executes a command.
 */
static void
Exec(commandT* cmd, bool forceFork)
{
	sigset_t chldSigset;
	sigemptyset(&chldSigset);
	sigaddset(&chldSigset, SIGCHLD);
	sigprocmask(SIG_BLOCK, &chldSigset, NULL);

	pid_t child_id = fork();
	if (child_id == 0) // child
	{
		char* fullPath = findCommand(cmd);
		setpgid(0, 0);
		sigprocmask(SIG_UNBLOCK, &chldSigset, NULL);
		if (!forceFork)
			cmd->argv[cmd->argc-1] = 0;
 		int i, flag = 0;
    for (i = 0; i < cmd->argc; i++)
    {
    	if (strchr(cmd->argv[i], '<') != NULL ||
            strchr(cmd->argv[i], '>') != NULL)
      {
      	flag = 1;
        break;
      }
     } 
     if (flag == 1)
     {
       IoRedirection(cmd);
     }          
     if(pipeCommand(cmd, 0))
       	return;
		execv(fullPath, cmd->argv);
	}
	else // parent
	{
		char* command = malloc(PATHBUFSIZE);
		strcpy(command, cmd->argv[0]);
		int i;
		for (i = 1; i < cmd->argc; i++)
		{
			strcat(command, " ");
			if (strcmp(cmd->argv[0], "bash") == 0 && i == 2)
				strcat(command, "\"");
			strcat(command, cmd->argv[i]);
			if (strcmp(cmd->argv[0], "bash") == 0 && i == 2)
				strcat(command, "\"");
		}

		if (forceFork)
		{
			fgjob = child_id;
			fgStatus = BUSY;
			strcpy(fgCmd, command);

			sigprocmask(SIG_UNBLOCK, &chldSigset, NULL);
			while (fgStatus != AVAIL)
			{
				sleep(1);
			}
		}
		else
		{	
			fgjob = 0;
			fgStatus = AVAIL;
			command[strlen(command)-1] = '\0';
			addJob(command, child_id, RUNNING);
			sigprocmask(SIG_UNBLOCK, &chldSigset, NULL);
		}
		free(command);
	}
} /* Exec */

int 
pipeCommand(commandT* cmd, int head)
{
  int tail = 0, i = 0, j = 0;
  char *argv[10];
  char *path;
  pid_t cid;
  
  fflush(stdout);
  for(tail=head; tail<cmd->argc-1 && cmd->argv[tail][0] != '|' ; tail++);
    
  if(head == 0 && tail == cmd->argc -1)
     return 0;     
 
  /* preparing for the execution  */ 
  if(tail != cmd->argc-1)
  {
  	for(i=head,j=0; i<tail; i++, j++)
      		argv[j] = cmd->argv[i]; 
    	argv[j] = (char *) 0;
  }
  else
  {
        for(i=head,j=0; i<=tail; i++, j++)
     		argv[j] = cmd->argv[i]; 
    	argv[j] = (char *) 0;
  }
        
  cmd->name = argv[0];
  path = findCommand(cmd);

  /* creat a pipe before fork() */
  command++;
  pipe(pp[command]);

  /* fork a child, execute the command 
     and put the result into pipe[command][0] */ 
  cid = fork();
  if(cid == 0)
  {
  	if(head == 0)
        {
		close(pp[command][0]);
                dup2(pp[command][1], STDOUT_FILENO);
	        close(pp[command][1]); 
        }
        else if(tail != cmd->argc-1)
        {       
                int cl;
                for(cl=1; cl < command-1; cl++)
                {
                	close(pp[cl][0]);
                        close(pp[cl][1]);
                }                 
                close(pp[command-1][1]);
	        dup2(pp[command-1][0], STDIN_FILENO);
	        close(pp[command-1][0]);
                                             
                close(pp[command][0]);
	        dup2(pp[command][1], STDOUT_FILENO);
	        close(pp[command][1]);        
        }
        else
        {
                int cl;
                for(cl=1; cl < command-1; cl++)
                {
                	close(pp[cl][0]);
                        close(pp[cl][1]);
                }
                close(pp[command-1][1]);
	        dup2(pp[command-1][0], STDIN_FILENO);
	        close(pp[command-1][0]);
                                             
                close(pp[command][0]);
	        close(pp[command][1]);
        }               
                execv(path, argv);        
  }
  else
  {  
 	head = tail+1; 
        if(head > cmd->argc)
           return 1;
        pipeCommand(cmd, head);
  }

  wait(0);
  return 1;
}


static void
IoRedirection(commandT* cmd)
{
  int i, j, ifp, ofp;
  /* output redirection  */
  for(i=1; i<cmd->argc; i++)
  {
    if(cmd->argv[i][0] == '>')
    {
      ofp = open(cmd->argv[i+1], O_RDWR | O_CREAT, S_IRWXU);
      close(1);
      dup2(ofp,1);
      close(ofp);
      cmd->argv[i] = NULL;
      cmd->argc = cmd->argc - 2;
    }  
  }
  /* input redirection*/
    
  for(i=1; i<cmd->argc; i++)
  {
    if(cmd->argv[i][0] == '<')
    {
      ifp = open(cmd->argv[i+1], O_RDWR | O_CREAT, S_IRWXU);
      close(0);
      dup2(ifp,0);
      close(ifp);
      for(j=i; j<cmd->argc; j++)
        cmd->argv[j] = cmd->argv[j+2];
      cmd->argc = cmd->argc - 2;
    }
  }



}


/*
 * IsBuiltIn
 *
 * arguments:
 *   char *cmd: a command string (e.g. the first token of the command line)
 *
 * returns: bool: TRUE if the command string corresponds to a built-in
 *                command, else FALSE.
 *
 * Checks whether the given string corresponds to a supported built-in
 * command.
 */
static bool
IsBuiltIn(char* cmd)
{
  if (strcmp(cmd, "cd") == 0 ||  
      strcmp(cmd, "exit") == 0 ||
      strcmp(cmd, "bg") == 0 ||
      strcmp(cmd, "fg") == 0 ||
      strcmp(cmd, "jobs") == 0 ||
      strchr(cmd, '=') != NULL ||   /* set env */
      strcmp(cmd, "alias") == 0 ||
      strcmp(cmd, "unalias") == 0
      )
    {
      return TRUE;
    }
  else
    {
      return FALSE;
    }
} /* IsBuiltIn */


/*
 * RunBuiltInCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs a built-in command.
 */
static void
RunBuiltInCmd(commandT* cmd)
{
	bgjobL* job;
	int num;
  if (strcmp(cmd->name, "cd") == 0) 
    {
      char * newPath;
      if (cmd->argc == 1)
        {
          newPath=(char *)malloc(PATHBUFSIZE);
          strcpy(newPath, getenv("HOME"));
        }
      else if (cmd->argc == 2) //cd takes exactly one argument
	{
          if (cmd->argv[1][0] == '/')
            {
              /* cd to absolute path */
              newPath = (char *)malloc(PATHBUFSIZE);
              strcpy(newPath, cmd->argv[1]);
            }
          else if (strcmp(cmd->argv[1], "~/") == 0)
            {
              newPath=(char *)malloc(PATHBUFSIZE);
              strcpy(newPath, getenv("HOME"));
            }
          else
            {
              /* cd to relative path */
              newPath = getcwd(NULL, PATHBUFSIZE);
              strcat(newPath, "/");
              strcat(newPath, cmd->argv[1]);
            }
       }
     else
        {
          return;
        }
      chdir(newPath);
      free(newPath);
    }
  else if (strcmp(cmd->name, "exit") == 0) 
    {
      if (strcmp(getenv("SHELL"), "./tsh") == 0 || strcmp(getenv("SHELL"), "../tsh") == 0)
	{
          /* print exit before quit */
	  printf("exit\n");
          fflush(stdout);
	}
      forceExit = TRUE;
    }
  else if (strchr(cmd->name, '=') != NULL)
    {
      char * var = (char *) malloc (PATHBUFSIZE);
      strcpy(var, cmd->name);
      char * value = strchr(var, '=');
      *value = '\0';
      value ++;
      setenv(var, value, 1);
      free(var);
      fflush(stdout);
    }
	else if (strcmp(cmd->name, "bg") == 0)
	{
		if (cmd->argc == 2)
		{
			num = atoi(cmd->argv[1]);
			job = searchJobByNum(num);
			
			if (job != NULL && job->state == STOPPED)
			{
				transitProcState(job, RUNNING);
				kill(-job->pid, SIGCONT);
			}
		}
		else
		{
			job = searchJobByState(STOPPED);
			if (job != NULL)
			{
				transitProcState(job, RUNNING);
				kill(-job->pid, SIGCONT);
			}
		}
	}
	else if (strcmp(cmd->name, "fg") == 0)
	{
		if (cmd->argc == 2)
		{
			num = atoi(cmd->argv[1]);
			job = searchJobByNum(num);

			if(job != NULL && job->state != TERMINATED)
			{
				fgjob = job->pid;
				fgStatus = BUSY;
				strcpy(fgCmd, job->cmd);
				if (job->state == STOPPED)	
					kill(-fgjob, SIGCONT);	// SIGCONT not going to sig
				removeJob(job);
				while (fgStatus != AVAIL)
				{
					sleep(1);
				}
			}
		}
		else
		{
			job = searchJobByState(RUNNING);
			if (job == NULL)
				job = searchJobByState(STOPPED);
			if (job != NULL)
			{
				fgjob = job->pid;
				fgStatus = BUSY;
				strcpy(fgCmd, job->cmd);
				setpgid(fgjob, fgjob);
				if (job->state == STOPPED)
					kill(-fgjob, SIGCONT);
				removeJob(job);
				while (fgStatus != AVAIL)
				{
					sleep(1);
				}
			}
		}
	}
	else if (strcmp(cmd->name, "jobs") == 0)
	{
		int i;
		bgjobL* job;
		for (i = 1; i < jobNum; i++)
		{
			job = searchJobByNum(i);
			if (job != NULL && job->state == RUNNING)
			{
				printf("[%d]\tRunning\t\t\t%s\t&\n", job->num, job->cmd);
				fflush(stdout);
			}
			if (job != NULL && job->state == STOPPED)
			{
				printf("[%d]\tStopped\t\t\t%s\n", job->num, job->cmd);
				fflush(stdout);
			}

		}
	}
  else if (strcmp(cmd->name, "alias") == 0)
    {
      /* print the alias map or create a new alias */
      if (cmd->argc == 1)
        {
          printAlias();
        }
      else if (cmd->argc == 2)
        {
          createAlias(cmd->argv[1]);
        }
    }
  else if (strcmp(cmd->name, "unalias") == 0 && cmd->argc == 2)
    {
      /* remove an alias entry from the alias map */
      if (!removeAlias(cmd->argv[1]))
        {
          printf("%s: line %d: %s: %s: not found\n", "/bin/bash",
            lineNum + 2, "unalias", cmd->argv[1]);
        }
    }
} /* RunBuiltInCmd */


/*
 * CheckJobs
 *
 * arguments: none
 *
 * returns: none
 *
 * Checks the status of running jobs.
 */
void
CheckJobs()
{ 

	bgjobL* job = bgjobs;
	while (job != NULL)
	{
		if (job->state == RUNNING || job->state == STOPPED)
			return;
//		printf("job num:%d\t\t%s\n", job->num, job->cmd);
		job = job->next;
	}
	jobNum = 1;
} /* CheckJobs */

/*
 * GetPrompt
 *
 * arguments: none
 *
 * returns: none
 */
void
PrintPrompt()
{
  char * prompt = getenv("PS1");

  char * promptout = (char *) malloc(PATHBUFSIZE);
  strcpy(promptout, "");

  if (prompt != NULL)
    {
      while (*prompt != '\0') 
	{
	  if (*prompt != '\\') 
	    {					
	      /* copy regular charactors */
	      char tmp[2] = {*prompt, '\0'};
	      strcat(promptout, tmp);
	    }
	  else 
	    {
	      ++ prompt;
	      if (*prompt == 'u')
		/* get user name */
		strcat(promptout, getlogin());
	      else if (*prompt == 'h') 
		{
		  /* get hostname */
		  char buf[PATHBUFSIZE];
		  gethostname(buf, PATHBUFSIZE);
		  strcat(promptout, strtok(buf, "."));
		}
	      else if (*prompt == 'w') 
		{
		  /* get cwd */
		  char buf[PATHBUFSIZE];
		  getcwd(buf, PATHBUFSIZE);
		  strcat(promptout, buf);
		}
	      else if (*prompt == 't') 
		{
		  /* get current time */
		  time_t t = time(NULL);
		  struct tm * tmp = localtime(&t);
		  char buf[PATHBUFSIZE];
		  strftime(buf, PATHBUFSIZE, "%H:%M:%S", tmp);
		  strcat(promptout, buf);
		}
	    }
	  ++ prompt;
	}
    }
  printf("%s", promptout);  
  free(promptout);
  fflush(stdout); /* make sure the prompt is printed */
  return;
}

void
transitProcState(bgjobL* job, state_t newState)
{
	bgjobL* temp = NULL;

	job->state = newState;
	if ((newState != TERMINATED) && (job->prev != NULL))
	{
		temp = job->prev;
		temp->next = job->next;
		temp = job->next;
		temp->prev = job->prev;
		job->prev = NULL;
		job->next = bgjobs;
		bgjobs = job;
	}
}

bgjobL*
searchJobByID(pid_t pid)
{
	bgjobL* job = bgjobs;

	while (job != NULL)
	{
		if (job->pid == pid)
			return job;
		job = job->next;
	}
	return job;
}

bgjobL*
searchJobByNum(int num)
{
	bgjobL* job = bgjobs;
	while (job != NULL)
	{
		if (job->num == num)
			return job;
		job = job->next;
	}
	return job;
}

bgjobL*
searchJobByState(state_t state)
{
	bgjobL* job = bgjobs;
	while (job != NULL)
	{
		if (job->state == state)
			return job;
		job = job->next;
	}
	return job;
}

void
addJob(char* command, pid_t pid, state_t state)
{
		bgjobL* newProc = malloc(sizeof(bgjobL));
		newProc->cmd = malloc(PATHBUFSIZE);
		newProc->pid = pid;
		newProc->state = state;
		newProc->num = jobNum++;
		strcpy(newProc->cmd, command);	
		newProc->prev = NULL;
		newProc->next = bgjobs;
		bgjobs = newProc;
			
#ifdef DEBUG_OUTPUT
			printf("jobs in background!\n");
			while (newProc != NULL)
			{
				printf("pid: %d\nstate: %d\n", newProc->pid, newProc->state);
				newProc = newProc->next;
			}
#endif
}

void
removeJob(bgjobL* job)
{
	bgjobL* temp;
	if (job->prev == NULL && job->next == NULL) // ONLY JOB
	{
		bgjobs = NULL;
	}
	else if (job->prev == NULL) // FIRST JOB
	{
		temp = job->next;
		temp->prev = NULL;
		bgjobs = temp;
	}
	else if (job->next == NULL) // LAST JOB
	{
		temp = job->prev;
		temp->next = NULL;
	}
	else
	{
		temp = job->prev;
		temp->next = job->next;
		temp = job->next;
		temp->prev = job->prev;
	}
	job->prev = NULL;
	job->next = NULL;
  job->num = 0;
	job->state = TERMINATED;
	free(job->cmd);
	free(job);
	fflush(stdin);
	fflush(stdout);
	CheckJobs();
}
