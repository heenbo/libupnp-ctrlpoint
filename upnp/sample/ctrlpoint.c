#include <stdio.h>
#include <stdlib.h>
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

const char *VarName[SERVICE_SERVCOUNT][MAXVARS] = 
{
	{
		"TransportStatus", "NextAVTransportURI", "NextAVTransportURIMetaData", "CurrentTrackMetaData"
	},
	{
		"A_ARG_TYPE_ConnectionManager", "SinkProtocolInfo", "A_ARG_TYPE_ConnectionStatus", "A_ARG_TYPE_AVTransportID"
	},
	{
		"GreenVideoGain", "BlueVideoBlackLevel", "VerticalKeystone", "GreenVideoBlackLevel"
	},
};
char VarCount[SERVICE_SERVCOUNT] = 
{ 
	AVTRANSPORT_VARCOUNT, CONNECTIONMANAGER_VARCOUNT, RENDERINGCONTROL_VARCOUNT,
};

//The first node in the global device list, or NULL if empty
struct DeviceNode *GlobalDeviceList = NULL;
int default_timeout = 1801;

pthread_mutex_t DeviceListMutex;
UpnpClient_Handle ctrlpt_handle = -1;
static int CtrlPointTimerLoopRun = 1;

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
	int rc;
	CtrlPointRemoveAll();
	/* Search for all devices of type tvdevice version 1,
	 * waiting for up to 5 seconds for the response */
	rc = UpnpSearchAsync(ctrlpt_handle, 5, DeviceType, NULL);
	if (UPNP_E_SUCCESS != rc) 
	{
		DEBUG("Error sending search request%d\n", rc);
		return ERROR;
	}
	return SUCCESS;
}

int CtrlPointRemoveAll(void)
{
	struct DeviceNode *curdevnode, *next;

	pthread_mutex_lock(&DeviceListMutex);
	curdevnode = GlobalDeviceList;
	GlobalDeviceList = NULL;
	while (curdevnode)
	{
		next = curdevnode->next;
		CtrlPointDeleteNode(curdevnode);
		curdevnode = next;
	}
	pthread_mutex_unlock(&DeviceListMutex);

	return SUCCESS;
}

/********************************************************************************
 * TvCtrlPointDeleteNode
 * Description: 
 *       Delete a device node from the global device list.  Note that this
 *       function is NOT thread safe, and should be called from another
 *       function that has already locked the global device list.
 * Parameters:
 *   node -- The device node
 ********************************************************************************/
int CtrlPointDeleteNode(struct DeviceNode *node)
{
	int rc, service, var;

	if (NULL == node)
	{
		DEBUG("ERROR: CtrlPointDeleteNode: Node is empty\n");
		return ERROR;
	}

	for (service = 0; service < SERVICE_SERVCOUNT; service++)
	{
		//If we have a valid control SID, then unsubscribe 
		if (strcmp(node->device.Service[service].SID, "") != 0)
		{
			rc = UpnpUnSubscribe(ctrlpt_handle, node->device.Service[service].SID);
			if (UPNP_E_SUCCESS == rc) 
			{
				    DEBUG("Unsubscribed from Tv %s EventURL with SID=%s\n",
				     ServiceName[service],
				     node->device.Service[service].SID);
			} else 
			{
				    DEBUG("Error unsubscribing to Tv %s EventURL -- %d\n",
				     ServiceName[service], rc);
			}
		}
		for (var = 0; var < VarCount[service]; var++) 
		{
			if (node->device.Service[service].VariableStrVal[var]) 
			{
				free(node->device.Service[service].VariableStrVal[var]);
			}
		}
	}
	/*Notify New Device Added , do not need*/
//	SampleUtil_StateUpdate(NULL, NULL, node->device.UDN, DEVICE_REMOVED);
	free(node);
	node = NULL;

	return SUCCESS;
}

void *CtrlPointTimerLoop(void *args)
{
	/* how often to verify the timeouts, in seconds */
	int incr = 30;

	while (CtrlPointTimerLoopRun) 
	{
		sleep((unsigned int)incr);
		CtrlPointVerifyTimeouts(incr);
	}

	return NULL;
}

void CtrlPointVerifyTimeouts(int incr)
{
	struct DeviceNode *prevdevnode;
	struct DeviceNode *curdevnode;
	int ret;

	pthread_mutex_lock(&DeviceListMutex);
	prevdevnode = NULL;
	curdevnode = GlobalDeviceList;
	while (curdevnode) 
	{
		curdevnode->device.AdvrTimeOut -= incr;
		if (curdevnode->device.AdvrTimeOut <= 0)
		{
			/* This advertisement has expired, so we should remove the device
			 * from the list */
			if (GlobalDeviceList == curdevnode)
			{
				GlobalDeviceList = curdevnode->next;
			}
			else
			{
				prevdevnode->next = curdevnode->next;
			}
			CtrlPointDeleteNode(curdevnode);
			if (prevdevnode)
			{
				curdevnode = prevdevnode->next;
			}
			else
			{
				curdevnode = GlobalDeviceList;
			}
		}
		else
		{
			if (curdevnode->device.AdvrTimeOut < 2 * incr) 
			{
				/* This advertisement is about to expire, so
				 * send out a search request for this device
				 * UDN to try to renew */
				ret = UpnpSearchAsync(ctrlpt_handle, incr,
						      curdevnode->device.UDN,
						      NULL);
				if (ret != UPNP_E_SUCCESS)
				{
					DEBUG("Error sending search request for Device UDN: %s -- err = %d\n", curdevnode->device.UDN, ret);
				}
			}
			prevdevnode = curdevnode;
			curdevnode = curdevnode->next;
		}
	}

	pthread_mutex_unlock(&DeviceListMutex);
}

