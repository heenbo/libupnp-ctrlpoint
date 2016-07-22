#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <upnp.h>
#include <ixml.h>
#include <upnptools.h>
#include "main.h"
#include "ctrlpoint.h"

#if __DEBUG__
#define DEBUG(format,...) printf(">>FILE: %s, LINE: %d: "format"\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG(format,...)
#endif

#if __DEBUG_PRINTF__
#define DEBUG_PRINTF(format,...) printf(format, ##__VA_ARGS__)
#else
#define DEBUG_PRINTF(format,...)
#endif

const char DeviceType[] = "urn:schemas-upnp-org:device:MediaRenderer:1";
const char *ServiceName[] = { "AVTransport", "ConnectionManager", "RenderingControl"};
/*! Service types for tv services. */
const char *ServiceType[] = 
{
	"urn:schemas-upnp-org:service:AVTransport:1",
	"urn:schemas-upnp-org:service:ConnectionManager:1",
	"urn:schemas-upnp-org:service:RenderingControl:1",
};

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
				struct Upnp_Discovery *d_event = (struct Upnp_Discovery *)Event;
				IXML_Document *DescDoc = NULL;
				int ret;

				if (d_event->ErrCode != UPNP_E_SUCCESS)
				{
					DEBUG("Error in Discovery Callback -- %d\n", d_event->ErrCode);
				}
				ret = UpnpDownloadXmlDoc(d_event->Location, &DescDoc);
				if (ret != UPNP_E_SUCCESS) 
				{
					DEBUG("Error obtaining device description from %s -- "
							"error = %d\n", d_event->Location, ret);
				}
				else
				{
					CtrlPointAddDevice(DescDoc, d_event->Location, d_event->Expires);
				}
				if (DescDoc)
				{
					ixmlDocument_free(DescDoc);
				}
#if __DEBUG__
				CtrlPointPrintList();
