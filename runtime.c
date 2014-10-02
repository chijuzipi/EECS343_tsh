/***************************************************************************
 *  Title: Runtime environment 
 * -------------------------------------------------------------------------
 *    Purpose: Runs commands
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.1 $
 *    Last Modification: $Date: 2005/10/13 05:24:59 $
 *    File: $RCSfile: runtime.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: runtime.c,v $
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

//a linked list struct
typedef struct bgjob_l {
  pid_t pid;
  struct bgjob_l* next;
  int count;
} bgjobL;

/* the pids of the background processes */
bgjobL *bgjobs = NULL;
bgjobL *conductor = NULL;

/************initialized builtin commands*********************************/
//const char *builtins[] = {":", ".", "break", "cd", "continue", "eval", "exec", "exit", "export", "getopts", "hash",
//"pwd", "readonly", "return", "shift", "test", "times", "trap", "umask", "unset"};

const char *builtins[] = {"jobs", "bg", "fg", "cd"};
const int builtinNumber = 4; 

/************Function Prototypes******************************************/
/* run command */
static void RunCmdFork(commandT*, bool);
/* runs an external program command after some checks */
static void RunExternalCmd(commandT*, bool);
/* resolves the path and checks for exutable flag */
static bool ResolveExternalCmd(commandT*);
/* forks and runs a external program */
static void Exec(commandT*, bool);
/* runs a builtin command */
static void RunBuiltInCmd(commandT*);
/* checks whether a command is a builtin command */
static bool IsBuiltIn(char*);
/************External Declaration*****************************************/

/**************Implementation*********************************************/
int total_task;

void RunCmd(commandT** cmd, int n)
{
  int i;
  total_task = n;
  //if n==1, means no pipe, definitely fork
  if(n == 1)
    RunCmdFork(cmd[0], TRUE);
  else{
    RunCmdPipe(cmd[0], cmd[1]);
    for(i = 0; i < n; i++)
      ReleaseCmdT(&cmd[i]);
  }
}

void RunCmdFork(commandT* cmd, bool fork)
{

  if (cmd->argc<=0)
    return;
  if (IsBuiltIn(cmd->argv[0]))
  { 
    //printf("the cmd name is %s", cmd->argv[0]);
    RunBuiltInCmd(cmd);
  }
  else
  {
    //printf("run external command\n");
    RunExternalCmd(cmd, fork);
  }
}

static void RunBuiltInCmd(commandT* cmd)
{ 
    if (strcmp(cmd->argv[0], "jobs") == 0) {
      printf("jobs");
    /*
        bgjobL* prev_job = NULL;
        bgjobL* top_job = oldest_bgjob;
        while(top_job != NULL)
        {
            prev_job = top_job;
            top_job = top_job->prev;
            print_job(prev_job, job_status(prev_job));
        }
        */
    }
    else if (strcmp(cmd->argv[0], "cd") == 0) {
      if (cmd->argc > 1)
        chdir(cmd->argv[1]);
      else
        chdir(getenv("HOME"));
    }
    else if (strcmp(cmd->name, "fg") == 0){
      printf("fg command");
        //fg(cmd);
    }
    else if (strcmp(cmd->name, "bg") == 0){
      printf("bg command");
        //bg(cmd);
    }
}

/*Try to run an external command*/
static void RunExternalCmd(commandT* cmd, bool fork)
{
  //the resolve external command add the command path to the cmd->name attribute
  if (ResolveExternalCmd(cmd)){
    //printf("command resolved\n");
    Exec(cmd, fork);
  }
  else {
    printf("%s: command not found\n", cmd->argv[0]);
    fflush(stdout);
    ReleaseCmdT(&cmd);
  }
}

void RunCmdBg(commandT* cmd)
{
  //add the bg process to list
  /*
  int currentCount;
  if (bgjobs == NULL){
    bgjobs = malloc(sizeof(bgjobL));
    bgjobs->next = NULL;
    currentCount = 1;
    bgjobs->count = currentCount;
    conductor = bgjobs;
  }
  else {
    conductor->next = malloc(sizeof(bgjobL));
    currentCount = conductor->count;
    conductor = conductor->next; 
    conductor->count = ++currentCount;
  }

  //int child_status;
  pid_t child_pid = fork();
  if(child_pid == 0) {
    execve(cmd->name, cmd->argv, NULL);
    printf("execv error, terminated\n");
    exit(0);
  }
  else {
    conductor->pid = child_pid;
    conductor->next = NULL;
    handleJobs(child_pid);
    //printf("[%d] ",currentCount);
    //printf("%d\n",child_pid);
  }
  */
}

