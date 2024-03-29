/***************************************************************************
 *  Title: Input 
 * -------------------------------------------------------------------------
 *    Purpose: Handles the input from stdin
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.4 $
 *    Last Modification: $Date: 2009/10/12 20:50:12 $
 *    File: $RCSfile: interpreter.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: interpreter.c,v $
 *    Revision 1.4  2009/10/12 20:50:12  jot836
 *    Commented tsh C files
 *
 *    Revision 1.3  2009/10/11 04:45:50  npb853
 *    Changing the identation of the project to be GNU.
 *
 *    Revision 1.2  2008/10/10 00:12:09  jot836
 *    JSO added simple command line parser to interpreter.c. It's not pretty
 *    but it works. Handles quoted strings, and preserves backslash behavior
 *    of bash. Also, added simple skeleton code as well as code to create and
 *    free commandT structs given a command line.
 *
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.4  2002/10/24 21:32:47  sempi
 *    final release
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
#define __INTERPRETER_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "string.h"

/************Private include**********************************************/
#include "interpreter.h"
#include "io.h"
#include "runtime.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */
typedef struct string_l
{
  char* s;
  struct string_l* next;
} stringL;

int BUFSIZE = 512;
int MAXARGS = 100;

struct alias_entry
{
  char* name;
  char* text;
  struct alias_entry* prev;
  struct alias_entry* next;
};
typedef struct alias_entry Alias;
Alias* aliasMap;
bool aliasExpanded = FALSE;

/**************Function Prototypes******************************************/

commandT*
getCommand(char* cmdLine);

void
resolveHome(commandT* cmd);

void
expandCmdWithAlias(commandT* cmd, int n);

char*
getAlias(char* cmdName);

Alias*
findAlias(char* name);

void
freeCommand(commandT* cmd);

void
updateEnv(commandT* cmd);
/**************Implementation***********************************************/

/*
 * Interpret
 *
 * arguments:
 *   char *cmdLine: pointer to the command line string
 *
 * returns: none
 *
 * This is the high-level function called by tsh's main to interpret a
 * command line.
 */
void
Interpret(char* cmdLine)
{
  commandT* cmd = getCommand(cmdLine);
  updateEnv(cmd);

#ifdef DEBUG_OUTPUT
  printf("argc: %d\n", cmd->argc);
  int i = 0;
  for (i = 0; cmd->argv[i] != 0; i++)
    {
      printf("#%d|%s|\n", i, cmd->argv[i]);
    }
#endif //DEBUG_OUTPUT

  RunCmd(cmd);
  freeCommand(cmd);
} /* Interpret */


/*
 * getCommand
 *
 * arguments:
 *   char *cmdLine: pointer to the command line string
 *
 * returns: commandT*: pointer to the commandT struct generated by
 *                     parsing the cmdLine string
 *
 * This parses the command line string, and returns a commandT struct,
 * as defined in runtime.h.  You must free the memory used by commandT
 * using the freeCommand function after you are finished.
 *
 * This function tokenizes the input, preserving quoted strings. It
 * supports escaping quotes and the escape character, '\'.
 */
commandT*
getCommand(char* cmdLine)
{
  commandT* cmd = malloc(sizeof(commandT) + sizeof(char*) * MAXARGS);
  cmd->argv[0] = 0;
  cmd->name = 0;
  cmd->argc = 0;

  int i, inArg = 0;
  char quote = 0;
  char escape = 0;

  // Set up the initial empty argument
  char* tmp = malloc(sizeof(char*) * BUFSIZE);
  int tmpLen = 0;
  tmp[0] = 0;

  //printf("parsing:%s\n", cmdLine);

  for (i = 0; cmdLine[i] != 0; i++)
    {
      //printf("\tindex %d, char %c\n", i, cmdLine[i]);

      // Check for whitespace
      if (cmdLine[i] == ' ')
        {
          if (inArg == 0)
            continue;
          if (quote == 0)
            {
              // End of an argument
              //printf("\t\tend of argument %d, got:%s\n", cmd.argc, tmp);
              cmd->argv[cmd->argc] = malloc(sizeof(char) * (tmpLen + 1));
              strcpy(cmd->argv[cmd->argc], tmp);

              inArg = 0;
              tmp[0] = 0;
              tmpLen = 0;
              cmd->argc++;
              cmd->argv[cmd->argc] = 0;
              continue;
            }
        }

      // If we get here, we're in text or a quoted string
      inArg = 1;

      // Start or end quoting.
      if (cmdLine[i] == '\'' || cmdLine[i] == '"')
        {
          if (escape != 0 && quote != 0 && cmdLine[i] == quote)
            {
              // Escaped quote. Add it to the argument.
              tmp[tmpLen++] = cmdLine[i];
              tmp[tmpLen] = 0;
              escape = 0;
              continue;
            }

          if (quote == 0)
            {
              //printf("\t\tstarting quote around %c\n", cmdLine[i]);
              quote = cmdLine[i];
              continue;
            }
          else
            {
              if (cmdLine[i] == quote)
                {
                  //printf("\t\tfound end quote %c\n", quote);
                  quote = 0;
                  continue;
                }
            }
        }

      // Handle escape character repeat
      if (cmdLine[i] == '\\' && escape == '\\')
        {
          escape = 0;
          tmp[tmpLen++] = '\\';
          tmp[tmpLen] = 0;
          continue;
        }

      // Handle single escape character followed by a non-backslash or quote character
      if (escape == '\\')
        {
          if (quote != 0)
            {
              tmp[tmpLen++] = '\\';
              tmp[tmpLen] = 0;
            }
          escape = 0;
        }

      // Set the escape flag if we have a new escape character sequence.
      if (cmdLine[i] == '\\')
        {
          escape = '\\';
          continue;
        }

      tmp[tmpLen++] = cmdLine[i];
      tmp[tmpLen] = 0;
    }
  // End the final argument, if any.
  if (tmpLen > 0)
    {
      //printf("\t\tend of argument %d, got:%s\n", cmd.argc, tmp);
      cmd->argv[cmd->argc] = malloc(sizeof(char) * (tmpLen + 1));
      strcpy(cmd->argv[cmd->argc], tmp);

      inArg = 0;
      tmp[0] = 0;
      tmpLen = 0;
      cmd->argc++;
      cmd->argv[cmd->argc] = 0;
    }

  free(tmp);

  expandCmdWithAlias(cmd, 0);

  resolveHome(cmd);

  cmd->name = cmd->argv[0];

  return cmd;
} /* getCommand */

