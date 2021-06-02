/* $begin shellmain */
#include "myshell.h"
#include<errno.h>
#define MAXARGS   128
#define MAXJOBS 1024 /* MAX # of jobs*/
typedef struct job{
    pid_t pid;
    int number,stop;
    char target;
    char fb,state[30],cmd[100];
}Jobs;
Jobs joblist[MAXJOBS];
pid_t cpids[MAXJOBS]={0,};
char fcommands[MAXJOBS][MAXARGS];
int argc;/* Number of args, Number of jobs */
int wnum,fnum=0,fgflag,fgjn;
/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv,char* cmdline);
void execute(const char *filename, char * argv[]);
void command_cat(char *const argv[]);
void command_echo(char * argv[]);
void command_cd(char *const argv[]);
void pipe_cmd(char *const argv[],int *const pindex,int pcount);

void addjob(Jobs* job,pid_t id,char state,char* const command,int stp);
Jobs deletejob(Jobs* job,pid_t id);
void listjob(Jobs* job);
Jobs getjobpid(Jobs* job,pid_t id,int st);
int findtargetp(Jobs* job,int exc);
int findtargetm(Jobs* job,int exc);
int emptyindex(Jobs* job);
void changetarget(Jobs* job,int jnum);
void Waitfg(pid_t pid);

