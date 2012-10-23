/***************************************************************************
 *  Title: tsh
 * -------------------------------------------------------------------------
 *    Purpose: A simple shell implementation 
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.4 $
 *    Last Modification: $Date: 2009/10/12 20:50:12 $
 *    File: $RCSfile: tsh.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
#define __MYSS_IMPL__

/************System include***********************************************/
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/************Private include**********************************************/
#include "tsh.h"
#include "io.h"
#include "interpreter.h"
#include "runtime.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

#define BUFSIZE 80

/************Global Variables*********************************************/

/************Function Prototypes******************************************/
/* handles SIGINT and SIGSTOP signals */
static void
sig(int);

/************External Declaration*****************************************/

/*
 * runtshrc
 * arguments: void
 * returns: void
 * read and execute commands in ~/.tshrc
 */
void
runtshrc()
{
  /* find the .tshrc file */
  char * home = getenv("HOME");
  char * tshrcPath = (char *) malloc(PATHBUFSIZE);
  strcpy(tshrcPath, home);
  strcat(tshrcPath, "/.tshrc");
  if (access(tshrcPath, R_OK) == 0) /* check read permission */
    {
      int size = BUFSIZE;
      char * cmd = (char *) malloc(size);
      /* open the file */
      FILE * fp = fopen(tshrcPath, "r");
      char ch;
      size_t used = 0;
      while ((ch = getc(fp)) != EOF) /* walk through the file */
	{
	  if (used == size)
	    {
	      size *= 2;
	      cmd = realloc(cmd, sizeof(char) * (size + 1));
	    }
	  /* handle a command line */
	  if (ch == '\n')
	    {
	      if (cmd[0] != '#') 
		{
		  Interpret(cmd);
		}
	      used = 0;
	      continue;
	    }
	  cmd[used] = ch;
	  used ++;
	  cmd[used] = '\0';
	}
      fclose(fp);
      free(cmd);
    }
  free(tshrcPath);
}

/**************Implementation***********************************************/

/*
 * main
 *
 * arguments:
 *   int argc: the number of arguments provided on the command line
 *   char *argv[]: array of strings provided on the command line
 *
 * returns: int: 0 = OK, else error
 *
 * This sets up signal handling and implements the main loop of tsh.
 */
int
main(int argc, char *argv[])
{
  /* Initialize command buffer */
  char* cmdLine = malloc(sizeof(char*) * BUFSIZE);

  /* shell initialization */
  if (signal(SIGINT, sig) == SIG_ERR)
    PrintPError("SIGINT");
  if (signal(SIGTSTP, sig) == SIG_ERR)
    PrintPError("SIGTSTP");

  if (strcmp(getenv("SHELL"), "./tsh") == 0 || strcmp(getenv("SHELL"), "../tsh") == 0)
    {
      runtshrc();
    }

  while (!forceExit) /* repeat forever */
    {
      /* print prompt */
      PrintPrompt();
      
      /* read command line */
      getCommandLine(&cmdLine, BUFSIZE);

      /* checks the status of background jobs */
      CheckJobs();

      /* interpret command and line
       * includes executing of commands */
      if (forceExit != TRUE) 
	{
	  Interpret(cmdLine);
	}
    }

  /* shell termination */
  free(cmdLine);
  freeAliasMap();
  fflush(stdin);
  fflush(stdout);

  return 0;
} /* main */

/*
 * sig
 *
 * arguments:
 *   int signo: the signal being sent
 *
 * returns: none
 *
 * This should handle signals sent to tsh.
 */
extern bool IsReading();

extern pid_t fgjob;

static void
sig(int signo)
{
  if (signo == SIGINT) 
    {
      /* only handle SIGINT for now */
      if (fgjob == 0) 
	{
	  /* if there is no fore ground job, exit (same as tsh-ref) */
	  exit(0);
	}
      else 
	{
	  /* kill the fore ground job */
	  if (getpid() != fgjob) 
	    {
	      /* kill the fore ground job from the shell process */
	      kill(fgjob, SIGINT);
	      fgjob = 0;
	    }
	}
    }
} /* sig */
