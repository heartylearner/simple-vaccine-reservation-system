#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <sys/time.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)
typedef struct {
    int id;          //902001-902020
    int AZ;
    int BNT;
    int Moderna;
}registerRecord;
registerRecord rR[20];

FILE *fp;
char *va[10];
int stage[10000];
int my_rdlck[10000],my_wrlck[10000];
typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    int buf_len;  // bytes used by buf
    // you don't need to change this.
    int id;
    int wait_for_write;  // used by handle_read to know if the header is read or not.
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request listconst char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";
char s81[512]="Preference order for 902001 modified successed, new preference order is AZ > BNT > Moderna.\n";
char s82[512]="Preference order for 902001 modified successed, new preference order is AZ > Moderna > BNT.\n";
char s83[512]="Preference order for 902001 modified successed, new preference order is BNT > AZ > Moderna.\n";
char s84[512]="Preference order for 902001 modified successed, new preference order is BNT > Moderna > AZ.\n";
char s85[512]="Preference order for 902001 modified successed, new preference order is Moderna > AZ > BNT.\n";
char s86[512]="Preference order for 902001 modified successed, new preference order is Moderna > BNT > AZ.\n";
char s41[512]="Your preference order is AZ > BNT > Moderna.\n";
char s42[512]="Your preference order is AZ > Moderna > BNT.\n";
char s43[512]="Your preference order is BNT > AZ > Moderna.\n";
char s44[512]="Your preference order is BNT > Moderna > AZ.\n";
char s45[512]="Your preference order is Moderna > AZ > BNT.\n";
char s46[512]="Your preference order is Moderna > BNT > AZ.\n";
char *BUF;
static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

