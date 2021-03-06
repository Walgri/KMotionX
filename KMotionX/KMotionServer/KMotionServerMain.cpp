/*
Copyright (c) 2014, P.Hansson
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project.
 */

// KMotionServerMain.cpp 



#include <HiResTimer.h>
#include <ftd2xx.h>
#include <MessageBox.h>
#include <CMutex.h>
#include <WinTypes.h>
#include <unistd.h>     /* Symbolic Constants */
#include <sys/types.h>  /* Primitive System Data Types */
#include <errno.h>      /* Errors */
#include <stdio.h>      /* Input/Output */
#include <stdlib.h>     /* General Utilities */
#include <pthread.h>    /* POSIX Threads */
#include <string.h>     /* String handling */
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <syslog.h>
#include <fcntl.h>
#include <list>
#include <signal.h>
#include <sys/syscall.h>
    //__NR_gettid

#include <KMotionDLL.h>
#include <KmotionIO.h>
#include <KMotionDLL_Direct.h>


#define BUFSIZE 4096
#define PIPE_TIMEOUT 10000

CKMotionDLL_Direct KMotionDLL;
void * InstanceThread(void *ptr);
void GetAnswerToRequest(char *chRequest, unsigned int nInBytes, char *chReply, unsigned int *cbReplyBytes, int hPipe);
int ConsoleHandler(int board, const char *buf);

int nClients = 0;

/* struct to hold data to be passed to a thread
   this shows how multiple data items can be passed to a thread */
typedef struct str_thdata
{
    int thread_no;
    int client_no;
    int file_desc;
    char message[100];
} thdata;

//Array of console socket file descriptor
int ConsolePipeHandle[MAX_BOARDS];

std::list<char*> ConsoleList[MAX_BOARDS];
const char LOCK_CODES[][22]={"KMOTION_LOCKED","KMOTION_IN_USE","KMOTION_NOT_CONNECTED"};
const char ENUM_NAMES[][35]={
		"ENUM_WriteLineReadLine",
		"ENUM_WriteLine",
		"ENUM_WriteLineWithEcho",
		"ENUM_ReadLineTimeOut",
		"ENUM_ListLocations",
		"ENUM_Failed",
		"ENUM_Disconnect",
		"ENUM_FirmwareVersion",
		"ENUM_CheckForReady",
		"ENUM_KMotionLock",
		"ENUM_USBLocation",
		"ENUM_KMotionLockRecovery",
		"ENUM_ReleaseToken",
		"ENUM_ServiceConsole",
		"ENUM_SetConsole"};


#define SOCK_PATH "kmotionsocket"


void MyErrExitThread(const char *s, int thread_socket){
	syslog(LOG_ERR, "%s", s);
	shutdown(thread_socket,2);
	close(thread_socket);
	pthread_exit(0);
}

void MyErrExit(const char *s)
{

	syslog(LOG_ERR, "%s", s);
	closelog();
	exit(1);
}
void logError(const char *s){
	syslog(LOG_ERR, "%s: %s", s, strerror(errno));
}

void perrorExitThread(const char *s, int thread_socket)
{
	logError(s);
	MyErrExitThread(s,thread_socket);
}


void perrorExit(const char *s)
{
	logError(s);
	MyErrExit(s);
}
#ifdef _DEAMON
/**
 * Uses double fork to kill session stuff.
 */
