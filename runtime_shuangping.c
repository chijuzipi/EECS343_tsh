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

char* BuiltInCommands[] = {"cd", "fg", "bg", "jobs", "alias", "unalias"};

typedef struct bgjob_l {
  pid_t pid;
  int jobid;
  int status;
  char* cmdline;
  struct bgjob_l* next;
} bgjobL;

typedef struct fgjob_l {
  pid_t pid;
  int jobid;
  char* cmdline;
} fgjobL;

typedef struct alias_l {
  char* lhs;
  char* rhs;
  struct alias_l* next;
} aliasL;

/* the pids of the background processes */
bgjobL *bgjobs = NULL;
fgjobL *fgjob = NULL;
aliasL *aliasList = NULL;

/************Function Prototypes*****************************************/
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
/* cd:
 *   change directory of current process
**/
static void ChangeDirectory(char*);

/* Background Jobs: */
static int     AddBgJob(pid_t, char*, int);
static bgjobL* QueryBgJob(pid_t);
static void    RemoveBgJob(pid_t);
static void    FreeBgJob(bgjobL*);
static void    ResumeBgJob(pid_t);

/* Foreground Jobs: */
static void UpdateFgJob(pid_t, char*);
static void FreeFgJob();
static void MoveToFg(int);

/* Show Jobs: */
static void ShowJobList();

/* Alias */
static void    AddAlias(char*);
static aliasL* CreateAlias(char*, char*, aliasL*);
static void    ShowAliasList();
static void    RemoveAlias(char*);
static void    FreeAlias(aliasL*);

/**************Implementation***********************************************/
int total_task;
void RunCmd(commandT** cmd, int n)
{
  int i;
  total_task = n;
  if (n == 1) {
    if (cmd[0]->is_redirect_in) {
      if (cmd[0]->is_redirect_out) {
        RunCmdRedirInOut(cmd[0], cmd[0]->redirect_in, cmd[0]->redirect_out);
      }
      else {
        RunCmdRedirIn(cmd[0], cmd[0]->redirect_in);
      }
    }
    else if (cmd[0]->is_redirect_out) {
      RunCmdRedirOut(cmd[0], cmd[0]->redirect_out);
    }
    else {
      RunCmdFork(cmd[0], TRUE);
    }
  }
  else{
    /*RunCmdPipe(cmd[0], cmd[1]);*/
    RunCmdPipe(cmd, n);
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
    RunBuiltInCmd(cmd);
  }
  else
  {
    RunExternalCmd(cmd, fork);
  }
}

void RunCmdBg(commandT* cmd)
{
  // TODO
}

void RunCmdPipe(commandT** cmd, int n)
{
    int i = 0;

    int pipefd[n - 1][2];

    for (i = 0; i < n - 1; i++) {
      if (pipe(pipefd[i]) < 0) {
        perror("Couldn't Pipe");
        exit(EXIT_FAILURE);
      }
    }

    int pid;
    int status;

    for (i = 0; i < n; ++i) {
        pid = fork();
        if (pid == 0) {
          if (i < n - 1){
            if (dup2(pipefd[i][1], 1) < 0){
              perror("dup2");
              exit(EXIT_FAILURE);
            }
          }

          if (i > 0 ){
            if (dup2(pipefd[i - 1][0], 0) < 0){
              perror("dup2");
              exit(EXIT_FAILURE);
            }
          }

          int q;
          for (q = 0; q < n - 1; q++){
            close(pipefd[q][0]);
            close(pipefd[q][1]);
          }

          RunCmdFork(cmd[i], FALSE);
        }
        else if(pid < 0){
          perror("error");
          exit(EXIT_FAILURE);
        }
    }

    for (i = 0; i < n - 1; i++){
      close(pipefd[i][0]);
      close(pipefd[i][1]);
    }

    for (i = 0; i < n; i++){
      wait(&status);
    }
}

void RunCmdRedirInOut(commandT* cmd, char* inFile, char* outFile)
{
  int fidOut = open(outFile, O_WRONLY | O_CREAT, 0644);
  int fidIn = open(inFile, O_RDONLY);

  int origStdout = dup(1);
  int origStdin = dup(0);

  dup2(fidOut, 1);
  dup2(fidIn, 0);

  RunCmdFork(cmd, TRUE);

  dup2(origStdout, 1);
  dup2(origStdin, 0);

  close(origStdout);
  close(origStdin);
}

void RunCmdRedirOut(commandT* cmd, char* file)
{
  int fid = open(file, O_WRONLY | O_CREAT, 0644);
  int origStdout = dup(1);
  dup2(fid, 1);

  RunCmdFork(cmd, TRUE);

  dup2(origStdout, 1);
  close(origStdout);
}

void RunCmdRedirIn(commandT* cmd, char* file)
{
  int fid = open(file, O_RDONLY);
  int origStdin = dup(0);
  dup2(fid, 0);

  RunCmdFork(cmd, TRUE);

  dup2(origStdin, 0);
  close(origStdin);
}