/*
 * resolveHome
 *
 * arguments:
 *   commandT* cmd: the pointer to the command
 *
 * returns: none
 *
 * This function finds the string "~/" in the command, and
 * replace it with the absolute home directory.
 */
void
resolveHome(commandT* cmd)
{
  int i;
  for (i = 0; i < cmd->argc; i++)
    {
      if (strcmp(cmd->argv[i], "~/") == 0)
        {
          char * newPath=(char *)malloc(PATHBUFSIZE);
          strcpy(newPath, getenv("HOME"));
          free(cmd->argv[i]);
          cmd->argv[i] = newPath;
        }
    }
}

/*
 * expandCmdWithAlias
 *
 * arguments:
 *   commandT* cmd: the pointer to the command that needs to be
 *   expanded
 *
 * returns: none
 *
 * This function expands any alias in the given command to the
 * full text of that alias. The command is directly modified
 * via its pointer, so nothing needs to be returned.
 */
void
expandCmdWithAlias(commandT* cmd, int n)
{
  /* get the alias text from the alias map */
  char* expandAlias = getAlias(cmd->argv[n]);
  if (strcmp(expandAlias, cmd->argv[n]) != 0 && aliasExpanded == FALSE)
    {
      /* get the command type of the alias text */
      commandT* cmdAlias = getCommand(expandAlias);
      cmd->argv[n] = cmdAlias->argv[0];
      int i;
      /* move all the original arguments to make room for new arguments */
      for (i = cmd->argc - 1; i > n; i--)
        {
          cmd->argv[i + cmdAlias->argc - 1] = cmd->argv[i];
        }
      /* put the new arguments */
      for (i = n + 1; i < n + cmdAlias->argc; i++)
        {
          cmd->argv[i] = cmdAlias->argv[i - n];
        }
      /* increase the argument count */
      cmd->argc += cmdAlias->argc - 1;
      /* expand the next alias if the current alias text ends with space or tab */
      if (*(strchr(expandAlias, '\0') - 1) == ' ' ||
          *(strchr(expandAlias, '\0') - 1) == '\t')
        {
          expandCmdWithAlias(cmd, cmdAlias->argc);
        }
      /* mark the alias as expanded to avoid recursive expansion */
      aliasExpanded = TRUE;
    }
}

/* getAlias
 *
 * arguments:
 *   char* cmdName: pointer to the command name string
 *
 * returns:
 *   char*: pointer to the text string of the alias
 *
 * This function checks if a command name has an alias, and if
 * it does, returns the expanded text of the alias.
 */
char*
getAlias(char* cmdName)
{
  if (aliasMap == NULL)
    {
      return cmdName;
    }
  /* find the alias or the smaller closest alias in the map*/
  Alias * prevAlias = findAlias(cmdName);
  /* return the alias text of the alias if found */
  if (strcmp(prevAlias->name, cmdName) == 0)
    {
      return prevAlias->text;
    }
  /* return the original text if alias not found */
  else
    {
      return cmdName;
    }
}

/*
 * findAlias
 *
 * arguments:
 *   char* name: the pointer to the name of the alias that
 *   needs to be found
 *
 * returns:
 *   Alias*: the pointer to an alias entry
 *
 * This function looks for the given alias in the alias map.
 * If the alias is found, it returns a pointer to the alias.
 * If not found, it returns a pointer to the alias in the map
 * whose name is smaller than and closest to the given alias.
 */
Alias*
findAlias(char* name)
{
  Alias * head = aliasMap;
  Alias * it;
  /* iterate through the map */
  for (it = head; it->next != NULL; it = it->next)
    {
      /* if alias name found, return the alias */
      if (strcmp(it->name, name) == 0)
        {
          return it;
        }
      /* if alias not found, return the smaller and closest alias */
      else if (strcmp(it->name, name) < 0)
        {
          if (strcmp((it->next)->name, name) > 0)
            {
              return it->next;
            }
        }
    }
  /* if there is only one entry in the map, return this entry */
  return it;
}

