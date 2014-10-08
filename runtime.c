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
#include "interpreter.h"
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
aliasT *aliases;

/************initialized builtin commands*********************************/
//const char *builtins[] = {":", ".", "break", "cd", "continue", "eval", "exec", "exit", "export", "getopts", "hash",
//"pwd", "readonly", "return", "shift", "test", "times", "trap", "umask", "unset"};

const char *builtins[] = {"jobs", "bg", "fg", "cd","alias","unalias"};
const int builtinNumber = 6; 

/************Function Prototypes******************************************/
/* run command */
static void RunCmdFork(commandT*, bool);
/* runs an external program command after some checks */
static void RunExternalCmd(commandT*, bool);
/* run command with both in and out redirection */
static void RunCmdRedirInOut(commandT*, char*, char*);
/* resolves the path and checks for exutable flag */
static bool ResolveExternalCmd(commandT*);
/* forks and runs a external program */
static void Exec(commandT*, bool);
/* runs a builtin command */
static void RunBuiltInCmd(commandT*);
/* checks whether a command is a builtin command */
static bool IsBuiltIn(char*);
/* add a bg job to the list */
static void addJobList(pid_t, char*, bool);
//list the jobs in the termial
static void ShowJobs();
/* wait for the fg job to finish */
static void waitfg();
/* wait for the fg job to finish */
static void switchToFg(int jobid);
/* wait for the fg job to finish */
static void resumeBg(int jobid); 

void printAliases(void);

void makeAlias(commandT*);

void push_alias(aliasT*);

void removeAlias(char*);

char* aliasOf(char*);

commandT* parseAliases(commandT*);

void freeCommand(commandT* cmd);
/************External Declaration*****************************************/

/**************Implementation*********************************************/
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
  else {
    RunCmdPipe(cmd[0], cmd[1]);
    for(i = 0; i < n; i++)
      ReleaseCmdT(&cmd[i]);
  }
}

void RunCmdFork(commandT* cmd, bool fork)
{
  //printf("The command is %s\n", cmd -> cmdline);
  //judge alias or not
  printf("cmd is %s\n", cmd->argv[1]);
  commandT* newCmd;
  if(strcmp(cmd -> cmdline, "unalias") == 0) {
    newCmd = cmd;
  }
  else {
    newCmd = parseAliases(cmd);

  }
  if (newCmd -> argc <= 0)
    return;
  if (IsBuiltIn(newCmd -> argv[0]))
  { 
    RunBuiltInCmd(newCmd);
  }
  else
  {
    RunExternalCmd(newCmd, fork);
  }
}

commandT* parseAliases(commandT* cmd) {
    //allocate 100 char mem;
    commandT* newCmd = malloc(sizeof(commandT) + sizeof(char *)* 100);
    int i = 0;
    int newArgCount = 0;
    bool inputQuoted = FALSE;

    for(i = 0; i < cmd -> argc; i++)
    {
        char* currentArg = cmd->argv[i];
        if(strchr(currentArg, ' '))
          inputQuoted = TRUE;
        //transfer name string into command and argv string
        char* new = aliasOf(currentArg);
        char* tmp = malloc(sizeof(char) * MAXLINE);
        int tmpLength = 0;
        int j;
        //divide the alias command and args string into command and args
        for(j = 0; j <= strlen(new); j++)
        {
            //printf("in the new args for loop%d\n", j);
            char currentChar = new[j];
            //divide the alias command and args string into command and args
            if(((currentChar == ' ' || currentChar == 0) && !inputQuoted) || (inputQuoted && currentChar == 0))
            {
                // an arg ends, we empty the tmp container
                char* newArg = malloc(sizeof(char) * (tmpLength + 1));

                tmp[tmpLength] = 0;    
                strcpy(newArg, tmp);
                newCmd->argv[newArgCount] = strdup(newArg);

                newArgCount++;

                tmp = realloc(tmp, sizeof(char) * MAXLINE);
                tmpLength = 0;    

                free(newArg);
            }         
            else
            {
              tmp[tmpLength] = currentChar;
              tmpLength++;
            }
        }
        inputQuoted = FALSE;
        free(tmp);
        //free(currentArg); freeCommand will handle this after this func returns
    }
    newCmd->name = strdup(newCmd->argv[0]);
    newCmd->cmdline = strdup(cmd->cmdline);
    newCmd->bg = cmd->bg;
    newCmd->argc = newArgCount;
    freeCommand(cmd);

    return newCmd;
    
}
void freeCommand(commandT* cmd)
{
  int i;

  cmd->name = 0;
  for (i = 0; cmd->argv[i] != 0; i++)
    {
      free(cmd->argv[i]);
      cmd->argv[i] = 0;
    }
  free(cmd);
}

char* aliasOf(char* nameStr) {
  char* result = malloc(sizeof(char) * MAXLINE);
    strcpy(result, nameStr);
    aliasT* headAlias = aliases;
    //travsel the alias list to find the alias command and args string
    while(headAlias)
    {
        if(strcmp(nameStr, headAlias->lhs) == 0)
        {
            strcpy(result, headAlias->rhs);
            break;
        }
        headAlias = headAlias->next;
    }
    return result;
}



