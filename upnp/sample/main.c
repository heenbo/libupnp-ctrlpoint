#include <stdio.h>
#include <string.h>
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
	rc = CtrlPointStart();
	if(rc != SUCCESS)
	{
		
	}
	
	DEBUG("OKOKOK hahahaha\n");

	return 0;
}

