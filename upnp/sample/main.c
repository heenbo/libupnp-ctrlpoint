#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <upnp.h>
#include "main.h"
#include "ctrlpoint.h"

#define __DEBUG__ 1
#if __DEBUG__
#define DEBUG(format,...) printf(">>FILE: %s, LINE: %d: "format"\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG(format,...)
#endif

int main(int argc, char *argv[])
{
	int rc;
	pthread_t cmdloop_thread;
#define SUPPORT_SIG
#ifdef SUPPORT_SIG
	int sig;
	sigset_t sigs_to_catch;
#endif
	rc = CtrlPointStart();
	if(rc != SUCCESS)
	{
		DEBUG("error CtrlPointStart failed!\n");		
	}
	DEBUG("CtrlPointStart success!\n");		

	/* start a command loop thread */
	rc = pthread_create(&cmdloop_thread, NULL, CtrlPointCommandLoop, NULL);
	if (rc !=  0) 
	{
		return UPNP_E_INTERNAL_ERROR;
	}

#ifdef SUPPORT_SIG
	/* Catch Ctrl-C and properly shutdown */
	sigemptyset(&sigs_to_catch);
	sigaddset(&sigs_to_catch, SIGINT);
	DEBUG("sigwait 1\n");
	sigwait(&sigs_to_catch, &sig);
	DEBUG("sigwait 2\n");
	DEBUG("Shutting down on signal %d...\n", sig);
#endif
	rc = CtrlPointStop();

	return 0;
}