static void RunBuiltInCmd(commandT* cmd)
{ 
    if (strcmp(cmd->argv[0], "jobs") == 0) {
      ShowJobs();
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
    else if (strcmp(cmd->argv[0], "alias") == 0)
    {
        if(cmd->argc == 1)
            printAliases();
        else
            makeAlias(cmd);
    }
    else if (strcmp(cmd->name, "unalias") == 0)
    {
      if(cmd -> argc == 2)
        removeAlias(cmd -> argv[1]);
      else
        printf("unalias requires exactly one arg.\n");
    }
}

void printAliases(void)
{
    aliasT *head = aliases;
    while(head)
    {
        printf("alias %s='%s'\n", head->lhs, head->rhs);
        head = head -> next;
    }
}

void makeAlias(commandT* cmd)
{
    char *rhs = malloc(sizeof(char) * MAXLINE);
    char *lhs = malloc(sizeof(char) * MAXLINE);
    int lhs_size = 0;
    printf("argv[1] = %s\n", cmd->argv[1]);
    char *rhs = malloc(sizeof(char) * MAXLINE);
    int rhs_size = 0;

    bool left = TRUE;
    int i;

    for(i = 0; i <= strlen(cmd -> argv[1]); i++)
    {
        char cur_char = cmd -> argv[1][i];
        if(left && cur_char != '=')
        {
            lhs[lhs_size] = cur_char;
            lhs_size++;
        }
        else if(left && cur_char == '=')
        {
            lhs[lhs_size] = 0;
            left = FALSE;
        }
        else if (!left && cur_char != '\'')
        {
            rhs[rhs_size] = cur_char;
            rhs_size++;
        }
        else if (!left && cur_char == '\'') {
            rhs[rhs_size] = 0;
        }
    }
    aliasT *alias = malloc(sizeof(alias));
    alias -> lhs = strdup(lhs);
    printf("alias left = %s\n", alias -> lhs);
    alias -> rhs = strdup(rhs);
    push_alias(alias);

    free(lhs);
    free(rhs);
}

void push_alias(aliasT* alias)
{
    printf("push alias now,left = %s\n", alias->lhs);
    aliasT *prev = NULL;
    aliasT *top = aliases;
    //if the alias string is already existed in list, overwrite it
    while( top && strcmp( alias -> lhs, top -> lhs) > 0)
    {
      prev = top;
      top = top -> next;
    }
    if(prev)
      prev -> next = alias; 
    else{
      aliases = alias;
      printf("else now! left = %s\n", aliases->lhs);
    }
      //aliases = alias;
    alias -> next = top;
    printf("finish pushing! left = %s\n", aliases->lhs);
    printf("finish pushing! right = %s\n", aliases->rhs);
}

void removeAlias(char *name)
{
    aliasT *prev = NULL;
    aliasT *top = aliases;
    while(top)
    {
        if(strcmp(top->lhs, name) == 0)
        {
            if(prev)
                prev->next = top->next;
            else
                aliases = top->next;
            top->next = NULL;
            free(top->lhs);
            free(top->rhs);
            free(top);
            return;
        }
        prev = top;
        top = top->next;
    }
  printf("/bin/bash: line 3: unalias: %s: not found\n", name);
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
  while(i < strlen(pathlist)){
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
  if (pid == 0) {
    //group the processs 
    setpgid(0, 0);
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
      //FIXME the fg job also add to the list, with the "isbg" parameter set to FALSE
      addJobList(pid, cmd->cmdline, FALSE);

      sigprocmask(SIG_UNBLOCK, &mask, NULL);

      waitfg(pid);
    }
  }
}

/* use a busy loop to wait fg */
static void waitfg(pid_t id)
{
  //every 1 sec check the status of fg job
  while(fgpid == id) sleep(1);
}

/*get the last job of the linked list*/
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

//add the new job to the end of the list
static void addJobList(pid_t pid, char *cmd, bool isbg){
  bgjobL *lastbgjob = getLastJob();
  bgjobL *newjob = (bgjobL *)malloc(sizeof(bgjobL));
  newjob->next = NULL;
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

  if (isbg)
    newjob->bg = 1;
  else
    newjob->bg = 0;
}

static void ShowJobs() {
  bgjobL *job = headbgjob;
  while (job != NULL) {

   if (!(!job->bg && job->state == DONE)) {
      state_t state = job->state;
      const char* msg =
      (state == DONE ? "Done" :
      (state == RUNNING ? "Running" :
      (state == STOPPED ? "Stopped" : "error")));
      if (strcmp(msg, "Done") != 0 && job->bg)
        printf("[%d]   %-24s%s &\n", job->jobid, msg, job->cmdline);
      else
        printf("[%d]   %-24s%s\n", job->jobid, msg, job->cmdline);
   }
   job = job->next;
  }
  fflush(stdout);
}

//update the job state in list
void UpdateBgJob(pid_t pid, state_t newstate){
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
//builin command handler, switch a bg job to fg, start it
static void switchToFg(int jobid) {
  bgjobL *job = headbgjob;
  while (job != NULL) {
    if (job->jobid == jobid) {
      // update state, send continue signal, and wait
      fgpid = job->pid;
      job->bg = 0;
      kill(-fgpid, SIGCONT);
      waitfg(job->pid);
    }
    job = job->next;
  }
}

//bg (jobid)
//builtin command handler, start a bg job
static void resumeBg(int jobid) {
  bgjobL *job = headbgjob;
  while (job != NULL) {
    if (job->state == STOPPED && job->jobid == jobid) {
      kill(job->pid, SIGCONT);
      //FIXME if this is a fg job, make it bg
      job->bg = 1;
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
    //FIXME do we need to remove it from list?
}

//signal handler
//send SIGTSTP (suspend a fg job) to fg job group
void StopFg()
{
  if (fgpid == -1){
    return;
  }
  bgjobL *job = headbgjob;
  while (job != NULL) {
    if (job->pid == fgpid)
      break;
    job = job->next;
  }
  job->state = STOPPED;
  kill(-fgpid, SIGTSTP);
  // printf("kill %d \n", kill(-fgpid, SIGTSTP));
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