/*Try to run an external command*/
static void RunExternalCmd(commandT* cmd, bool fork)
{
  if (ResolveExternalCmd(cmd)){
    Exec(cmd, fork);
  }
  else {
    printf("%s: command not found\n", cmd->argv[0]);
    fflush(stdout);
    ReleaseCmdT(&cmd);
  }
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
    strcat(buf,cmd->argv[0]);
    if(stat(buf, &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(buf,X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(buf); 
          return TRUE;
        }
    }
  }
  return FALSE; /*The command is not found or the user don't have enough priority to run.*/
}

static void Exec(commandT* cmd, bool forceFork)
{
  int status;
  pid_t childPid = forceFork ? fork() : 0;

  if (childPid > 0) {
    if (cmd->bg == 0) {
      UpdateFgJob(childPid, cmd->cmdline);
      waitpid(childPid, &status, WUNTRACED);
      FreeFgJob();
    }
    else {
      AddBgJob(childPid, cmd->cmdline, 1);
    }
  }
  else if (childPid == 0) {
    if (forceFork)
      setpgid(0, 0);

    execv(cmd->name, cmd->argv);
    PrintPError("External command error!");
    exit(1);
  }
  else {
    PrintPError("Fork failed!");
  }

}

static bool IsBuiltIn(char* cmd)
{
  int i;
  for (i = 0; i < NBUILTINCOMMANDS; i++) {
    if (strcmp(cmd, BuiltInCommands[i]) == 0)
      return TRUE;
  }
  return FALSE;     
}


static void RunBuiltInCmd(commandT* cmd)
{
  if (strcmp("cd", cmd->argv[0]) == 0) {
    ChangeDirectory(cmd->argv[1]);
  }
  if (strcmp("fg", cmd->argv[0]) == 0) {
    if (cmd->argv[1])
      MoveToFg(atoi(cmd->argv[1]));
    else
      perror("fg");
  }
  if (strcmp("bg", cmd->argv[0]) == 0) {
    if (cmd->argv[1])
      ResumeBgJob(atoi(cmd->argv[1]));
    else
      perror("bg");
  }
  if (strcmp("jobs", cmd->argv[0]) == 0) {
    ShowJobList();
  }
  if (strcmp("alias", cmd->argv[0]) == 0) {
    if (cmd->argv[1])
      AddAlias(cmd->argv[1]);
    else
      ShowAliasList();
  }
  if (strcmp("unalias", cmd->argv[0]) == 0) {
    if (cmd->argv[1])
      RemoveAlias(cmd->argv[1]);
    else
      perror("unalias");

  }
}

void CheckJobs()
{
  pid_t childPid;
  bgjobL* bgjob;
  int status;

  while((childPid = waitpid(-1, &status, WNOHANG)) > 0) {
    bgjob = QueryBgJob(childPid);
    
    printf("[%d]   Done                    %s\n", bgjob->jobid, bgjob->cmdline);
    fflush(stdout);

    RemoveBgJob(childPid);
  }
}

void ShowJobList()
{
  char status[8];
  bgjobL* current = bgjobs;

  while (current) {
    switch (current->status) {
      case 0:
        sprintf(status, "Done   ");
        printf("[%d]   %s                 %s\n", current->jobid, status, current->cmdline);
        break;
      case 1:
        sprintf(status, "Running");
        if (current->cmdline[strlen(current->cmdline) - 1] == ' ')
          printf("[%d]   %s                 %s&\n", current->jobid, status, current->cmdline);
        else 
          printf("[%d]   %s                 %s &\n", current->jobid, status, current->cmdline);
        break;
      case 2:
        sprintf(status, "Stopped");
        printf("[%d]   %s                 %s\n", current->jobid, status, current->cmdline);
    }
    fflush(stdout);

    current = current->next;
  }
}

void ChangeDirectory(char* path)
{
  char* newPath = (path != NULL) ? path : getenv("HOME");
  int err = chdir(newPath);

  char* errMsg = malloc(sizeof(char*) * (strlen(newPath + 3)));
  if (err < 0) {
    sprintf(errMsg, "cd: %s", newPath);
    PrintPError(errMsg);
  }
  free(errMsg);
}

int AddBgJob(pid_t pid, char* cmdline, int status) {
  bgjobL* newJob;
  bgjobL* current = bgjobs;

  newJob = (bgjobL*)malloc(sizeof(bgjobL));
  newJob->pid = pid;
  newJob->status = status;
  newJob->cmdline = (char*)malloc(sizeof(char*) * MAXLINE);
  newJob->cmdline = strdup(cmdline);
  newJob->next = NULL;
  
  if (current == NULL) {
    newJob->jobid = 1;
    bgjobs = newJob;
  }
  else {
    while (current->next) {
      current = current->next;
    }
    newJob->jobid = current->jobid + 1;
    current->next = newJob;
  }
  
  return newJob->jobid;
}

