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

const char DeviceType[] = "urn:schemas-upnp-org:device:MediaRenderer:1";
const char *ServiceName[] = { "AVTransport", "ConnectionManager:1", "RenderingControl"};
int default_timeout = 1801;

pthread_mutex_t DeviceListMutex;
UpnpClient_Handle ctrlpt_handle = -1;

int CtrlPointStart(void)
{
	int rc = 0;
	char *ip_address = NULL;
	unsigned short port = 0;
	pthread_t timer_thread;

	pthread_mutex_init(&DeviceListMutex, 0);
	rc = UpnpInit(ip_address, port);
	if (rc != UPNP_E_SUCCESS)
	{
		UpnpFinish();
		return ERROR;
	}
	if (!ip_address) 
	{
		ip_address = UpnpGetServerIpAddress();
	}
	if (!port) 
	{
		port = UpnpGetServerPort();
	}
	DEBUG("ip_address: %s, port: %d\n", ip_address, port);

	rc = UpnpRegisterClient(CtrlPointCallbackEventHandler, &ctrlpt_handle, &ctrlpt_handle);
	if (rc != UPNP_E_SUCCESS) 
	{
		DEBUG("Error registering ctrlpoint: %d\n", rc);
		UpnpFinish();
		return ERROR;
	}
	else
	{
		DEBUG("Success registering ctrlpoint: %d\n", rc);
	}
	CtrlPointRefresh();

	/* start a timer thread */
	pthread_create(&timer_thread, NULL, CtrlPointTimerLoop, NULL);
	pthread_detach(timer_thread);

	return SUCCESS;
}


int CtrlPointCallbackEventHandler(Upnp_EventType EventType, void *Event, void *Cookie)
{
	switch ( EventType )
	{
		case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
		case UPNP_DISCOVERY_SEARCH_RESULT: 
			{
#if 0
				pthread_mutex_lock(&DeviceListMutex);
				printf("\n\n\n\nLINE: %d search result\n", __LINE__);
				struct Upnp_Discovery *d_event = (struct Upnp_Discovery *)Event;
				
				if (d_event->ErrCode != UPNP_E_SUCCESS)
				{
					printf("Error in Discovery Callback -- %d\n", d_event->ErrCode);
				}

				print_Upnp_Discovery_Info(d_event);
				IXML_Document *DescDoc = NULL;
//				UpnpDownloadXmlDoc(d_event->Location, &DescDoc);
				if(UPNP_E_SUCCESS == UpnpDownloadXmlDoc(d_event->Location, &DescDoc))
				{	//add device
						char *UDN = NULL;			
						char *deviceType = NULL;
						char *friendlyName = NULL;

						if(0 == strcmp(ctrlpt_DeviceType, SampleUtil_GetFirstDocumentItem(DescDoc, "deviceType")))
						{
							/* Read key elements from description document */
							UDN = SampleUtil_GetFirstDocumentItem(DescDoc, "UDN");
							deviceType = SampleUtil_GetFirstDocumentItem(DescDoc, "deviceType");
							friendlyName = SampleUtil_GetFirstDocumentItem(DescDoc, "friendlyName");			
							printf("\nUDN\t\t: %s\n", UDN);
							printf("deviceType\t: %s\n", deviceType);
							printf("friendlyName\t: %s\n", friendlyName);
						}	
			}
				if(DescDoc)
				{
					ixmlDocument_free(DescDoc);	
				} //				char * buf = ixmlPrintDocument(DescDoc); //				printf("buf = %s\n", buf);
				pthread_mutex_unlock(&DeviceListMutex);
#endif
				break;
			}
		case UPNP_DISCOVERY_SEARCH_TIMEOUT:
			{
				printf("LINE: %d search_timeout\n", __LINE__);
			/* Nothing to do here... */
			}
			break;
		case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE: 
			{
				printf("LINE: %d bye bye!!!\n", __LINE__);
			}
			/* SOAP Stuff */
		case UPNP_CONTROL_ACTION_COMPLETE: 
			{
				printf("LINE: %d action complete\n", __LINE__);
			}
		case UPNP_CONTROL_GET_VAR_COMPLETE: 
			{
				printf("LINE: %d get var complete\n", __LINE__);
			}
			/* GENA Stuff */
		case UPNP_EVENT_RECEIVED: 
			{
				printf("LINE: %d received\n", __LINE__);
			}
		case UPNP_EVENT_SUBSCRIBE_COMPLETE:
		case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
		case UPNP_EVENT_RENEWAL_COMPLETE: 
			{
				printf("LINE: %d sub complete, unsub complete, renewal complete\n", __LINE__);
			}
		case UPNP_EVENT_AUTORENEWAL_FAILED:
		case UPNP_EVENT_SUBSCRIPTION_EXPIRED: 
			{
				printf("LINE: %d autorenewal failed, sub expired\n", __LINE__);
			}
			/* ignore these cases, since this is not a device */
		case UPNP_EVENT_SUBSCRIPTION_REQUEST:
		case UPNP_CONTROL_GET_VAR_REQUEST:
		case UPNP_CONTROL_ACTION_REQUEST:
				printf("LINE: %d event sub req, control get var req, control action req\n", __LINE__);
			break;	

	}
		return 0;
}

int CtrlPointRefresh(void)
{
#if 0
	int rc;

	TvCtrlPointRemoveAll();
	/* Search for all devices of type tvdevice version 1,
	 * waiting for up to 5 seconds for the response */
	rc = UpnpSearchAsync(ctrlpt_handle, 5, DeviceType, NULL);
	if (UPNP_E_SUCCESS != rc) 
	{
		SampleUtil_Print("Error sending search request%d\n", rc);

		return ERROR;
	}
#endif
	return SUCCESS;
}

void *CtrlPointTimerLoop(void *args)
{
#if 0
	/* how often to verify the timeouts, in seconds */
	int incr = 30;

	while (CtrlPointTimerLoopRun) 
	{
		sleep((unsigned int)incr);
//		TvCtrlPointVerifyTimeouts(incr);
	}

#endif
	return NULL;
}