static void daemonize2()
{
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Catch, ignore and handle signals */
    //TODO: Implement a working signal handler */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    //chdir("/");

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>0; x--)
    {
        close (x);
    }

}
static void daemonize(){
    /* Our process ID and Session ID */
    pid_t pid, sid;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
            exit(EXIT_FAILURE);
    }
    /* If we got a good PID, then
       we can exit the parent process. */
    if (pid > 0) {
            exit(EXIT_SUCCESS);
    }

    /* Change the file mode mask */
    umask(0);

    /* Open any logs here */

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
    }

    /* Catch, ignore and handle signals */
    //TODO: Implement a working signal handler */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);


    /* Change the current working directory */
    //Socketpath must be correct if we change directory
    //if ((chdir("/")) < 0) {
            /* Log the failure */
      //      exit(EXIT_FAILURE);
    //}

    /* Close out the standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

}
#endif

int main(void) {
#ifdef _DEAMON
	//daemonize2();
	daemonize();
#endif
    //http://robertoacevedo.net/blog/2012/12/03/socket-server/
    pthread_attr_t attr; // Thread attribute
    pthread_attr_init(&attr); // Creating thread attributes
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO); // FIFO scheduling for threads
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED); // Don't want threads (particualrly main)
                                                                   // waiting on each other

	openlog("KMotionServer", LOG_PID|LOG_CONS, LOG_USER);
	syslog(LOG_ERR, "KMotionServer started ");


    int main_socket;
    unsigned int t;
    struct sockaddr_un local, remote;


    if ((main_socket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    	perrorExit("socket");
    }

    if (strlen(SOCK_PATH) >= sizeof(local.sun_path)) {
    	perrorExit("path too long!");
    }

    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, SOCK_PATH);

    int unlink_err;
    if((unlink_err = unlink(local.sun_path))){
    	logError("unlink");
    }

    //http://idletechnology.blogspot.se/2011/12/unix-domain-sockets-on-osx.html
#ifdef __APPLE__
    local.sun_len = sizeof(local);
    if (bind(main_socket, (struct sockaddr *)&local, SUN_LEN(&local)) == -1) {
#else
    int len;
    len = strlen(local.sun_path) + sizeof(local.sun_family);
    if (bind(main_socket, (struct sockaddr *)&local, len) == -1) {
#endif
    	perrorExit("bind");
    }

    if (listen(main_socket, 5) == -1) {
    	perrorExit("listen");
    }

   for (int i=0; i<MAX_BOARDS; i++) ConsolePipeHandle[i]=0;
 
// The main loop creates an instance of the named pipe and 
// then waits for a client to connect to it. When the client 
// connects, a thread is created to handle communications 
// with that client, and the loop is repeated. 
	//pthread_t ct = pthread_self();
	//printf("Thread %.8x %.8x: Current thread\n", ct);
   for (;;) 
   { 

       syslog(LOG_ERR,"Main Thread. Waiting for a connection...\n");
       t = sizeof(remote);
       int client_socket;
       if ((client_socket = accept(main_socket, (struct sockaddr *)&remote, &t)) == -1) {
    	   perrorExit("Main Thread. accept");
       } else {
    	   //struct timeval tv;

    	   //tv.tv_sec = 5;  /* 30 Secs Timeout */

    	   //setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));

    	   syslog(LOG_ERR,"Main Thread. Connected descriptor %d \n",client_socket);
    	   nClients++;

    	   syslog(LOG_ERR,"Main Thread. Spawning worker\n");
			pthread_t thr;
			// initialize data to pass to thread
			thdata data;
			data.thread_no = nClients;
			data.file_desc = client_socket;
			strcpy(data.message, "Hi!");

 		    if(pthread_create(&thr, &attr, &InstanceThread, (void *) &data))
 		    {
 		        MyErrExit("Main Thread. pthread_create");
 		        //return -1;
 		    }

 		    //if(pthread_join(thr, NULL))
 		    //{
 		    //    printf("Could not join thread\n");
 		    //    return -1;
 		    //}
       }

   }
   closelog();
   exit(EXIT_SUCCESS);
} 
//http://www.amparo.net/ce155/thread-ex.html
void * InstanceThread(void *ptr){
	thdata *data = (thdata*) ptr;
	int thread_socket = data->file_desc;
	//pthread_t ct = pthread_self();
	//printf("Thread %.8x %.8x: Current thread\n", ct);
	syslog(LOG_ERR,"The ID of this of this thread is: %ld\n", syscall(SYS_gettid/*224*/));

	syslog(LOG_ERR,"Worker Thread. Nr of Clients when entered %d", nClients);
	//vsyslog(LOG_INFO, "Inside Thread %d\n", thread_socket);
	syslog(LOG_ERR, "Worker Thread. Inside Thread %d\n", thread_socket);

	char chRequest[BUFSIZE];
	char chReply[BUFSIZE];
	unsigned int cbReplyBytes;
	int cbBytesRead, cbWritten;


	while(true) {

		cbBytesRead = recv(thread_socket, chRequest, BUFSIZE, 0);
		if (cbBytesRead <= 0) {
			if (cbBytesRead < 0){
				logError("Worker Thread. recv");
			} else {
				syslog(LOG_ERR,"Worker Thread. received 0 bytes");
			}
			break;
		}

		GetAnswerToRequest(chRequest, cbBytesRead, chReply, &cbReplyBytes, thread_socket);
		cbWritten = send(thread_socket, chReply, cbReplyBytes, 0);

		if (cbWritten < 0 || cbReplyBytes != cbWritten) {
			if (cbWritten < 0){
				logError("Worker Thread. send");
			} else {
				syslog(LOG_ERR,"Worker Thread. %d bytes written != %d bytes sent\n",cbWritten,cbReplyBytes);
			}
			break;
		}
	}
	syslog(LOG_ERR,"Worker Thread. Exiting thread %d", thread_socket);

	// Flush the pipe to allow the client to read the pipe's contents
	// before disconnecting. Then disconnect the pipe, and close the
	// handle to this pipe instance.

	//	   FlushFileBuffers(thread_socket);
	//	   DisconnectNamedPipe(thread_socket);
	//	   CloseHandle(thread_socket);
	if(shutdown(thread_socket,SHUT_RDWR)< 0){
		logError("thread socket shutdown");
	}

	close(thread_socket);
	if (--nClients <= 0){
		//syslog(LOG_ERR,"Worker Thread. Nr of clients %d left. Terminating server", nClients);
		//_exit(0);              // nobody left - terminate server
	}
	syslog(LOG_ERR,"Worker Thread. Nr of clients left %d", nClients);
	//If exit() is executed in the processs that started the server we never get here
	//Hence the reference counting from KMotionDLl.nInstances must be made.
	   //if (KMotionDLL.nInstances() < 2) exit(0);  // nobody left - terminate server
	pthread_exit(0);
	return 0;
}


