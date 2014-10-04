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
  int jobid; //job count
  struct bgjob_l* next;
  state_t state; //what the status of the job
  bool bg;
  char *cmdline; //the whole command line input 
} bgjobL;

/* the pids of the background processes */
bgjobL *headbgjob = NULL;
//bgjobL *currbgjob = NULL;

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
/* add a bg job to the list */
static bgjobL* addJobList(pid_t, char*, bool);
//list the jobs in the termial
static void showjobs();
/* wait for the fg job to finish */
static void waitforfg();
/* wait for the fg job to finish */
static void switchToFg(int jobid);
/* wait for the fg job to finish */
static void resumeBg(int jobid); 

/************External Declaration*****************************************/

/**************Implementation*********************************************/
int total_task;

void RunCmd(commandT** cmd, int n)
{
  int i;
  total_task = n;

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
    RunBuiltInCmd(cmd);
  }
  else
  {
    RunExternalCmd(cmd, fork);
  }
}

static void RunBuiltInCmd(commandT* cmd)
{ 
    if (strcmp(cmd->argv[0], "jobs") == 0) {
      showjobs();
    }
    else if (strcmp(cmd->argv[0], "cd") == 0) {
      if (cmd->argc > 1)
        chdir(cmd->argv[1]);
      else
        chdir(getenv("HOME"));
    }
    else if (strcmp(cmd->argv[0], "fg") == 0){
        switchToFg(atoi(cmd->argv[1]));
    }
    else if (strcmp(cmd->argv[0], "bg") == 0){
        resumeBg(atoi(cmd->argv[1]));
    }
}

/*Try to run an external command*/
static void RunExternalCmd(commandT* cmd, bool fork)
{
  //the resolve external command add the command path to the cmd->name attribute
  if (ResolveExternalCmd(cmd)){
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
      return TRUE;     
    }
  }
  return FALSE;     
}

static void Exec(commandT* cmd, bool forceFork)
{  
  //process divided into two, one is praent process with child_pid equal child's process id
  // one is child process with child_pid equals to 0
  int pid;
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);

  // block sigchld signals until recording the new process id,
  // and then fork the foreground process
  sigprocmask(SIG_BLOCK, &mask, NULL);
    pid = fork();

    /* This is run by the child. execute the command */
    if(pid == 0) {
      //to group the processs 
      setpgid(0,0);
      //Usage : int execv(const char *path, char *const argv[]); 
      execv(cmd->name, cmd->argv);
    }

    /* This is run by the parent. */
    else {
      //HandleJobs(child_pid);
      if(cmd->bg){
        addJobList(pid, cmd->cmdline, TRUE);
        sigprocmask(SIG_UNBLOCK, &mask, NULL);
      }
      else {
        fgpid = pid;
        //fg job does not add to the list
        addJobList(pid, cmd->cmdline, FALSE);
        sigprocmask(SIG_UNBLOCK, &mask, NULL);
        waitforfg(pid);
    }
  }
}

/* use a busy loop to wait fg */
static void waitforfg(pid_t id)
{
  while(fgpid ==id) sleep(1);
}

bgjobL * getLastJob(){
  bgjobL *job = headbgjob;
  while (job != NULL) {
    if (job->next == NULL){
      return job;
    }
    else
      job = job->next;
  }
  return job;
}

bgjobL * addJobList(pid_t pid, char *cmd, bool isbg){
  //printf("adding job\n");
  bgjobL *lastbgjob = getLastJob();
  bgjobL *newjob = (bgjobL *)malloc(sizeof(bgjobL));
  newjob->next = NULL;
  //int maxjobid = 0;
  // add the job to the beginning of the list
  // (bg jobs is in descending order of age)
  int count = 0;
  if (lastbgjob == NULL) {
    headbgjob = newjob;
  }
  else{
    count = lastbgjob->jobid;
    lastbgjob->next = newjob;
  }
  lastbgjob = newjob;
  // record everying in newjob and return it
  newjob->pid = pid;
  newjob->state = RUNNING; //assume the job is still running
  newjob->jobid = count + 1;
  newjob->cmdline = (char *)malloc(sizeof(char) * (MAXLINE));
  strcpy(newjob->cmdline, cmd);
  //int cmdlinelen = strlen(newjob->cmdline);
  // drop the " &" if it's a bg jobx
  if (isbg)
    newjob->bg = 1;
  else
    newjob->bg = 0;
  return newjob;
}

static void showjobs() {
  bgjobL *job = headbgjob;
  while (job != NULL) {

   if (!(!job->bg && job->state == DONE)) {
      state_t state = job->state;
      const char* msg =
      (state == DONE ? "Done" :
      (state == RUNNING ? "Running" :
      (state == STOPPED ? "Stopped" : "error")));
      if (strcmp(msg, "Done") != 0 && job->bg)
        printf("[%d]   %-24s%s&\n", job->jobid, msg, job->cmdline);
      else
        printf("[%d]   %-24s%s\n", job->jobid, msg, job->cmdline);
   }
   job = job->next;
  }
  fflush(stdout);
}

//update the job state in list
void UpdateBgJob(pid_t pid, state_t newstate){
  //printf("updating bg job....");
  bgjobL *current;
  current = headbgjob;
  while (current != NULL) {
    if (current->pid == pid) {
	      current->state = newstate;
    }
        current = current->next;
  }
}

void CheckJobs() {
  bgjobL *current;
  bgjobL *previous;
  current = headbgjob;
  previous = NULL;
  while (current != NULL) {
    if (current->state == DONE){
      //print out the finished job
      if(current->bg)
        printf("[%d]   %-24s%s\n", current->jobid, "Done", current->cmdline);
      //remove the job
      if (previous == NULL)
        headbgjob = current->next;
      else
        previous->next = current->next;
      current = current->next;
    }
    else{
      if (previous == NULL)
        previous = current;
      else
        previous = previous->next;
      current = current->next;
    }
  }
  fflush(stdout);

}

//fg (jobid)
//builtin command handler, switch a bg job to fg
static void switchToFg(int jobid) {
  bgjobL *job = headbgjob;
  while (job != NULL) {
    if (job->jobid == jobid) {
      // update state, send continue signal, and wait
      //TODO: shall we remove the job?
      fgpid = job->pid;
      job->bg = 0;
      kill(-fgpid, SIGCONT);
      waitforfg(job->pid);
    }
    job = job->next;
  }
}

//bg (jobid)
//builtin command handler, restart a stopped bg job
static void resumeBg(int jobid) {
  bgjobL *job = headbgjob;
  while (job != NULL) {
    if (job->state == STOPPED && job->jobid == jobid) {
      kill(job->pid, SIGCONT);
      job->state = RUNNING;
    }
    job = job->next;
  }
}

/************************************************/
//signal handler
//send SIGINT to fg job group
void IntFg()
{
  if (fgpid != -1) 
    kill(-fgpid, SIGINT);
    //printf("\n");
    //fflush(stdout);
    //FIXME do we need to remove it from list?
}

//signal handler
//send SIGTSTP to fg job group
void StopFg()
{
  if (fgpid == -1){
    return;
  }
  //printf("$455: fgpid is %d\n", fgpid);
  bgjobL *job = headbgjob;
  while (job != NULL) {
    if (job->pid == fgpid)
      break;
    job = job->next;
  }
  job->state = STOPPED;
  kill(-fgpid, SIGTSTP);
  fgpid = -1; 
  printf("[%d]   %-24s%s\n", job->jobid, "Stopped", job->cmdline);
  fflush(stdout);
}
/************************************************/


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