#endif
			}
			break;
		case UPNP_DISCOVERY_SEARCH_TIMEOUT:
			{
				DEBUG("search_timeout\n");
			/* Nothing to do here... */
			}
			break;
		case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE: 
			{
				DEBUG("bye bye!!!\n");
				struct Upnp_Discovery *d_event = (struct Upnp_Discovery *)Event;
				if (d_event->ErrCode != UPNP_E_SUCCESS)
				{
					DEBUG("Error in Discovery ByeBye Callback -- %d\n",
							d_event->ErrCode);
				}
				DEBUG("Received ByeBye for Device: %s\n", d_event->DeviceId);
				CtrlPointRemoveDevice(d_event->DeviceId);
				DEBUG("After byebye:\n");
#if __DEBUG__
				CtrlPointPrintList();
#endif
			}
			break;
			/* SOAP Stuff */
		case UPNP_CONTROL_ACTION_COMPLETE: 
			{
				struct Upnp_Action_Complete *a_event = (struct Upnp_Action_Complete *)Event;
//				if (a_event->ErrCode != UPNP_E_SUCCESS)
				{
					DEBUG_PRINTF("Error in  Action Complete Callback -- %d\n",
							a_event->ErrCode);
				}
				/* No need for any processing here, just print out results.
				 * Service state table updates are handled by events. */
				DEBUG("action complete\n");
			}
			break;
		case UPNP_CONTROL_GET_VAR_COMPLETE: 
			{
				DEBUG("get var complete\n");
			}
			break;
			/* GENA Stuff */
		case UPNP_EVENT_RECEIVED: 
			{
				DEBUG("received\n");
				struct Upnp_Event *e_event = (struct Upnp_Event *)Event;
				char *xmlbuff = NULL;

				DEBUG("SID         =  %s\n", e_event->Sid);
				DEBUG("EventKey    =  %d\n",	e_event->EventKey);
				xmlbuff = ixmlPrintNode((IXML_Node *)e_event->ChangedVariables);
				DEBUG("ChangedVars =  %s\n", xmlbuff);
				ixmlFreeDOMString(xmlbuff);
				xmlbuff = NULL;				
#if 0
				struct Upnp_Event *e_event = (struct Upnp_Event *)Event;

				DEBUG("Sid:%s EventKey:%d ", e_event->Sid, e_event->EventKey,
                                                e_event->ChangedVariables);
				CtrlPointHandleEvent( e_event->Sid, e_event->EventKey,
						e_event->ChangedVariables);	
#endif
			}
			break;
		case UPNP_EVENT_SUBSCRIBE_COMPLETE:
		case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
		case UPNP_EVENT_RENEWAL_COMPLETE: 
			{
				DEBUG("sub complete, unsub complete, renewal complete\n");
				struct Upnp_Event_Subscribe *es_event = (struct Upnp_Event_Subscribe *)Event;

				if (es_event->ErrCode != UPNP_E_SUCCESS) {
					DEBUG_PRINTF("Error in Event Subscribe Callback -- %d\n",
							es_event->ErrCode);
				}
				else
				{
					CtrlPointHandleSubscribeUpdate( es_event->PublisherUrl,
							es_event->Sid, es_event->TimeOut);
				}
			}
			break;
		case UPNP_EVENT_AUTORENEWAL_FAILED:
		case UPNP_EVENT_SUBSCRIPTION_EXPIRED: 
			{
				struct Upnp_Event_Subscribe *es_event = (struct Upnp_Event_Subscribe *)Event;
				int TimeOut = default_timeout;
				Upnp_SID newSID;
				int ret;
				ret = UpnpSubscribe(ctrlpt_handle, es_event->PublisherUrl, &TimeOut, newSID);
				if (ret == UPNP_E_SUCCESS)
				{
					DEBUG("Subscribed to EventURL with SID=%s\n", newSID);
					CtrlPointHandleSubscribeUpdate(es_event->PublisherUrl, newSID, TimeOut);
				}
				else
				{
					DEBUG("Error Subscribing to EventURL -- %d\n", ret);
				}
				DEBUG("autorenewal failed, sub expired\n");
			}
			break;
			/* ignore these cases, since this is not a device */
		case UPNP_EVENT_SUBSCRIPTION_REQUEST:
		case UPNP_CONTROL_GET_VAR_REQUEST:
		case UPNP_CONTROL_ACTION_REQUEST:
			{
				DEBUG("event sub req, control get var req, control action req\n");
			}
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
 * CtrlPointDeleteNode
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
				    DEBUG("Unsubscribed from %s EventURL with SID=%s\n",
				     ServiceName[service],
				     node->device.Service[service].SID);
			} else 
			{
				    DEBUG("Error unsubscribing to %s EventURL -- %d\n",
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

int CtrlPointStop(void)
{
	CtrlPointTimerLoopRun = 0;
	CtrlPointRemoveAll();
	UpnpUnRegisterClient( ctrlpt_handle );
	UpnpFinish();
//	SampleUtil_Finish();

	return SUCCESS;
}

void *CtrlPointCommandLoop(void *args)
{
	char cmdline[100];

	while (1) 
	{
		printf("\n>> ");
		fflush(stdout);
		fgets(cmdline, 100, stdin);
		CtrlPointProcessCommand(cmdline);
	}

	return NULL;
}

int CtrlPointProcessCommand(char *cmdline)
{
	char cmd[100];
	int arg1 = 0;
	sscanf(cmdline, "%s %d", cmd, &arg1);	
	
	if(0 == strcmp(cmd, "play"))
	{
		char * action = "Play";
		const char * argm[] = {"InstanceID", "Speed"};
		char * argm_val[] = {"0", "1"}; 
		CtrlPointSendAction(SERVICE_AVTRANSPORT, arg1,  action, argm, argm_val, 2);
	}
	else if(0 == strcmp(cmd, "next"))
	{
		char * action = "Next";
		const char * argm[] = {"InstanceID"};
		char * argm_val[] = {"0"}; 
		CtrlPointSendAction(SERVICE_AVTRANSPORT, arg1,  action, argm, argm_val, 1);
	}
	else if(0 == strcmp(cmd, "get"))
        {
                char * action = "GetCurrentConnectionInfo";
                const char * argm[] = {"ConnectionID"};
                char * argm_val[] = {"0"};
                CtrlPointSendAction(SERVICE_CONNECTIONMANAGER, arg1,  action, argm, argm_val, 1);
        }
	else if(0 == strcmp(cmd, "pause"))
	{
		char * action = "Pause";
		const char * argm[] = {"InstanceID"};
		char * argm_val[] = {"0"}; 
		CtrlPointSendAction(SERVICE_AVTRANSPORT, arg1,  action, argm, argm_val, 1);
	}
	else if(0 == strcmp(cmd, "stop"))
	{
		char * action = "Stop";
		const char * argm[] = {"InstanceID"};
		char * argm_val[] = {"0"}; 
		CtrlPointSendAction(SERVICE_AVTRANSPORT, arg1,  action, argm, argm_val, 1);
	}
	else if(0 == strcmp(cmd, "send"))
	{
		char * action = "SetAVTransportURI";
		const char * argm[] = {"InstanceID", "CurrentURI", "CurrentURIMetaData"};
		char * argm_val[] = {"0", "http://o9o6wy2tb.bkt.clouddn.com/need_for_speed.mp4", "Metadata"};

		CtrlPointSendAction(SERVICE_AVTRANSPORT, arg1,  action, argm, argm_val, 3);
	}
	else if(0 == strcmp(cmd, "ls"))
	{
		CtrlPointPrintList();
	}
	else
	{
		DEBUG("command input error!\n");
	}
		
	return SUCCESS;
}

/********************************************************************************
 * CtrlPointSendAction
 * Description: 
 *       Send an Action request to the specified service of a device.
 * Parameters:
 *   service -- The service
 *   devnum -- The number of the device (order in the list, starting with 1)
 *   actionname -- The name of the action.
 *   param_name -- An array of parameter names
 *   param_val -- The corresponding parameter values
 *   param_count -- The number of parameters
 ********************************************************************************/
int CtrlPointSendAction( int service, int devnum, const char *actionname, 
				const char **param_name, char **param_val, int param_count)
{
	struct DeviceNode *devnode;
	IXML_Document *actionNode = NULL;
	int rc = SUCCESS;
	int param;

	pthread_mutex_lock(&DeviceListMutex);

	rc = CtrlPointGetDevice(devnum, &devnode);
	DEBUG("rc: %d \n", rc);
	if (SUCCESS == rc) 
	{
		if (0 == param_count) 
		{
			actionNode = UpnpMakeAction(actionname, ServiceType[service], 0, NULL);
		}
		else 
		{
			for (param = 0; param < param_count; param++)
			{
				if (UpnpAddToAction(&actionNode, actionname,
						ServiceType[service], param_name[param],
				     		param_val[param]) != UPNP_E_SUCCESS) 
				{
					DEBUG("ERROR: CtrlPointSendAction: Trying to add action param\n");
				}
			}
		}
		DEBUG("ServiceType[service].ControlURL: %s\n", devnode->device.Service[service].ControlURL);
		DEBUG("param_count: %d ServiceType[service]: %s\n", param_count, ServiceType[service]);
		char *xmlbuff = NULL;
		xmlbuff = ixmlPrintNode((IXML_Node *)actionNode);
		DEBUG("xmlbuff =  %s\n", xmlbuff);
		ixmlFreeDOMString(xmlbuff);
		xmlbuff = NULL;


		rc = UpnpSendActionAsync(ctrlpt_handle, devnode->device.Service[service].ControlURL,
						ServiceType[service], NULL, actionNode,
					 	CtrlPointCallbackEventHandler, NULL);

		if (rc != UPNP_E_SUCCESS)
		{
			DEBUG("Error in UpnpSendActionAsync -- %d\n", rc);
			rc = ERROR;
		}
	}
	pthread_mutex_unlock(&DeviceListMutex);
	if (actionNode)
	{
		ixmlDocument_free(actionNode);
	}
	
	return rc;
}

/********************************************************************************
 * CtrlPointGetDevice
 * Description: 
 *       Given a list number, returns the pointer to the device
 *       node at that position in the global device list.  Note
 *       that this function is not thread safe.  It must be called 
 *       from a function that has locked the global device list.
 * Parameters:
 *   devnum -- The number of the device (order in the list, starting with 1)
 *   devnode -- The output device node pointer
 ********************************************************************************/
int CtrlPointGetDevice(int devnum, struct DeviceNode **devnode)
{
	int count = devnum;
	struct DeviceNode *tmpdevnode = NULL;

	if (count)
	{
		tmpdevnode = GlobalDeviceList;
	}
	while (--count && tmpdevnode)
	{
		tmpdevnode = tmpdevnode->next;
	}
	if (!tmpdevnode)
	{
		DEBUG("Error finding Device number -- %d\n", devnum);
		return ERROR;
	}
	*devnode = tmpdevnode;

	return SUCCESS;
}

/********************************************************************************
 * CtrlPointPrintList
 * Description: 
 *       Print the universal device names for each device in the global device list
 * Parameters:
 *   None
 ********************************************************************************/
int CtrlPointPrintList(void)
{
	struct DeviceNode *tmpdevnode;
#if __DEBUG_PRINTF__
	int i = 0;
#endif

	pthread_mutex_lock(&DeviceListMutex);
	DEBUG_PRINTF("CtrlPointPrintList:\n");
	tmpdevnode = GlobalDeviceList;
	while (tmpdevnode)
	{
		DEBUG_PRINTF("%3d -- %s\t\tUDN:%s\n", ++i, tmpdevnode->device.UDN, tmpdevnode->device.FriendlyName);
#if 0 //"ls"
		DEBUG_PRINTF("      |-- DescDocURL:%s\n", tmpdevnode->device.DescDocURL);
		DEBUG_PRINTF("      |-- PresURL:%s\n", tmpdevnode->device.PresURL);
		DEBUG_PRINTF("      |-- 0:ServiceId:%s\n", tmpdevnode->device.Service[0].ServiceId);
		DEBUG_PRINTF("        |-- 0:ServiceType:%s\n", tmpdevnode->device.Service[0].ServiceType);
		DEBUG_PRINTF("        |-- 0:EventURL:%s\n", tmpdevnode->device.Service[0].EventURL);
		DEBUG_PRINTF("        |-- 0:ControlURL:%s\n", tmpdevnode->device.Service[0].ControlURL);
		DEBUG_PRINTF("        |-- 0:SID:%s\n", tmpdevnode->device.Service[0].SID);
		DEBUG_PRINTF("      |-- 1:ServiceId:%s\n", tmpdevnode->device.Service[1].ServiceId);
		DEBUG_PRINTF("        |-- 1:ServiceType:%s\n", tmpdevnode->device.Service[1].ServiceType);
		DEBUG_PRINTF("        |-- 1:EventURL:%s\n", tmpdevnode->device.Service[1].EventURL);
		DEBUG_PRINTF("        |-- 1:ControlURL:%s\n", tmpdevnode->device.Service[1].ControlURL);
		DEBUG_PRINTF("        |-- 1:SID:%s\n", tmpdevnode->device.Service[1].SID);
		DEBUG_PRINTF("      |-- 2:ServiceId:%s\n", tmpdevnode->device.Service[2].ServiceId);
		DEBUG_PRINTF("        |-- 2:ServiceType:%s\n", tmpdevnode->device.Service[2].ServiceType);
		DEBUG_PRINTF("        |-- 2:EventURL:%s\n", tmpdevnode->device.Service[2].EventURL);
		DEBUG_PRINTF("        |-- 2:ControlURL:%s\n", tmpdevnode->device.Service[2].ControlURL);
		DEBUG_PRINTF("        |-- 2:SID:%s\n", tmpdevnode->device.Service[2].SID);
#endif
		tmpdevnode = tmpdevnode->next;
	}
	DEBUG_PRINTF("\n");
	pthread_mutex_unlock(&DeviceListMutex);

	return SUCCESS;
}

/********************************************************************************
 * CtrlPointAddDevice
 * Description: 
 *       If the device is not already included in the global device list,
 *       add it.  Otherwise, update its advertisement expiration timeout.
 * Parameters:
 *   DescDoc -- The description document for the device
 *   location -- The location of the description document URL
 *   expires -- The expiration time for this advertisement
 ********************************************************************************/
void CtrlPointAddDevice( IXML_Document *DescDoc, const char *location, int expires)
{

	char *deviceType = NULL;
	char *friendlyName = NULL;
	char presURL[200];
	char *baseURL = NULL;
	char *relURL = NULL;
	char *UDN = NULL;
	char *serviceId[SERVICE_SERVCOUNT] = { NULL, NULL, NULL };
	char *eventURL[SERVICE_SERVCOUNT] = { NULL, NULL, NULL };
	char *controlURL[SERVICE_SERVCOUNT] = { NULL, NULL, NULL };
	Upnp_SID eventSID[SERVICE_SERVCOUNT];
	int TimeOut[SERVICE_SERVCOUNT] = { default_timeout, default_timeout, default_timeout, };
	struct DeviceNode *deviceNode;
	struct DeviceNode *tmpdevnode;
	int ret = 1;
	int found = 0;
	int service;
	int var;

	pthread_mutex_lock(&DeviceListMutex);
	/* Read key elements from description document */
	UDN = SampleUtil_GetFirstDocumentItem(DescDoc, "UDN");
	deviceType = SampleUtil_GetFirstDocumentItem(DescDoc, "deviceType");
	friendlyName = SampleUtil_GetFirstDocumentItem(DescDoc, "friendlyName");
	baseURL = SampleUtil_GetFirstDocumentItem(DescDoc, "URLBase");
	relURL = SampleUtil_GetFirstDocumentItem(DescDoc, "presentationURL");

	ret = UpnpResolveURL((baseURL ? baseURL : location), relURL, presURL);
	if (UPNP_E_SUCCESS != ret)
	{
		DEBUG("Error generating presURL from %s + %s\n", baseURL, relURL);
	}

	if (strcmp(deviceType, DeviceType) == 0)
	{
		DEBUG("Found device\n");
		/* Check if this device is already in the list */
		tmpdevnode = GlobalDeviceList;
		while (tmpdevnode)
		{
			if (strcmp(tmpdevnode->device.UDN, UDN) == 0)
			{
				found = 1;
				break;
			}
			tmpdevnode = tmpdevnode->next;
		}

		if (found)
		{
			/* The device is already there, so just update  */
			/* the advertisement timeout field */
			tmpdevnode->device.AdvrTimeOut = expires;
		}
		else	
		{
			for (service = 0; service < SERVICE_SERVCOUNT; service++)
			{
				if (SampleUtil_FindAndParseService (DescDoc, location,
						ServiceType[service], &serviceId[service],
						&eventURL[service], &controlURL[service]))
				{
					DEBUG("Subscribing to EventURL %s...\n", eventURL[service]);
					ret = UpnpSubscribe(ctrlpt_handle, eventURL[service],
							&TimeOut[service], eventSID[service]);
					if (ret == UPNP_E_SUCCESS)
					{
						DEBUG("Subscribed to EventURL with SID=%s\n",
								eventSID[service]);
					}
					else
					{
						DEBUG("Error Subscribing to EventURL -- %d\n", ret);
						strcpy(eventSID[service], "");
					}
				}
				else
				{
					DEBUG("Error: Could not find Service: %s\n",
						 ServiceType[service]);
				}
			}
			/* Create a new device node */
			deviceNode = (struct DeviceNode *) malloc(sizeof(struct DeviceNode));
			strcpy(deviceNode->device.UDN, UDN);
			strcpy(deviceNode->device.DescDocURL, location);
			strcpy(deviceNode->device.FriendlyName, friendlyName);
			strcpy(deviceNode->device.PresURL, presURL);
			deviceNode->device.AdvrTimeOut = expires;
			for (service = 0; service < SERVICE_SERVCOUNT; service++)
			{
				if (serviceId[service] == NULL)
				{
					/* not found */
					continue;
				}
				strcpy(deviceNode->device.Service[service].ServiceId,
						serviceId[service]);
				strcpy(deviceNode->device.Service[service].ServiceType,
						ServiceType[service]);
				strcpy(deviceNode->device.Service[service].ControlURL,
						controlURL[service]);
				strcpy(deviceNode->device.Service[service].EventURL, 
						eventURL[service]);
				strcpy(deviceNode->device.Service[service].SID,
						eventSID[service]);
				for (var = 0; var < VarCount[service]; var++)
				{
					deviceNode->device.Service[service].VariableStrVal[var] =
						(char *)malloc(MAX_VAL_LEN);
					strcpy(deviceNode->device.Service[service].VariableStrVal[var], 
							"");
				}
			}
			deviceNode->next = NULL;
			/* Insert the new device node in the list */
			if ((tmpdevnode = GlobalDeviceList))
			{
				while (tmpdevnode)
				{
					if (tmpdevnode->next)
					{
						tmpdevnode = tmpdevnode->next;
					}
					else
					{
						tmpdevnode->next = deviceNode;
						break;
					}
				}
			}
			else
			{
				GlobalDeviceList = deviceNode;
			}
			/*Notify New Device Added */
//			SampleUtil_StateUpdate(NULL, NULL, deviceNode->device.UDN, DEVICE_ADDED);
		}
	}
	pthread_mutex_unlock(&DeviceListMutex);

	if (deviceType)
		free(deviceType);
	if (friendlyName)
		free(friendlyName);
	if (UDN)
		free(UDN);
	if (baseURL)
		free(baseURL);
	if (relURL)
		free(relURL);
	for (service = 0; service < SERVICE_SERVCOUNT; service++)
	{
		if (serviceId[service])
			free(serviceId[service]);
		if (controlURL[service])
			free(controlURL[service]);
		if (eventURL[service])
			free(eventURL[service]);
	}
}

char *SampleUtil_GetFirstDocumentItem(IXML_Document *doc, const char *item)
{
	IXML_NodeList *nodeList = NULL;
	IXML_Node *textNode = NULL;
	IXML_Node *tmpNode = NULL;
	char *ret = NULL;

	nodeList = ixmlDocument_getElementsByTagName(doc, (char *)item);
	if (nodeList)
	{
		tmpNode = ixmlNodeList_item(nodeList, 0);
		if (tmpNode)
		{
			textNode = ixmlNode_getFirstChild(tmpNode);
			if (!textNode)
			{
				DEBUG("%s(%d): (BUG) ixmlNode_getFirstChild(tmpNode) returned NULL\n",
						__FILE__, __LINE__); 
				ret = strdup("");
				goto epilogue;
			}
			ret = strdup(ixmlNode_getNodeValue(textNode));
			if (!ret)
			{
				DEBUG("%s(%d): ixmlNode_getNodeValue returned NULL\n",
						__FILE__, __LINE__); 
				ret = strdup("");
			}
		} 
		else
		{
			DEBUG("%s(%d): ixmlNodeList_item(nodeList, 0) returned NULL\n",
					__FILE__, __LINE__);
		}
	} else
		DEBUG("%s(%d): Error finding %s in XML Node\n",
			__FILE__, __LINE__, item);

epilogue:
	if (nodeList)
	{
		ixmlNodeList_free(nodeList);
	}
	return ret;
}

int SampleUtil_FindAndParseService(IXML_Document *DescDoc, const char *location,
	const char *serviceType, char **serviceId, char **eventURL, char **controlURL)
{
	unsigned int i;
	unsigned long length;
	int found = 0;
	int ret;
	char *tempServiceType = NULL;
	char *baseURL = NULL;
	const char *base = NULL;
	char *relcontrolURL = NULL;
	char *releventURL = NULL;
	IXML_NodeList *serviceList = NULL;
	IXML_Element *service = NULL;

	baseURL = SampleUtil_GetFirstDocumentItem(DescDoc, "URLBase");
	if (baseURL)
	{
		base = baseURL;
	}
	else
	{
		base = location;
	}
	serviceList = SampleUtil_GetFirstServiceList(DescDoc);
	length = ixmlNodeList_length(serviceList);
	for (i = 0; i < length; i++)
	{
		service = (IXML_Element *)ixmlNodeList_item(serviceList, i);
		tempServiceType = SampleUtil_GetFirstElementItem((IXML_Element *)service,"serviceType");
		if (tempServiceType && strcmp(tempServiceType, serviceType) == 0)
		{
			DEBUG("Found service: %s\n", serviceType);
			*serviceId = SampleUtil_GetFirstElementItem(service, "serviceId");
			DEBUG("serviceId: %s\n", *serviceId);
			relcontrolURL = SampleUtil_GetFirstElementItem(service, "controlURL");
			releventURL = SampleUtil_GetFirstElementItem(service, "eventSubURL");
			*controlURL = malloc(strlen(base) + strlen(relcontrolURL) + 1);
			if (*controlURL)
			{
				ret = UpnpResolveURL(base, relcontrolURL, *controlURL);
				if (ret != UPNP_E_SUCCESS)
				{
					DEBUG("Error generating controlURL from %s + %s\n",
							base, relcontrolURL);
				}
			}
			*eventURL = malloc(strlen(base) + strlen(releventURL) + 1);
			if (*eventURL)
			{
				ret = UpnpResolveURL(base, releventURL, *eventURL);
				if (ret != UPNP_E_SUCCESS)
				{
					DEBUG("Error generating eventURL from %s + %s\n",
							base, releventURL);
				}
			}
			free(relcontrolURL);
			free(releventURL);
			relcontrolURL = NULL;
			releventURL = NULL;
			found = 1;
			break;
		}
		free(tempServiceType);
		tempServiceType = NULL;
	}
	free(tempServiceType);
	tempServiceType = NULL;
	if (serviceList)
		ixmlNodeList_free(serviceList);
	serviceList = NULL;
	free(baseURL);

	return found;
}

IXML_NodeList *SampleUtil_GetFirstServiceList(IXML_Document *doc)
{
	IXML_NodeList *ServiceList = NULL;
	IXML_NodeList *servlistnodelist = NULL;
	IXML_Node *servlistnode = NULL;

	servlistnodelist = ixmlDocument_getElementsByTagName(doc, "serviceList");
	if (servlistnodelist && ixmlNodeList_length(servlistnodelist))
	{
		/* we only care about the first service list, from the root device */
		servlistnode = ixmlNodeList_item(servlistnodelist, 0);
		/* create as list of DOM nodes */
		ServiceList = ixmlElement_getElementsByTagName((IXML_Element *)servlistnode, "service");
	}
	if (servlistnodelist)
	{
		ixmlNodeList_free(servlistnodelist);
	}
	return ServiceList;
}

char *SampleUtil_GetFirstElementItem(IXML_Element *element, const char *item)
{
	IXML_NodeList *nodeList = NULL;
	IXML_Node *textNode = NULL;
	IXML_Node *tmpNode = NULL;
	char *ret = NULL;

	nodeList = ixmlElement_getElementsByTagName(element, (char *)item);
	if (nodeList == NULL) 
	{
		DEBUG("%s(%d): Error finding %s in XML Node\n", __FILE__, __LINE__, item);
		return NULL;
	}
	tmpNode = ixmlNodeList_item(nodeList, 0);
	if (!tmpNode)
	{
		DEBUG("%s(%d): Error finding %s value in XML Node\n", __FILE__, __LINE__, item);
		ixmlNodeList_free(nodeList);
		return NULL;
	}
	textNode = ixmlNode_getFirstChild(tmpNode);
	ret = strdup(ixmlNode_getNodeValue(textNode));
	if (!ret)
	{
		DEBUG("%s(%d): Error allocating memory for %s in XML Node\n", __FILE__, __LINE__, item);
		ixmlNodeList_free(nodeList);
		return NULL;
	}
	ixmlNodeList_free(nodeList);

	return ret;
}

/********************************************************************************
 * CtrlPointRemoveDevice
 * Description: 
 *	Remove a device from the global device list.
 * Parameters:
 *	UDN -- The Unique Device Name for the device to remove
 ********************************************************************************/
int CtrlPointRemoveDevice(const char *UDN)
{
	struct DeviceNode *curdevnode;
	struct DeviceNode *prevdevnode;

	pthread_mutex_lock(&DeviceListMutex);

	curdevnode = GlobalDeviceList;
	if (!curdevnode)
	{
		DEBUG( "WARNING: CtrlPointRemoveDevice: Device list empty\n");
	}
	else
	{
		if (0 == strcmp(curdevnode->device.UDN, UDN))
		{
			GlobalDeviceList = curdevnode->next;
			CtrlPointDeleteNode(curdevnode);
		}
		else 
		{
			prevdevnode = curdevnode;
			curdevnode = curdevnode->next;
			while (curdevnode)
			{
				if (strcmp(curdevnode->device.UDN, UDN) == 0)
				{
					prevdevnode->next = curdevnode->next;
					CtrlPointDeleteNode(curdevnode);
					break;
				}
				prevdevnode = curdevnode;
				curdevnode = curdevnode->next;
			}
		}
	}
	pthread_mutex_unlock(&DeviceListMutex);

	return SUCCESS;
}

/********************************************************************************
 * CtrlPointHandleSubscribeUpdate
 * Description: 
 *       Handle a UPnP subscription update that was received.  Find the 
 *       service the update belongs to, and update its subscription
 *       timeout.
 * Parameters:
 *   eventURL -- The event URL for the subscription
 *   sid -- The subscription id for the subscription
 *   timeout  -- The new timeout for the subscription
 ********************************************************************************/
void CtrlPointHandleSubscribeUpdate( const char *eventURL, const Upnp_SID sid, int timeout)
{
	struct DeviceNode *tmpdevnode;
	int service;

	pthread_mutex_lock(&DeviceListMutex);

	tmpdevnode = GlobalDeviceList;
	while (tmpdevnode)
	{
		for (service = 0; service < SERVICE_SERVCOUNT; service++)
		{
			if (strcmp (tmpdevnode->device.Service[service].EventURL, eventURL) == 0)
			{
				    DEBUG("Received Tv %s Event Renewal for eventURL %s\n",
				     ServiceName[service], eventURL);
				strcpy(tmpdevnode->device.Service[service].SID, sid);
				break;
			}
		}
		tmpdevnode = tmpdevnode->next;
	}
	pthread_mutex_unlock(&DeviceListMutex);

	return;
}

/********************************************************************************
 * CtrlPointHandleEvent
 * Description: 
 *       Handle a UPnP event that was received.  Process the event and update
 *       the appropriate service state table.
 * Parameters:
 *   sid -- The subscription id for the event
 *   eventkey -- The eventkey number for the event
 *   changes -- The DOM document representing the changes
 ********************************************************************************/
void CtrlPointHandleEvent( const char *sid, int evntkey, IXML_Document *changes)
{
	struct DeviceNode *tmpdevnode;
	int service;

	pthread_mutex_lock(&DeviceListMutex);
	tmpdevnode = GlobalDeviceList;
	while (tmpdevnode)
	{
		for (service = 0; service < SERVICE_SERVCOUNT; ++service)
		{
			if (strcmp(tmpdevnode->device.Service[service].SID, sid) ==  0)
			{
				DEBUG("Received  %s Event: %d for SID %s\n", ServiceName[service],
						evntkey, sid);
//				StateUpdate( tmpdevnode->device.UDN, service, changes,
//					(char **)&tmpdevnode->device.Service[service].VariableStrVal);
				break;
			}
		}
		tmpdevnode = tmpdevnode->next;
	}
	pthread_mutex_unlock(&DeviceListMutex);
}

void StateUpdate(char *UDN, int Service, IXML_Document *ChangedVariables, char **State)
{
	IXML_NodeList *properties;
	IXML_NodeList *variables;
	IXML_Element *property;
	IXML_Element *variable;
	long unsigned int length;
	long unsigned int length1;
	long unsigned int i;
	int j;
	char *tmpstate = NULL;

	DEBUG("State Update (service %d):\n", Service);
	/* Find all of the e:property tags in the document */
	properties = ixmlDocument_getElementsByTagName(ChangedVariables, "e:property");
	if (properties)
	{
		length = ixmlNodeList_length(properties);
		for (i = 0; i < length; i++)
		{
			/* Loop through each property change found */
			property = (IXML_Element *)ixmlNodeList_item( properties, i);
			/* For each variable name in the state table,
			 * check if this is a corresponding property change */
			for (j = 0; j < VarCount[Service]; j++)
			{
				variables = ixmlElement_getElementsByTagName(
						property, VarName[Service][j]);
				/* If a match is found, extract 
				 * the value, and update the state table */
				if (variables)
				{
					length1 = ixmlNodeList_length(variables);
					if (length1)
					{
						variable = (IXML_Element *)
							ixmlNodeList_item(variables, 0);
						tmpstate = SampleUtil_GetElementValue(variable);
						if (tmpstate)
						{
							strcpy(State[j], tmpstate);
							DEBUG( " Variable Name: %s New Value:'%s'\n",
								VarName[Service][j], State[j]);
						}
						if (tmpstate)
							free(tmpstate);
						tmpstate = NULL;
					}
					ixmlNodeList_free(variables);
					variables = NULL;
				}
			}
		}
		ixmlNodeList_free(properties);
	}
	return;
}

char *SampleUtil_GetElementValue(IXML_Element *element)
{
	IXML_Node *child = ixmlNode_getFirstChild((IXML_Node *)element);
	char *temp = NULL;

	if (child != 0 && ixmlNode_getNodeType(child) == eTEXT_NODE)
		temp = strdup(ixmlNode_getNodeValue(child));

	return temp;
}