void GetAnswerToRequest(char *chRequest, unsigned int nInBytes, char *chReply, unsigned int *cbReplyBytes, int hPipe)
{

	int code, BoardID, board, TimeOutms, result=0, nLocations, List[256];

	memcpy(&code, chRequest,4);

	if (code!=ENUM_ListLocations)  // all commands require a board to be mapped, except this command
	{
		memcpy(&BoardID, chRequest+4,4);
		board=KMotionDLL.MapBoardToIndex(BoardID);
	}

	chReply[0]=DEST_NORMAL;

	switch (code)
	{

	case ENUM_WriteLineReadLine:	// Send Code, board, string -- Get Dest byte, Result (int) and string
		result = KMotionDLL.WriteLineReadLine(board, chRequest+8, chReply+1+4);
		syslog(LOG_ERR,"GetAnswerToRequest %s \n\trequest=%s \n\treply=%s \n",ENUM_NAMES[code],chRequest+8,chReply+1+4);
		memcpy(chReply+1, &result,4);
		*cbReplyBytes = 1+4+strlen(chReply+1+4)+1; // Dest byte, Result int, string, null char
		break;

	case ENUM_WriteLine:	
		result = KMotionDLL.WriteLine(board, chRequest+8);
		syslog(LOG_ERR,"GetAnswerToRequest %s \n\trequest=%s \n\tresult=%d \n",ENUM_NAMES[code],chRequest+8,result);
		memcpy(chReply+1, &result,4);
		*cbReplyBytes=1+4;
		break;

	case ENUM_WriteLineWithEcho:	
		result = KMotionDLL.WriteLineWithEcho(board, chRequest+8);
		syslog(LOG_ERR,"GetAnswerToRequest %s \n\trequest=%s \n\tresult=%d \n",ENUM_NAMES[code],chRequest+8,result);
		memcpy(chReply+1, &result,4);
		*cbReplyBytes=1+4;
		break;

	case ENUM_ReadLineTimeOut:	// Send Code, board, timeout -- Dest byte, Get Result (int), and string
		memcpy(&TimeOutms, chRequest+8,4);
		result = KMotionDLL.ReadLineTimeOut(board, chReply+1+4 ,TimeOutms);
		syslog(LOG_ERR,"GetAnswerToRequest %s \n\trequest=%d \n\treply=%s \n\tresult=%d \n",ENUM_NAMES[code],TimeOutms,chReply+1+4,result);
		memcpy(chReply+1, &result,4);
		*cbReplyBytes = 1+4+strlen(chReply+1+4)+1; // Dest byte, Result int, string, null char
		break;

	case ENUM_ListLocations:		// Send Code -- Get Dest, Result (int), nlocations (int), List (ints)
		syslog(LOG_ERR,"GetAnswerToRequest %s\n",ENUM_NAMES[code]);
		result = KMotionDLL.ListLocations(&nLocations, List);
		memcpy(chReply+1, &result,4);
		memcpy(chReply+1+4, &nLocations,4);
		memcpy(chReply+1+8, List, nLocations*4);
		*cbReplyBytes = 1+4+4+4*nLocations; // Dest byte, Result int, string, null char
		break;

	case ENUM_Failed:	
		result = KMotionDLL.Failed(board);
		syslog(LOG_ERR,"GetAnswerToRequest %s %d\n",ENUM_NAMES[code],result);
		memcpy(chReply+1, &result,4);
		*cbReplyBytes=1+4;
		break;

	case ENUM_Disconnect:	
		result = KMotionDLL.Disconnect(board);
		syslog(LOG_ERR,"GetAnswerToRequest %s %d\n",ENUM_NAMES[code],result);
		memcpy(chReply+1, &result,4);
		*cbReplyBytes=1+4;
		break;

	case ENUM_FirmwareVersion:	
		result = KMotionDLL.FirmwareVersion(board);
		syslog(LOG_ERR,"GetAnswerToRequest %s %d\n",ENUM_NAMES[code],result);
		memcpy(chReply+1, &result,4);
		*cbReplyBytes=1+4;
		break;

	case ENUM_CheckForReady:	
		result = KMotionDLL.CheckForReady(board);
		syslog(LOG_ERR,"GetAnswerToRequest %s %d\n",ENUM_NAMES[code],result);
		memcpy(chReply+1, &result,4);
		*cbReplyBytes=1+4;
		break;

	case ENUM_KMotionLock:	
		result = KMotionDLL.KMotionLock(board);
		syslog(LOG_ERR,"GetAnswerToRequest %s %s\n",ENUM_NAMES[code],LOCK_CODES[result]);
		memcpy(chReply+1, &result,4);
		*cbReplyBytes=1+4;
		break;

	case ENUM_USBLocation:	
		result = KMotionDLL.USBLocation(board);
		syslog(LOG_ERR,"GetAnswerToRequest %s \n\tresult=%d \n",ENUM_NAMES[code],result);
		memcpy(chReply+1, &result,4);
		*cbReplyBytes=1+4;
		break;

	case ENUM_KMotionLockRecovery:	
		result = KMotionDLL.KMotionLockRecovery(board);
		syslog(LOG_ERR,"GetAnswerToRequest %s \n\tresult=%d \n",ENUM_NAMES[code],result);
		memcpy(chReply+1, &result,4);
		*cbReplyBytes=1+4;
		break;

	case ENUM_ReleaseToken:	
		syslog(LOG_ERR,"GetAnswerToRequest %s\n",ENUM_NAMES[code]);
		KMotionDLL.ReleaseToken(board);
		memcpy(chReply+1, &result,4);
		*cbReplyBytes=1+4;
		break;

	case ENUM_ServiceConsole:	
		result = KMotionDLL.ServiceConsole(board);
		syslog(LOG_ERR,"GetAnswerToRequest %s \n\tresult=%d \n",ENUM_NAMES[code],result);
		memcpy(chReply+1, &result,4);
		*cbReplyBytes=1+4;
		break;
	case ENUM_SetConsole:
		// remember which pipe is associated with the console handler for the board
		ConsolePipeHandle[board] = hPipe;
		result = KMotionDLL.SetConsoleCallback(board,ConsoleHandler);
		syslog(LOG_ERR,"GetAnswerToRequest %s \n\tresult=%d \n\tboard=%d \n\thandle=%d \n",ENUM_NAMES[code],result,board,hPipe);
		memcpy(chReply+1, &result,4);
		*cbReplyBytes=1+4;
		break;

	default: MyErrExitThread("Bad Request Code",hPipe);
	}

	// before we send the answer back check if this pipe
	// is the message handler for the board and there are
	// messages in the queue 

	if (code!=ENUM_ListLocations && hPipe == ConsolePipeHandle[board])
	{
		int nSent=0;

		//while (!ConsoleList[board].IsEmpty() && nSent++<10)
		while (!ConsoleList[board].empty() && nSent++<10)
		{
			int cbReplyBytes, cbBytesRead, cbWritten;
			unsigned char Reply;
			//CString s = ConsoleList[board].RemoveHead();
			char *str = ConsoleList[board].front();
			ConsoleList[board].pop_front();

			char * s = new char [strlen(str)+2];
			s[0] = DEST_CONSOLE;
			strcpy(s+1, str);
			delete []str;

			cbReplyBytes = strlen(s)+1;  // + Term Null, DEST code already accounted for in s

			// Write the message to the pipe. 
			cbWritten = send(hPipe, s, cbReplyBytes, 0);

			if (cbWritten < 0 || cbReplyBytes != cbWritten) {
				if (cbWritten < 0){
					logError("send");
				} else{
					syslog(LOG_ERR,"GetAnswerToRequest %d bytes written != %d bytes sent\n",cbWritten,cbReplyBytes);
				}
				break;
			}

		   // Read back 1 byte ack 0xAA from Console Handler
			cbBytesRead = recv(hPipe, &Reply, 1, 0);
			if (cbBytesRead < 0 || cbBytesRead != 1 || Reply != 0xAA) {
				if (cbWritten < 0){
					logError("send");
				} else if(Reply != 0xAA){
					syslog(LOG_ERR,"GetAnswerToRequest received ACK not 0xAA as expected: %d",Reply);
				} else {
					syslog(LOG_ERR,"GetAnswerToRequest received ACK length != 1: %d",cbBytesRead);
				}

				break;
			}
		}
	}

	// check if we have an error message to send back

	if (code!=ENUM_ListLocations && !(KMotionDLL.GetErrMsg(board)[0]==0))
	{
		int cbReplyBytes, cbBytesRead, cbWritten;
		unsigned char Reply;
		char s[MAX_LINE+1];
		s[0] = DEST_ERRMSG;
		strcpy(s+1,KMotionDLL.GetErrMsg(board));

		cbReplyBytes = strlen(s)+1;// + Term Null, DEST code already accounted for in s
		// Write the message to the pipe. 
		cbWritten = send(hPipe, s, cbReplyBytes, 0);

		if (cbWritten < 0) {
			syslog(LOG_ERR,"GetAnswerToRequest Send to DEST_ERRMSG(%d) failed: %s",s[0], s+1);
			logError("send");
		}

		if (cbWritten >=0 && cbReplyBytes == cbWritten)
		{
		   // Read back 1 byte ack 0xAA from Console Handler
			cbBytesRead = recv(hPipe, &Reply, 1, 0);
			if (cbBytesRead < 0) {
				syslog(LOG_ERR,"GetAnswerToRequest Failed to receive Ack(0xAA) on message: %s", s+1);
				logError("recv");
			}

		}

		KMotionDLL.ClearErrMsg(board);
	}
}


int ConsoleHandler(int board, const char *buf)
{
	// check if there is a console handler for this board

	//remeber to delete to avoid memory leak when popped
	char *buf2 = new char [strlen(buf) + 1];
	strcpy(buf2,buf);

	if (ConsolePipeHandle[board])
	{
		// there is, add the message to the list
		syslog(LOG_ERR,"Console message: %s",buf2);
		ConsoleList[board].push_back(buf2);
	}
	return 0;
}