int Atoi(char *s,int l,int r){
	int ans=0;
	for(int i=l;i<=r;++i){
		ans*=10;
		ans+=s[i]-'0';
	}
	return ans;
}
char* Itoa(int x){
    char *s=(char*)malloc(sizeof(char)*10);
    s[0]='9';s[1]='0';s[2]='2';s[3]='0';
    if(x<902010){
        s[4]='0';
        s[5]=x-902000+'0';
    }
    else if(x<902020){
        s[4]='1';
        s[5]=x-902010+'0';
    }
    else{
        s[4]='2';s[5]='0';
    }
    s[6]='\0';
    return s;
}
int cp(char *from,int L,int R,char *to,int l){
    for(int i=L;i<=R;++i)
        to[i-L+l]=from[i];
    return R-L+1;
}
int handle_read(request* reqP) {
    int r;
    char buf[512];
    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
    char* p1 = strstr(buf, "\015\012");
    int newline_len = 2;
    if (p1 == NULL) {
       p1 = strstr(buf, "\012");
        if (p1 == NULL) {
            ERR_EXIT("this really should not happen...");
        }
    }
    size_t len = p1 - buf + 1;
    memmove(reqP->buf, buf, len);
    reqP->buf[len - 1] = '\0';
    reqP->buf_len = len-1;
    return 1;
}
int lock_reg(int cmd,int type,int id){
    id-=902001;
	struct flock L;
    L.l_type=type;
    L.l_start=(id)*(sizeof(registerRecord));
    L.l_whence=SEEK_SET;
    L.l_len=sizeof(registerRecord);
    if(type==F_RDLCK){
        if(my_rdlck[id]){
            ++my_rdlck[id];
            printf("lock! %d~%d\n",L.l_start,L.l_start+L.l_len);
            return 1;
        }
        int x=fcntl(fileno(fp),cmd,&L);
        if(x!=-1){
            ++my_rdlck[id];
            //printf("lock! %d~%d\n",L.l_start,L.l_start+L.l_len);
            return 1;
        }
        else{
            //printf("since fd lock!\n");
            return -1;
        }
    }
    else if(type==F_WRLCK){
        if(my_wrlck[id]){
            //printf("since inside locked");
            return -1;
        }
        int x=fcntl(fileno(fp),cmd,&L);
        if(x!=-1){
            my_wrlck[id]=1;
            //printf("lock!\n");
            return 1;
        }
        else{
            printf("since fd lock!\n");
            return -1;
        }
    }
    else{
        //printf("unlock!\n");
        if(my_wrlck[id])
            my_wrlck[id]=0;
        else
            --my_rdlck[id];
        //printf("1\n");
        fcntl(fileno(fp),cmd,&L);
        return 1;
    }
}
void my_read(int id){

    //fseek(fp,(id-902000)*sizeof(registerRecord),SEEK_SET);
    fsync(fileno(fp));
    fseek(fp,(id-902001)*sizeof(registerRecord),SEEK_SET);
    fread(rR+id-902001,sizeof(registerRecord),1,fp);
}
void my_write(int id){
    fseek(fp,(id-902001)*sizeof(registerRecord),SEEK_SET);
    fwrite(rR+id-902001,sizeof(registerRecord),1,fp);
    //lseek(FD,(id-902001)*sizeof(registerRecord),SEEK_SET);
    //write(FD,rR+id-902001,sizeof(registerRecord));
    fsync(fileno(fp));
}
void stage1(request* reqP){
    printf("in stage1\n");
    #ifdef READ_SERVER
            reqP->wait_for_write=0;
    #elif defined WRITE_SERVER
            reqP->wait_for_write=1;
    #endif
    int fd=reqP->conn_fd;
    stage[fd]=2;
    char buf[512]="Please enter your id (to check your preference order):\n";
    write(fd,buf,strlen(buf));
}
void stage2(request* reqP){
    printf("in stage2\n");
    int fd=reqP->conn_fd;
    stage[fd]=3;
    handle_read(reqP);
    if(reqP->buf_len!=6){
        reqP->id=-1;
        return;
    }
    int id=Atoi(reqP->buf,0,7-2);
    if(!(id>902000&&id<=902020)){
        reqP->id=-1;
        return;
    }
    reqP->id=id;
}
int stage3(request* reqP){
    int fd=reqP->conn_fd;
    double t=clock();
    printf("in stage3\n");
    if(reqP->id==-1){
        write(fd,"[Error] Operation failed. Please try again.\n",44);
        return 0;
    }
    printf("time 1=%lf\n",clock()-t);t=clock();
    if(reqP->wait_for_write){
    	if(lock_reg(F_SETLK,F_WRLCK,reqP->id)==-1){
        	write(fd,"Locked.\n",8);
        	return 0;
    	}
    }
    else{
        if(lock_reg(F_SETLK,F_RDLCK,reqP->id)==-1){
            write(fd,"Locked.\n",8);
            return 0;
    	}
    }
    printf("time 2=%lf\n",clock()-t);t=clock();
    my_read(reqP->id);
    printf("time 3=%lf\n",clock()-t);t=clock();
    if(reqP->wait_for_write){
        stage[fd]=4;
    }
    else{
        lock_reg(F_SETLK,F_UNLCK,reqP->id);
    }
    if(rR[reqP->id-902001].AZ==1){
        if(rR[reqP->id-902001].BNT==2)
            write(fd,s41,45);
        else
            write(fd,s42,45);
    }
    else if(rR[reqP->id-902001].AZ==2){
        if(rR[reqP->id-902001].BNT==1)
            write(fd,s43,45);
        else
            write(fd,s45,45);
    }
    else{
        if(rR[reqP->id-902001].BNT==1)
            write(fd,s44,45);
        else
            write(fd,s46,45);
    }
    printf("time 4=%lf\n",clock()-t);t=clock();
    /*int s[4];
    s[rR[reqP->id-902001].AZ]=1;s[rR[reqP->id-902001].BNT]=2;s[rR[reqP->id-902001].Moderna]=3;
    int l=0;
    l+=cp("Your preference order is ",0,25-1,reqP->buf,l);
    l+=cp(va[s[1]],0,strlen(va[s[1]])-1,reqP->buf,l);
    l+=cp(" > ",0,2,reqP->buf,l);
    l+=cp(va[s[2]],0,strlen(va[s[2]])-1,reqP->buf,l);
    l+=cp(" > ",0,2,reqP->buf,l);
    l+=cp(va[s[3]],0,strlen(va[s[3]])-1,reqP->buf,l);
    l+=cp(".\n",0,2,reqP->buf,l);
    reqP->buf[l]='\0';
    write(fd,reqP->buf,l+1);*/
    return 1;
}
void stage4(request* reqP){
    printf("in stage4\n");
    int fd=reqP->conn_fd;
    stage[fd]=5;
    char buf[512]="Please input your preference order respectively(AZ,BNT,Moderna):\n";
    write(fd,buf,strlen(buf));
}
void stage5(request* reqP){
    printf("in stage5\n");
    int fd=reqP->conn_fd;
    stage[fd]=6;
    handle_read(reqP);
    if(reqP->buf_len!=5){
        lock_reg(F_SETLK,F_UNLCK,reqP->id);
        reqP->id=-1;
        return;
    }
    char *buf=reqP->buf;
    reqP->buf[5]='\0';
    if(!(((strcmp("1 2 3",buf)==0)||(strcmp("1 3 2",buf)==0)||(strcmp("2 1 3",buf)==0)
       ||(strcmp("2 3 1",buf)==0)||(strcmp("3 1 2",buf)==0)||(strcmp("3 2 1",buf)==0)))){
        lock_reg(F_SETLK,F_UNLCK,reqP->id);
        reqP->id=-1;
        return;
    }
    rR[reqP->id-902001].AZ=(int)(reqP->buf[0]-'0');
    rR[reqP->id-902001].BNT=(int)(reqP->buf[2]-'0');
    rR[reqP->id-902001].Moderna=(int)(reqP->buf[4]-'0');
}
int stage6(request* reqP){
    printf("in stage6\n");
    int fd=reqP->conn_fd;
    //stage[fd]=8;
    if(reqP->id==-1){
        write(fd,"[Error] Operation failed. Please try again.\n",44);
        return 0;
    }
    my_write(reqP->id);
    lock_reg(F_SETLK,F_UNLCK,reqP->id);
    if(rR[reqP->id-902001].AZ==1){
        if(rR[reqP->id-902001].BNT==2){
            s81[25]=(char)((reqP->id/10)%10+'0');
            s81[26]=(char)(reqP->id%10+'0');
            write(fd,s81,92);
        }
        else{
            s82[25]=(char)((reqP->id/10)%10+'0');
            s82[26]=(char)(reqP->id%10+'0');
            write(fd,s82,92);
        }
    }
    else if(rR[reqP->id-902001].AZ==2){
        if(rR[reqP->id-902001].BNT==1){
            s83[25]=(char)((reqP->id/10)%10+'0');
            s83[26]=(char)(reqP->id%10+'0');
            write(fd,s83,92);
        }
        else{
            s85[25]=(char)((reqP->id/10)%10+'0');
            s85[26]=(char)(reqP->id%10+'0');
            write(fd,s85,92);
        }
    }
    else{
        if(rR[reqP->id-902001].BNT==1){
            s84[25]=(char)((reqP->id/10)%10+'0');
            s84[26]=(char)(reqP->id%10+'0');
            write(fd,s84,92);
        }
        else{
            s86[25]=(char)((reqP->id/10)%10+'0');
            s86[26]=(char)(reqP->id%10+'0');
            write(fd,s86,92);
        }
    }
    return 1;
}
int main(int argc, char** argv) {

	va[1]=(char*)malloc(sizeof(char)*10);
	va[2]=(char*)malloc(sizeof(char)*10);

	va[3]=(char*)malloc(sizeof(char)*10);

	va[1]="AZ";
	va[2]="BNT";
	va[3]="Moderna";
	for(int i=0;i<maxfd;++i)
        my_rdlck[i]=my_wrlck[i]=0;
    fp=fopen("registerRecord","r+");
    setbuf(fp,NULL);
    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen,y=0;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[512];
    int buf_len;

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));
    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);
    fd_set read_fds,write_fds,cur1_fds,cur2_fds;
    FD_ZERO(&read_fds);FD_ZERO(&write_fds);
    FD_SET(svr.listen_fd,&read_fds);
    struct timeval ti,start,end;
    ti.tv_sec=0;
    ti.tv_usec=50000;
    double t,t2;
    while (1) {
	// TODO: Add IO multiplexing
	//printf("in todo 1\n");
        //cur_fds=read_fds;
	//memcpy(&cur1_fds,&read_fds,sizeof(read_fds));memcpy(&cur2_fds,&write_fds,sizeof(write_fds));
        //cur1_fds=read_fds;cur2_fds=write_fds;
        memcpy(&cur1_fds,&read_fds,sizeof(read_fds));memcpy(&cur2_fds,&write_fds,sizeof(write_fds));
	    int x=select(maxfd,&cur1_fds,&cur2_fds,NULL,(struct timeval *) 0);
	    gettimeofday(&start,NULL);
	    printf("x=%d\n",x);
        if(x<1)continue;
        for(int i=0;(i<maxfd&&x);++i){
            if(i==svr.listen_fd){
                if(FD_ISSET(svr.listen_fd,&cur1_fds)){
                    clilen = sizeof(cliaddr);
                    conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
                    if (conn_fd < 0) {
                        if (errno == EINTR || errno == EAGAIN) continue;  // try again
                        if (errno == ENFILE) {
                            (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                            continue;
                        }
                        ERR_EXIT("accept");
                    } 
                    FD_SET(conn_fd,&write_fds);FD_SET(conn_fd,&read_fds);
                    stage[conn_fd]=1;
                    requestP[conn_fd].conn_fd = conn_fd;
                    strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
                    fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
                    /*int ret = handle_read(&requestP[conn_fd]); // parse data from client to requestP[conn_fd].buf
                    if (ret < 0) {
                        fprintf(stderr, "bad request from %s\n", requestP[conn_fd].host);
                        continue;
                    }*/
                    --x;
                    /*#ifdef READ_SERVER
                            fprintf(stderr, "%s", requestP[conn_fd].buf);
                            sprintf(buf,"%s : %s",accept_read_header,requestP[conn_fd].buf);
                            //write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                    #elif defined WRITE_SERVER
                            fprintf(stderr, "%s", requestP[conn_fd].buf);
                            sprintf(buf,"%s : %s",accept_write_header,requestP[conn_fd].buf);
                            //write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                    #endif*/
                }
            }
            else if(FD_ISSET(i,&cur2_fds)){
                --x;
                switch(stage[i]){
                case 1:
                    stage1(&requestP[i]);
                    gettimeofday(&end,NULL);
                    printf("time=%ld\n",end.tv_usec-start.tv_usec);fflush(stdin);
                    FD_CLR(i,&write_fds);
                    break;
                case 3:
                    if((!stage3(&requestP[i]))||(!requestP[i].wait_for_write)){
                        FD_CLR(i,&write_fds);
                        FD_CLR(i,&read_fds);
                        close(requestP[i].conn_fd);
                        free_request(&requestP[i]);
                    }
                    gettimeofday(&end,NULL);
                    printf("time=%ld\n",end.tv_usec-start.tv_usec);fflush(stdin);
                    break;
                case 4:
                    stage4(&requestP[i]);
                    FD_CLR(i,&write_fds);
                    gettimeofday(&end,NULL);
                    printf("time=%ld\n",end.tv_usec-start.tv_usec);fflush(stdin);
                    break;
                case 6:
                    stage6(&requestP[i]);
                    gettimeofday(&end,NULL);
                    printf("time=%ld\n",end.tv_usec-start.tv_usec);fflush(stdin);
                    FD_CLR(i,&write_fds);
                    FD_CLR(i,&read_fds);
                    close(requestP[i].conn_fd);
                    free_request(&requestP[i]);
                    break;
                }
            }
            else if(FD_ISSET(i,&cur1_fds)){
                --x;
                switch(stage[i]){
                case 2:
                    stage2(&requestP[i]);
                    gettimeofday(&end,NULL);
                    printf("time=%ld\n",end.tv_usec-start.tv_usec);fflush(stdin);
                    FD_SET(i,&write_fds);
                    break;
                case 5:
                    stage5(&requestP[i]);
                    gettimeofday(&end,NULL);
                    printf("time=%ld\n",end.tv_usec-start.tv_usec);fflush(stdin);
                    FD_SET(i,&write_fds);
                    break;

                }
            }
        }
        // Check new connection
    }

        // TODO: handle requests from clients
    free(requestP);
    return 0;
}

// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->id = 0;
}

static void free_request(request* reqP) {
    /*if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }*/
    init_request(reqP);
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
	    ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
	    ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
	    ERR_EXIT("listen");
    }

    // Get file descripter table size and initialize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    return;
}