static void sigchild_handler(int sig);
static void sigint_handler(int sig);
static void sigtstp_handler(int sig);
static void sigcont_handler(int sig);
int main()
{
    char *pt,cmdline[MAXLINE]; /* Command line */
    wnum = 0;
    while (1) {
    /* Read */
        signal(SIGINT,sigint_handler);
        signal(SIGTSTP,sigtstp_handler);
        signal(SIGCHLD,sigchild_handler);
        signal(SIGCONT,sigcont_handler);

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
    int bg,status,pcnt=0,pflag=0,qflag,i,empty=1,len = (int)(strlen(cmdline)),lenpt;              /* Should the job run in bg or fg? */
    pid_t wpid,pid;           /* Process id */
    fgflag = 0;
    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    for(i=0;i<len;i++)
        if(cmdline[i] == '&'){
            cmdline[i] = '\0'; break;
        }
    if (argv[0] == NULL)
        return;   /* Ignore empty lines */
    
    /* parsing about quotes */
    for(i=0;argv[i] != NULL;i++){
        qflag  = 0;
        strcpy(pt,argv[i]);
        lenpt = (int)(strlen(pt));
        if((pt[0] == '\''&&pt[lenpt-1] == '\'') || (pt[0] == '\"'&&pt[lenpt-1] == '\"')){
            (argv[i])[(int)strlen(argv[i])-1] = '\0';
            strcpy(temp,argv[i]+1); qflag = 1;
        }
        else if(pt[lenpt-1] == '\"' || pt[lenpt-1] == '\''){
            (argv[i])[(int)strlen(argv[i])-1] = '\0';
            strcpy(temp,argv[i]); qflag = 1;
        }
        else if(pt[0] == '\"' || pt[0] == '\''){
            strcpy(temp,argv[i]+1); qflag = 1;
        }
        if(qflag)
            strcpy(argv[i],temp);
        memset(pt,'\0',lenpt);memset(temp,'\0',lenpt);
    }
    /*whether pipe*/
    for(i=0;argv[i] != NULL;i++){
        if(!(strcmp(argv[i],"|"))){
            pindex[++pcnt] = i+1; pflag = 1;
            argv[i] = NULL;
        }
    }
    
    if(!(builtin_command(argv,cmdline))){
        if((pid = Fork()) < 0) { //do fork
            perror(argv[0]); exit(0);
        }
        if(pid == 0){ /*child process*/
            if(setpgid(0,0)<0)
                perror(argv[0]);
            if(bg)//if process in background, ignore SIGINT
            {
                 signal(SIGINT,SIG_IGN);
            }
            else{
                signal(SIGINT,SIG_DFL);
            }
            if(pflag){ // pipe exist
                pipe_cmd(argv,pindex,pcnt);
            }
            else  //normal command
                execute(argv[0], argv);
            exit(0);
        }
        /* Parent process waits for foreground job to terminate */
        if(!bg){
            signal(SIGCHLD,NULL);
            cmdline[len-1] = '\0';
            strcpy(fcommands[fnum],cmdline);
            cpids[fnum++] = pid;
            wpid = Waitpid(pid,&status);
        }
        else{
            // Parent doesn't wait for background job
            for(i=1;i<=wnum;i++){
                if(joblist[i].number > 0){
                    empty =0; break;
                }
            }
            if(wnum == 0)
                printf("[%d] %d\n",++wnum,pid);
            else if(empty){
                wnum = 1;
                printf("[%d] %d\n",wnum,pid);
            }
            else if(emptyindex(joblist) < wnum){
                printf("[%d] %d\n",emptyindex(joblist),pid);
            }
            else{
                printf("[%d] %d\n",++wnum,pid);
            }
            addjob(joblist,pid,'b',cmdline,0);
            bg = 0;
        }
    }
    return;
}
int builtin_command(char **argv,char*cmdline)
{
    if (!strcmp(argv[0], "exit")) /* quit command */
        exit(0);
    if (!strcmp(argv[0], "cd")){
        command_cd(argv);
        return 1;
    }
    if (!strcmp(argv[0], "&"))   /* Ignore singleton & */
        return 1;
    if (!strcmp(argv[0], "jobs")){
        listjob(joblist);
        return 1;
    }
    if((strncmp(cmdline,"kill ",5))==0){
        if(argv[2] != NULL){
            perror(argv[2]);
            return 1;
        }
        if(argv[1][0] == '%'){
            int jnum =argv[1][1]-48;
            Jobs j = getjobpid(joblist,jnum,2);
            kill(j.pid,SIGKILL);
            printf("[%d] %c %d terminated\t\t%s\n",jnum,j.target,j.pid,j.cmd);
            deletejob(joblist,j.pid);
        }
        return 1;
    }
    if((strncmp(cmdline,"bg ",3))==0)
    {
        if(argv[2] != NULL){
            perror(argv[2]);
            return 1;
        }
        if(argv[1][0] == '%'){
            int jnum =argv[1][1]-48;
            Jobs j = getjobpid(joblist,jnum,2);
            joblist[jnum].fb = 'b';
            strcpy(joblist[jnum].state,"running");
            kill(j.pid,SIGCONT);
            printf("[%d] %c %d continued\t\t%s\n",jnum,j.target,j.pid,j.cmd);
        }
        else if(argv[1][0] < 48) {perror(argv[1]);return 1;}
        else{
            Jobs j=getjobpid(joblist,atoi(argv[1]),1);
            joblist[atoi(argv[1])].fb = 'b';
            strcpy(joblist[atoi(argv[1])].state,"running");
            kill(atoi(argv[1]),SIGCONT);
            printf("[%d] %c %d continued\t\t%s\n",atoi(argv[1]),j.target,j.pid,j.cmd);
        }
        return 1;
    }
    
    if((strncmp(cmdline,"fg ",3))==0)
    {
        if(argv[2] != NULL){
            perror(argv[2]);
            return 1;
        }
        if(argv[1][0] == '%'){
            fgflag = 1;
            int jnum=argv[1][1]-48;
            Jobs j=getjobpid(joblist,jnum,2);
            strcpy(fcommands[fnum],j.cmd);
            cpids[fnum++] = j.pid;
            joblist[jnum].fb = 'f';
            kill(j.pid,SIGCONT);
            printf("[%d] %c %d continued\t\t%s\n",jnum,j.target,j.pid,j.cmd);
            deletejob(joblist,j.pid);
            Waitfg(cpids[fnum-1]);
        }
        else if(argv[1][0] < 48) {perror(argv[1]);return 1;}
        else{
            fgflag = 1;
            Jobs j=getjobpid(joblist,atoi(argv[1]),1);
            strcpy(fcommands[fnum],j.cmd);
            cpids[fnum++] = j.pid;
            joblist[atoi(argv[1])].fb = 'f';
            kill(atoi(argv[1]),SIGCONT);
            printf("[%d] %c %d continued\t\t%s\n",atoi(argv[1]),j.target,j.pid,j.cmd);
            deletejob(joblist,j.pid);
            Waitfg(cpids[fnum-1]);
        }
        return 1;
    }
    
    return 0;                    /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv)
{
    char *delim;         /* Points to first space delimiter */
    int bg=0,i;              /* Background job? */
    for(i=0;i<(int)(strlen(buf));i++){
        if(buf[i] == '&'){
            bg = 1; buf[i] = ' ';
            break;
        }
    }
    buf[(int)(strlen(buf))-1] = ' ';  /* Replace trailing '\n' with space */
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

    return bg;
}
/* $end parseline */
/*-------------------------------------------------*/
static void sigchild_handler(int sig){
    int status;
    pid_t chpid;
    Jobs child;
    while((chpid = waitpid(-1,&status,WNOHANG|WUNTRACED|WCONTINUED))>0){ //terminated normally
        if((WIFEXITED(status))>0){
            child = deletejob(joblist,chpid);
            if(child.pid < 0)
                printf("job deleted already\n");
            else
                printf("[%d]  %c %d done\t\t%s\n",child.number,child.target,child.pid,child.cmd);
        }
        else if(WIFSIGNALED(status) != 0){ //terminated by signal
            
        }
        else if(WIFSTOPPED(status) == 1){
            child = getjobpid(joblist,chpid,1);
            if(child.pid < 0)
                printf("job doesn't exist\n");
            else
                printf("[%d] %c %s\t\t%s\n",child.number,child.target,child.state,child.cmd);
        }
    }
}
static void sigint_handler(int sig){
    int i;
    for(i=fnum-1;i>=0;i--){
        if(cpids[i]){
            kill(cpids[i],SIGKILL);
            strcpy(fcommands[i],"\0");
            fnum--;
        }
    }
    return;
}
static void sigtstp_handler(int sig){
    int i,f=0,ind = emptyindex(joblist);
    for(i=fnum-1;i>=0;i--){
        if(cpids[i]){
            if(ind == wnum) wnum++;
            addjob(joblist,cpids[i],'b',fcommands[i],1);
            kill(cpids[i],SIGTSTP);
            f = 1;
        }
    }
    if(f)
        fnum--;
    return;
}
static void sigcont_handler(int sig){
    int i;
    for(i=1;i<=wnum;i++){
        if(joblist[i].stop == 1){
            strcpy(joblist[i].state,"running");
            kill(joblist[i].pid,SIGCONT);
            printf("process back to continue\n");
        }
    }
    return;
}

/*-------------------------------------------------*/
Jobs getjobpid(Jobs* job,pid_t id,int st){
    Jobs temp; temp.pid = -1;
    int i;
    if(st == 2){
        for(i=1;i<=wnum;i++){
            if(job[i].number == id){
                return job[i];
            }
        }
    }
    else{
        for(i=1;i<=wnum;i++){
            if(job[i].number >0 && job[i].pid == id){
                if(st == 0){
                    job[i].stop = 1;
                    strcpy(job[i].state,"stopped");
                }
                return job[i];
            }
        }
    }
    return temp;
}
int findtargetp(Jobs* job,int exc){
    int i;
    for(i = 1;i<=wnum;i++){
        if(i != exc && job[i].target == '+')
            return i;
    }
    return -1;
}
int findtargetm(Jobs* job,int exc){
    int i;
    for(i = 1;i<=wnum;i++){
        if(i != exc && job[i].target == '-')
            return i;
    }
    return -1;
}
void changetarget(Jobs* job,int jnum){
    int p,m;
    p = findtargetp(job,jnum);
    m = findtargetm(job,jnum);
    job[jnum].target = '+';
    if(p != -1)
        job[p].target = '-';
    if(m != -1)
        job[m].target = ' ';
}
int emptyindex(Jobs* job){
    int i,f=1;
    for(i=1;i<wnum;i++){
        if(job[i].number > 0){
            f=0; break;
        }
    }
    if(f){
        wnum = 1;
        return wnum;
    }
    for(i=1;i<wnum;i++){
        if(job[i].number < 0){
            return i;
        }
    }
    return wnum;
}
void addjob(Jobs* job,pid_t id,char state,char* const command,int stp){
    int ind;
    ind = emptyindex(job);
    if(fgflag)
        ind = fgjn;
    changetarget(job,ind);
    job[ind].fb = state; job[ind].pid = id; job[ind].stop = stp;
    job[ind].target = '+'; strcpy(job[ind].cmd,command);
    job[ind].number = ind;
    if(stp == 1)
        strcpy(job[ind].state,"stopped");
    else
        strcpy(job[ind].state,"running");
    return;
}
Jobs deletejob(Jobs* job,pid_t id){
    Jobs temp;
    temp.pid = -1;
    int i;
    for(i=1;i<=wnum;i++){
        if(job[i].pid == id){
            if(fgflag == 1) {fgjn = job[i].number;}
            temp.fb = job[i].fb ; temp.number = job[i].number; temp.pid = job[i].pid;
            temp.target = job[i].target ; strcpy(temp.cmd,job[wnum].cmd); temp.stop = job[i].stop;
            strcpy(temp.state,"terminated");
            job[i].number = -1;
        }
    }
    return temp;
}
void listjob(Jobs* job){
    int i;
    for(i =1;i<=wnum;i++){
        if(job[i].number >0)
            printf("[%d]  %c %s\t\t%s\n",job[i].number,job[i].target,job[i].state,job[i].cmd);
    }
}
/*-------------------------------------------------*/
void Waitfg(pid_t pid){
    Jobs temp;
    int flag=1,i;
    temp.pid = -1;
    while(temp.pid == -1 && flag){
        sleep(1);
        temp = getjobpid(joblist,pid,1);
        flag = 0;
        for(i=0;i<fnum;i++){
            if(cpids[i] == pid){
                flag = 1;
                break;
            }
        }
    }
}
pid_t Waitpid(pid_t pid, int *iptr)
{
    Jobs child;
    pid_t retpid;
    int i,j,f=0;
    if ((retpid  = waitpid(pid, iptr, WUNTRACED )) < 0 )
        printf("%s: %s\n", "waitpid error", strerror(errno));
    if(WIFSTOPPED(*iptr)){
        child = getjobpid(joblist,pid,1);
        printf("[%d] %c  %s\t\t%s\n",child.number,child.target,child.state,child.cmd);
    }
    else if(WIFSIGNALED(*iptr)){
        if(*iptr == 2 || *iptr == 15)
            return(retpid);
    }
    else if(!WIFEXITED(*iptr))// child terminate abnormally
        printf("Child %d terminate abnormally!\n",retpid);
    else{
        for(i=0;i<fnum;i++){
            if(cpids[i] == pid){
                f = 1; break;
            }
        }
        if(f){
            for(j=i;j<fnum-1;j++)
                cpids[j] = cpids[j+1];
            strcpy(fcommands[j],"\0");
            fnum--;
        }
    }
    return(retpid);
}
pid_t Fork(void){
    pid_t pid;
    if ((pid = fork()) < 0)
        printf("%s: %s\n", "fork error", strerror(errno));
    return pid;
}
void pipe_cmd(char *const argv[],int *const pindex,int pcount){
    pid_t pid,wpid;
    int i,status,fd[MAXARGS][2] = {0,};
    
    if(pipe(fd[0]) < 0) // make pipe
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
        signal(SIGCHLD,NULL);
        close(fd[0][1]);
        wpid = Waitpid(pid,&status);
    }
    for(i=0;i<pcount-1;i++){
        if(pipe(fd[i+1]) < 0)
            perror(argv[0]);
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
            signal(SIGCHLD,NULL);
            close(fd[i+1][1]);
            wpid = Waitpid(pid,&status);
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
        signal(SIGCHLD,NULL);
        if((wpid = Waitpid(pid,&status)) < 0)
           perror(argv[0]);
    }
    return;
}
/*-------------------------------------------------*/
void command_cat(char *const argv[]){
    FILE *fp;
    char str[MAXLINE];
    if(argv[1] == NULL){ // only cat command
        //system("cat");
        while(fgets(str, MAXLINE, stdin) != NULL && (str[0] != 4)){ //Ctrl + d : exit
            fputs(str,stdout);
            fflush(stdout);
        }
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
    int i,flag=0,lenpt;
    char *ev,*pt,temp[MAXLINE],str[MAXLINE];
    if(argv[1] != NULL){ // echo [string]
        for(i=1;i<argc;i++){
            if(argv[i] == NULL) break;
            strcpy(temp,argv[i]);
            pt = argv[i]; lenpt = (int)(strlen(pt));
            if(pt[0] == '`'){
                
            }
            if(pt[0] == '$' && lenpt != 1){ //'echo 환경변수' 일때
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
void execute(const char *filename, char * argv[]){
    if(!(strcmp(filename,"cd")) || !(strcmp(filename,"jobs")) || !(strcmp(filename,"exit"))){
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
        if(execvp(filename,argv)<0)
            perror(filename);
        // printf("%s: Command not found.\n", filename);
    return;
}