void RunCmdPipe(commandT* cmd1, commandT* cmd2)
{
}

void RunCmdRedirOut(commandT* cmd, char* file)
{
}

void RunCmdRedirIn(commandT* cmd, char* file)
{
}

/*Find the executable based on search list provided by environment variable PATH*/
static bool ResolveExternalCmd(commandT* cmd)
{
  char *pathlist, *c;
  char buf[1024];
  int i, j;
  struct stat fs;

  if(strchr(cmd->argv[0],'/') != NULL){
    if(stat(cmd->argv[0], &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(cmd->argv[0],X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(cmd->argv[0]);
          return TRUE;
        }
    }
    return FALSE;
  }
  pathlist = getenv("PATH");
  //printf("pathlist is : %s \n", pathlist);
  if(pathlist == NULL) return FALSE;
  i = 0;
  while(i<strlen(pathlist)){
    c = strchr(&(pathlist[i]),':');
    if(c != NULL){
      for(j = 0; c != &(pathlist[i]); i++, j++)
        buf[j] = pathlist[i];
      i++;
    }
    else{
      for(j = 0; i < strlen(pathlist); i++, j++)
        buf[j] = pathlist[i];
    }
    buf[j] = '\0';
    strcat(buf, "/");
    //strcat is to add latter string to the previous
    strcat(buf,cmd->argv[0]);
    //stat is a system call that is used to determine information about a file based on its file path.
    if(stat(buf, &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(buf,X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(buf); 
          //printf("the cmd's name is now : %s \n", cmd->name);
          return TRUE;
        }
    }
  }
  return FALSE; /*The command is not found or the user don't have enough priority to run.*/
}

static bool IsBuiltIn(char* cmd)
{
  int i;
  for (i = 0; i < builtinNumber; i ++){
    if (strcmp(cmd, builtins[i]) == 0){
      //printf("this is a builtin command\n");
      return TRUE;     
    }
  }
  return FALSE;     
}

static void Exec(commandT* cmd, bool forceFork)
{  
  //process divided into two, one is praent process with child_pid equal child's process id
  // one is child process with child_pid equals to 0
  int status;
  if(forceFork){
    //printf("external command executing...\n");
    pid_t child_pid = fork();

    /* This is run by the child. execute the command */
    if(child_pid == 0) {
      /* This is done by the child process. */
      //Usage : int execv(const char *path, char *const argv[]); 
      execve(cmd->name, cmd->argv, NULL);
      /* If execv returns, it must have failed. */
      printf("execv error, terminated\n");
      exit(2);
    }

    /* This is run by the parent. */
    else {
      //HandleJobs(child_pid);
      if(cmd->bg){
        printf("$$$$:");
      }
      else 
        waitpid(child_pid, &status, 0);
        //printf("\n");
    }
  }
  else{
    /*
    execv(cmd->name, cmd->argv);
    printf("execv error, terminated\n");
    exit(0);
    */
  }
}

//void HandleJobs(pid_t pid){
  /* 
  1) create list node
  2) check existing jobs status
  3) printng updates
  4) update linked list
  5) print prompt
  */
//}
  
void CheckJobs()
{
}


commandT* CreateCmdT(int n)
{
  int i;
  commandT * cd = malloc(sizeof(commandT) + sizeof(char *) * (n + 1));
  cd -> name = NULL;
  cd -> cmdline = NULL;
  cd -> is_redirect_in = cd -> is_redirect_out = 0;
  cd -> redirect_in = cd -> redirect_out = NULL;
  cd -> argc = n;
  for(i = 0; i <=n; i++)
    cd -> argv[i] = NULL;
  return cd;
}

/*Release and collect the space of a commandT struct*/
void ReleaseCmdT(commandT **cmd){
  int i;
  if((*cmd)->name != NULL) free((*cmd)->name);
  if((*cmd)->cmdline != NULL) free((*cmd)->cmdline);
  if((*cmd)->redirect_in != NULL) free((*cmd)->redirect_in);
  if((*cmd)->redirect_out != NULL) free((*cmd)->redirect_out);
  for(i = 0; i < (*cmd)->argc; i++)
    if((*cmd)->argv[i] != NULL) free((*cmd)->argv[i]);
  free(*cmd);
}