/*
 * createAlias
 *
 * arguments:
 *   char* aliasCmd: the assignment of alias, which includes a
 *   '=' character
 *
 * returns: none
 *
 * This function takes in the alias assignment, and creates a 
 * record of the alias.
 */  
void
createAlias(char* aliasCmd)
{
  /* if the context is not correct, return */
  if (strchr(aliasCmd, '=') == NULL)
    {
      return;
    }
  /* get the name and text of the alias */
  char * name = (char *) malloc(BUFSIZE);
  strcpy(name, aliasCmd);
  char * text = strchr(name, '=');
  *text = '\0';
  text++;
  Alias * newAlias = (Alias *) malloc(sizeof(Alias)); 
  /* create a new alias */
  newAlias->name = name;
  newAlias->text = text;
  /* put the alias into an empty map*/
  if (aliasMap == NULL)
    {
      newAlias->next = NULL;
      newAlias->prev = NULL;
      aliasMap = newAlias;
    }
  else /* put the alias into a non-empty map */
    {
      Alias * prevAlias = findAlias(newAlias->name);
      /* if an alias with the same name is found, replace the text */
      if (strcmp(prevAlias->name, newAlias->name) == 0)
        {
          prevAlias->text = newAlias->text;
        }
      else if (strcmp(prevAlias->name, newAlias->name) < 0)
        {
          /* if a smaller closest entry is found, put the new alias next to it */
          newAlias->next = prevAlias->next;
          newAlias->prev = prevAlias;
          prevAlias->next = newAlias;
        }
      else
        {
          /* if a larger closest entry is found, put the new alias ahead of it */
          if (prevAlias == aliasMap)
            {
              /* update the map pointer */
              aliasMap = newAlias;
            }
          newAlias->next = prevAlias;
          newAlias->prev = prevAlias->prev;
          prevAlias->prev = newAlias;
        }
    }
}

/*
 * removeAlias
 *
 * arguments:
 *   char* name: the name of the alias that needs to be removed
 *
 * returns: TRUE if found and removed, FALSE if not found
 *
 * This function finds the given alias name in the alias map,
 * and remove the entry if it is found.
 */
bool
removeAlias(char* name)
{
  if (aliasMap == NULL)
    {
      return FALSE;
    }
  else
    {
      Alias * prevAlias = findAlias(name);
      /* remove the entry only if the exact alias name is found */
      if (strcmp(prevAlias->name, name) == 0)
        {
          if (prevAlias->prev != NULL)
            {
              (prevAlias->prev)->next = prevAlias->next;
            }
          if (prevAlias->next != NULL)
            {
              (prevAlias->next)->prev = prevAlias->prev;
            }
          if (prevAlias == aliasMap)
            {
              aliasMap = prevAlias->next;
            }
          free(prevAlias);
          return TRUE;
        }
      else
        {
          return FALSE;
        }
    }
}

/*
 * printAlias
 *
 * arguments: none
 *
 * returns: none
 *
 * This function prints all the alias records.
 */
void
printAlias()
{
  Alias * it;
  /* iterate through the map and print every entry */
  for (it = aliasMap; it != NULL; it = it->next)
    {
      printf("alias %s=\'%s\'\n", it->name, it->text);
    }
    fflush(stdout);
}

/*
 * freeAliasMap
 *
 * arguments: none
 *
 * returns: none
 *
 * This function frees all the memory associated with the alias
 * map.
 */
void
freeAliasMap()
{
  Alias * it;
  Alias * head = aliasMap;
  /* iterate through the map and free every entry */
  for (it = aliasMap; it != NULL; it = head)
    {
      head = it->next;
      free(it);
    }
}

/*
 * freeCommand
 *
 * arguments:
 *   commandT *cmd: pointer to the commandT struct to be freed
 *
 * returns: none
 *
 * This function frees all the memory associated with the given
 * commandT struct, before freeing the struct's memory itself.
 */
void
freeCommand(commandT* cmd)
{
  int i;

  cmd->name = 0;
  for (i = 0; cmd->argv[i] != 0; i++)
    {
      free(cmd->argv[i]);
      cmd->argv[i] = 0;
    }
  free(cmd);
} /* freeCommand */

/*
 * updateEnv
 *
 * arguments:
 *   commandT *cmd: pointer to the commandT struct to be freed
 *
 * returns: none
 *
 * This function update environment variables in the arguements
 */
void
updateEnv(commandT* cmd)
{
  int i;

  for (i = 0; cmd->argv[i] != 0; i++)
    {
      if (cmd->argv[i][0] == '$')
	{
	  char * var = cmd->argv[i] + 1;
	  char * value = getenv(var);
	  int tmpLen = strlen(value);
	  cmd->argv[i] = realloc(cmd->argv[i], tmpLen + 1);
	  strcpy(cmd->argv[i], value);
	}
    }
} /* updateEnv */
