#include <stdio.h>
#include <upnp.h>

int main(int argc, char *argv[])
{
	int rc = 0;

	rc = UpnpInit (NULL, 0);

	if ( UPNP_E_SUCCESS == rc ) 
	{
		const char* ip_address = UpnpGetServerIpAddress();
		unsigned short port    = UpnpGetServerPort();
		printf ("UPnP Initialized OK ip=%s, port=%d\n", (ip_address ? ip_address : "UNKNOWN"), port);
	}
	else
	{
		printf ("** ERROR UpnpInit(): %d\n", rc);
	}
	

	(void) UpnpFinish();
	printf("\n");
	return 0;
}
