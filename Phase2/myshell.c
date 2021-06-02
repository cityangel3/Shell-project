/* $begin shellmain */
#include "myshell.h"
#include<errno.h>
#define MAXARGS   128
#define MAXJOBS 1024 /* MAX # of jobs*/
typedef struct job{
    pid_t pid;
    int number;
    char target;
    char fbs,state[30],cmd[100];
}Jobs;
Jobs joblist[MAXJOBS];
int argc,wnum;            /* Number of args, Number of jobs */
/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);
void execute(const char *filename, char * argv[], char *const envp[]);
void command_ls(char *const argv[]);
void command_cat(char *const argv[]);
void command_echo(char * argv[]);
void command_cd(char *const argv[]);
void pipe_cmd(char *const argv[],int *const pindex,int pcount);

void addjob(Jobs* job,int wnum,pid_t id,char state,char* const command);
int deletejob(Jobs* job,int wnum,pid_t id);
void listjob(Jobs* job,int fd);
Jobs* getjobpid(Jobs* job,pid_t id);
int main()
{
    char *pt,cmdline[MAXLINE]; /* Command line */

    while (1) {
    /* Read */
        printf("CSE20171700-SP-P#4> ");
        pt = fgets(cmdline, MAXLINE, stdin);
        if (feof(stdin))
            exit(0);
        /* Evaluate */
        eval(cmdline);
    }
    return 0;
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline)
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char temp[MAXLINE],pt[MAXLINE],buf[MAXLINE];   /* Holds modified command line */
    int pindex[MAXARGS] = {0,}; /*pipe command index*/
    int bg,status,pcnt=0,pflag=0,qflag,i;              /* Should the job run in bg or fg? */
    pid_t wpid,pid;           /* Process id */
    sigset_t mask, prev;
    sigemptyset(&mask);
    
    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if (argv[0] == NULL)
        return;   /* Ignore empty lines */
    /* parsing about quotes */
    for(i=0;argv[i] != NULL;i++){
        qflag  = 0;
        strcpy(pt,argv[i]);
        if((pt[0] == '\''&&pt[strlen(pt)-1] == '\'') || (pt[0] == '\"'&&pt[strlen(pt)-1] == '\"')){
            (argv[i])[strlen(argv[i])-1] = '\0';
            strcpy(temp,argv[i]+1); qflag = 1;
        }
        else if(pt[strlen(pt)-1] == '\"' || pt[strlen(pt)-1] == '\''){
            (argv[i])[strlen(argv[i])-1] = '\0';
            strcpy(temp,argv[i]); qflag = 1;
        }
        else if(pt[0] == '\"' || pt[0] == '\''){
            strcpy(temp,argv[i]+1); qflag = 1;
        }
        if(qflag)
            strcpy(argv[i],temp);
        memset(pt,'\0',strlen(pt));memset(temp,'\0',strlen(pt));
    }
    /*whether pipe*/
    for(i=0;argv[i] != NULL;i++){
        if(!(strcmp(argv[i],"|"))){
            pindex[++pcnt] = i+1; pflag = 1;
            argv[i] = NULL;
        }
    }
    
    if((pid = Fork()) < 0) { //do fork
        perror(argv[0]); exit(0);
    }
    if(pid == 0){ /*child process*/
        if(bg){//if process in background, ignore SIGINT
            signal(SIGINT,SIG_IGN);
            printf("[%d] %d\n",wnum++,getpid());
        }
        else
            signal(SIGINT,SIG_DFL);
        
        if(pflag){ // pipe exist
            pipe_cmd(argv,pindex,pcnt);
        }
        else if(!(builtin_command(argv))) //normal command
            execute(argv[0], argv, environ);
        exit(0);
    }
    /* Parent process waits for foreground job to terminate */
    if(!bg){
        addjob(joblist,wnum,pid,'f',cmdline);
        wpid = Waitpid(pid,&status,0);
        if((builtin_command(argv)) == 1) exit(0);
        if((builtin_command(argv)) == 2) command_cd(argv);
    }
    // Parent doesn't wait for background job
    addjob(joblist,wnum,pid,'b',cmdline);
    return;
}
int builtin_command(char **argv)
{
    if (!strcmp(argv[0], "exit")) /* quit command */
        return 1;
    if (!strcmp(argv[0], "cd"))
        return 2;
    if (!strcmp(argv[0], "&"))   /* Ignore singleton & */
    return -1;
    return 0;                    /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv)
{
    char *delim;         /* Points to first space delimiter */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
    buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
    argv[argc++] = buf;
    *delim = '\0';
    buf = delim + 1;
    while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
    return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
    argv[--argc] = NULL;

    return bg;
}
/* $end parseline */
/*-------------------------------------------------*/
static void sigchild_handler(int sig){ //다시 짜야됨 signal들
    int status;
    pid_t chpid;
    while((chpid = waitpid(-1,&status,WNOHANG|WUNTRACED))>0){ //terminated normally
        if((WIFEXITED(status))>0){
            if(deletejob(joblist,-1,chpid) < 0)
                printf("Delete job error\n");
        }
        else if(WIFSIGNALED(status) != 0){ //terminated by signal
            if(WTERMSIG(status) == 15)
                printf("%d terminated by 15\n",chpid);
            else
                printf("%d terminated by 2\n",chpid);
            if(deletejob(joblist,-1,chpid) < 0)
                printf("Delete job error\n");
        }
        else if(WIFSTOPPED(status) == 1){
            printf("%d stopped",chpid);
        }
    }
}
static void sigint_handler(int sig){
    int i;
    for(i=0;i<MAXJOBS;i++){
        if(joblist[i].fbs == 'f')
            kill(joblist[i].pid,SIGKILL);
    }
    return;
}
static void sigtstp_handler(int sig){
    int i;
    for(i=0;i<MAXJOBS;i++){
        if(joblist[i].fbs == 's')
            kill(joblist[i].pid,SIGSTOP);
    }
    return;
}
static void sigcont_handler(int sig){
    int i;
    for(i=0;i<MAXJOBS;i++){
        if(joblist[i].fbs == 's'){
            kill(joblist[i].pid,SIGCONT);
            printf("process back to continue\n");
        }
    }
    return;
}
static void sigquit_handler(int sig){
    exit(0);
}
/*-------------------------------------------------*/
Jobs* getjobpid(Jobs* job,pid_t id){
    Jobs* temp = NULL;
    int i;
    for(i=0;i<=wnum;i++){
        if(job[wnum].pid == id)
            return &(job[wnum]);
    }
    return temp;
}
void addjob(Jobs* job,int wnum,pid_t id,char state,char* const command){
    int i;
    job[wnum].fbs = state; job[wnum].number = wnum; job[wnum].pid = id;
    job[wnum].target = '+'; strcpy(job[wnum].cmd,command);
    strcpy(job[wnum].state,"running");
    for(i=0;i<wnum-1;i++)
        job[i].target = ' ';
    job[i].target = '-';
    return;
}
int deletejob(Jobs* job,int wnum,pid_t id){
    strcpy(job[wnum].state,"done");
    job[wnum].pid = -1;
    return 1;
}
/*-------------------------------------------------*/
void unix_error(char *msg) /* Unix-style error */
{
    printf("%s: %s\n", msg, strerror(errno));
    exit(0);
}
pid_t Waitpid(pid_t pid, int *iptr, int options)
{
    pid_t retpid;
    if ((retpid  = waitpid(pid, iptr, options)) < 0)
    unix_error("Waitpid error");
    if(!WIFEXITED(*iptr) || WIFSIGNALED(*iptr)||WIFSTOPPED(*iptr)){ // child terminate abnormally
        printf("Child %d terminate abnormally\n",retpid);
    }
    return(retpid);
}
pid_t Fork(void){
    pid_t pid;
    if ((pid = fork()) < 0)
    unix_error("Fork error");
    return pid;
}
void pipe_cmd(char *const argv[],int *const pindex,int pcount){
    pid_t pid,wpid;
    int i,status,fd[MAXARGS][2] = {0,};
    
    if((pipe(fd[0])) < 0) // make pipe
    	perror(argv[0]);
    if((pid = Fork()) < 0) { perror(argv[0]); exit(0); }
    if(pid == 0){ //case of child process
        close(STDOUT_FILENO); 
        dup2(fd[0][1],STDOUT_FILENO);
        close(fd[0][1]);
        if(execvp(argv[pindex[0]],&argv[pindex[0]]) <0)
            perror(argv[pindex[0]]);
    }
    else{ //case of parent process
        close(fd[0][1]);
        wpid = Waitpid(pid,&status,0);
    }
    for(i=0;i<pcount-1;i++){
        if(pipe(fd[i+1]) < 0) perror(argv[0]);
        if((pid = Fork()) < 0) { perror(argv[0]); exit(0); }
        if(pid == 0){
            close(STDIN_FILENO); 
            close(STDOUT_FILENO); 
            dup2(fd[i][0],STDIN_FILENO); 
            dup2(fd[i+1][1],STDOUT_FILENO); 
            close(fd[i][0]); close(fd[i+1][1]);
            if(execvp(argv[pindex[i+1]],&argv[pindex[i+1]]) < 0)
                perror(argv[pindex[i+1]]);
        }
        else{
            close(fd[i+1][1]);
            wpid = Waitpid(pid,&status,0);
        }
    }
    if((pid = Fork()) < 0) { perror(argv[0]); exit(0); }
    if(pid == 0){
        close(STDIN_FILENO); 
        dup2(fd[pcount-1][0],STDIN_FILENO); 
        close(fd[pcount-1][0]); close(fd[pcount-1][1]);
        if(execvp(argv[pindex[pcount]],&argv[pindex[pcount]]) < 0)
            perror(argv[pindex[i+1]]);
    }
    else{
        wpid = Waitpid(pid,&status,0);
    }
    return;
}
/*-------------------------------------------------*/
void command_ls(char *const argv[]){
    if(execvp(argv[0],argv)<0)
        unix_error("Execvp error");
}
void command_cat(char *const argv[]){
    FILE *fp;
    char str[MAXLINE];
    if(argv[1] == NULL){
        if(execvp(argv[0],argv)<0)
            perror(argv[0]);
        return;
    }
    if(!(strcmp(argv[1],">"))){ // create file or cover existing file
        if((fp = fopen(argv[2],"w")) == NULL)
            printf("File write failed.\n");
        while(fgets(str, MAXLINE, stdin) != NULL){
            if(str[0] == 4) break; //Ctrl + d : exit
            fprintf(fp,"%s",str);
        }
        fclose(fp); return;
    }
    else if(!(strcmp(argv[1],">>"))){ //create file or append existing file
        if((fp = fopen(argv[2],"a")) == NULL)
            printf("File write failed.\n");
        while(fgets(str, MAXLINE, stdin) != NULL){
            if(str[0] == 4) break; //Ctrl + d : exit
            fprintf(fp,"%s",str);
        }
        fclose(fp); return;
    }
    if(execvp(argv[0],argv)<0)
        perror(argv[0]);
    return;
}
void command_echo(char * argv[]){
    int i,flag=0;
    char *ev,*pt,temp[MAXLINE],str[MAXLINE];
    if(argv[1] != NULL){ // echo [string]
        for(i=1;i<argc;i++){
            if(argv[i] == NULL) break;
            strcpy(temp,argv[i]);
            pt = argv[i];
            if(pt[0] == '`'){
               
            }
            if(pt[0] == '$' && strlen(pt) != 1){ //'echo 환경변수' 일때
                strcpy(temp,argv[i]+1);
                ev = getenv(temp); flag = 1;
                strcpy(temp,ev);
            }
            if(!(strcmp(pt,"\\\\"))){
                strcpy(temp,"\\"); flag = 1;
            }
            strcat(str,temp);
            strcat(str," ");
        }
        if(flag)
        printf("%s\n",str);
    }
    if(!flag)
        if(execvp(argv[0],argv)<0)
            perror(argv[0]);
}
void command_cd(char *const argv[]){
    if(argv[1] == NULL){ // go to home directory
            if(chdir(getenv("HOME"))<0)
                perror(argv[1]); // error message
    }
    else if(chdir(argv[1]) < 0)
        perror(argv[1]); // error message
}
void execute(const char *filename, char * argv[], char *const envp[]){
    if(!(strcmp(filename,"cd"))){ // child에서 어케 구현하냐
        return;
    }
    else if(!(strcmp(filename,"ls"))){
        if(execvp(argv[0],argv)<0)
            perror(filename);
    }
    else if(!(strcmp(filename,"mkdir"))){
        if(execvp(argv[0],argv)<0)
            perror(filename);
    }
    else if(!(strcmp(filename,"rmdir"))){
        if(execvp(argv[0],argv)<0)
            perror(filename);
    }
    else if(!(strcmp(filename,"cat"))){
        command_cat(argv);
    }
    else if(!(strcmp(filename,"touch"))){
        if(execvp(argv[0],argv)<0)
            perror(filename);
    }
    else if(!(strcmp(filename,"echo"))){
        command_echo(argv);
    }
    else if(!(strcmp(filename,"exit")))
        exit(1);
    else //command isn't exist
        //execvp(filename,argv);
         printf("%s: Command not found.\n", filename);
    return;
}