bgjobL* QueryBgJob(pid_t pid) {
  bgjobL* current = bgjobs;

  while (current->pid != pid)
    current = current->next;
  
  return current;
}

void RemoveBgJob(pid_t pid) {
  bgjobL* previous = NULL;
  bgjobL* current = bgjobs;

  while (current->pid != pid) {
    previous = current;
    current = current->next;
  }

  if (previous)
    previous->next = current->next;
  else
    bgjobs = current->next;

  FreeBgJob(current);
}


void FreeBgJob(bgjobL* bgJob) {
  if(bgJob->cmdline != NULL) free(bgJob->cmdline);
  free(bgJob);
}

void ResumeBgJob(int jobid) {
  bgjobL* current = bgjobs;

  while (current->jobid != jobid) {
    current = current->next;
  }

  if (current->jobid == jobid) {
    current->status = 1;
    kill(-current->pid, SIGCONT);
  }
  else {
    PrintPError("bg: The job does not exist.");
  }
}

void UpdateFgJob(pid_t pid, char* cmdline) {
  if (fgjob == NULL) {
    fgjob = (fgjobL*)malloc(sizeof(fgjobL));
    fgjob->cmdline = (char*)malloc(sizeof(char*) * MAXLINE);
  }
  fgjob->pid = pid;
  fgjob->cmdline = strdup(cmdline);
}

void FreeFgJob() {
  if (fgjob) {
    if (fgjob->cmdline != NULL) free(fgjob->cmdline);
    free(fgjob);
    fgjob = NULL;
  }
}

void StopFgProc() {
  if (fgjob) {
    kill(-fgjob->pid, SIGINT);
  }
}

void SuspendFgProc() {
  int jobid;
  
  if (fgjob) {
    kill(-fgjob->pid, SIGTSTP);
    jobid = AddBgJob(fgjob->pid, fgjob->cmdline, 2);
    printf("\n[%d]   Stopped                 %s\n", jobid, fgjob->cmdline);
    fflush(stdout);
    FreeFgJob();
  }
}

void MoveToFg(int jobid) {
  int status;
  int pid;
  bgjobL* current = bgjobs;

  while (current->jobid != jobid) {
    current = current->next;
  }

  if (current->jobid == jobid) {	
    pid = current->pid;

    if (current->status == 2)
      kill(-pid, SIGCONT);

    UpdateFgJob(pid, current->cmdline);
    RemoveBgJob(pid);
    waitpid(pid, &status, WUNTRACED);
    FreeFgJob();
  }
}

void AddAlias(char* aliasLine) {
  aliasL* previous = NULL;
  aliasL* current = aliasList;
  
  char* argv = strtok(aliasLine, "=");
  char* left = strdup(argv);

  argv = strtok(NULL, "=");
  if (argv) {
    if (current == NULL) {
      aliasList = CreateAlias(left, strdup(argv) + 1, NULL);
    }
    else {
      while (current) {
        if (strcmp(left, current->lhs) < 0) {
          if (previous)
            previous->next = CreateAlias(left, strdup(argv) + 1, current);
          else
            aliasList = CreateAlias(left, strdup(argv) + 1, current);
          break;
        }
        else if (current->next == NULL && strcmp(left, current->lhs) > 0) {
          current->next = CreateAlias(left, strdup(argv) + 1, NULL);
          break;
        }
        else if (strcmp(left, current->lhs) == 0) {
          current->rhs = strdup(argv) + 1;
          break;
        }
        previous = current;
        current = current->next;
      }
    }
  }
  else
    PrintPError("alias: Cannot alias the command");
}

aliasL* CreateAlias(char* lhs, char* rhs, aliasL* next) {
  aliasL* newAlias = malloc(sizeof(aliasL));
  newAlias->lhs = lhs;
  newAlias->rhs = rhs;
  newAlias->next = next;
  return newAlias;
}

void ShowAliasList() {
  aliasL* current = aliasList;

  while (current) {
    printf("alias %s=\'%s\'\n", current->lhs, current->rhs);
    fflush(stdout);

    current = current->next;
  }
}

void RemoveAlias(char* lhs) {
  aliasL* previous = NULL;
  aliasL* current = aliasList;

  if (current) {
    while(current->next && (strcmp(lhs, current->lhs) != 0)) {
      previous = current;
      current = current->next;
    }

    if (strcmp(lhs, current->lhs) == 0) {
      if (previous) {
        previous->next = current->next;
      }
      else {
        aliasList = current->next;
      }
      FreeAlias(current);
    }
    else {
      PrintPError("unalias: Cannot find the alias");
    }
  }
  else {
    PrintPError("unalias: Cannot find the alias");
  }
}

char* QueryAliasList(char* lhs) {
  aliasL* current = aliasList;

  while (current) {
    if (strcmp(lhs, current->lhs) == 0) {
      return current->rhs;
    }
    current = current->next;
  }

  return NULL;
}

void FreeAlias(aliasL* alias) {
  free(alias);
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
