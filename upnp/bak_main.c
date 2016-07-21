#include <stdio.h>
#include <string.h>
#include <upnp.h>
#include <main.h>
#include <pthread.h>

#define SUPPORT_SIG 1

UpnpClient_Handle ctrlpt_handle = -1;
//char *ctrlpt_DeviceType = "ssdp:all";
char *ctrlpt_DeviceType = "urn:schemas-upnp-org:device:MediaRenderer:1";

void print_Upnp_Discovery_Info(struct Upnp_Discovery *d_event);
char *SampleUtil_GetFirstDocumentItem(IXML_Document *doc, const char *item);

pthread_mutex_t DeviceListMutex;

int main(int argc, char *argv[])
{
	int rc = 0;
#ifdef SUPPORT_SIG
	int sig;
	sigset_t sigs_to_catch;
#endif

	pthread_mutex_init(&DeviceListMutex, 0);

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

	rc = UpnpRegisterClient(CtrlPointCallbackEventHandler,&ctrlpt_handle, &ctrlpt_handle);
	if (rc != UPNP_E_SUCCESS) 
	{
		UpnpFinish();
		printf("error LINE:%d\n", __LINE__);	
		return -1;
	}

	rc = UpnpSearchAsync(ctrlpt_handle, 5, ctrlpt_DeviceType, NULL);
	if (UPNP_E_SUCCESS != rc) 
	{
		printf("error LINE:%d\n", __LINE__);	
		return -1;
	}

	
#ifdef SUPPORT_SIG
	printf("wait ... sig\n");
	sigemptyset(&sigs_to_catch);
	sigaddset(&sigs_to_catch, SIGINT);
	sigwait(&sigs_to_catch, &sig);
#endif

	printf("LINE: %d +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n", __LINE__);
	sleep(2);
	printf("LINE: %d +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n", __LINE__);
	UpnpUnRegisterClient(ctrlpt_handle);
	printf("LINE: %d +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n", __LINE__);
	UpnpFinish();
	printf("LINE: %d +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n", __LINE__);
	return 0;
}

int CtrlPointCallbackEventHandler(Upnp_EventType EventType, void *Event, void *Cookie)
{
	switch ( EventType )
	{
		case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
		case UPNP_DISCOVERY_SEARCH_RESULT: 
			{
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

void print_Upnp_Discovery_Info(struct Upnp_Discovery *d_event)
{
	printf("DeviceId\t:%s\n", d_event->DeviceId);
	printf("DeviceType\t:%s\n", d_event->DeviceType);
	printf("ServiceType\t:%s\n", d_event->ServiceType);
	printf("ServiceVer\t:%s\n", d_event->ServiceVer);
	printf("Location\t:%s\n", d_event->Location);
	printf("Os\t\t:%s\n", d_event->Os);
	printf("Date\t\t:%s\n", d_event->Date);
	printf("Ext\t\t:%s\n",  d_event->Ext);
}

char *SampleUtil_GetFirstDocumentItem(IXML_Document *doc, const char *item)
{
	IXML_NodeList *nodeList = NULL;
	IXML_Node *textNode = NULL;
	IXML_Node *tmpNode = NULL;
	char *ret = NULL;

	nodeList = ixmlDocument_getElementsByTagName(doc, (char *)item);
	if (nodeList) {
		tmpNode = ixmlNodeList_item(nodeList, 0);
		if (tmpNode) {
			textNode = ixmlNode_getFirstChild(tmpNode);
			if (!textNode) {
				ret = strdup("");
				goto epilogue;
			}
			ret = strdup(ixmlNode_getNodeValue(textNode));
			if (!ret) {
				ret = strdup("");
			}
		} else
		{
		}
	} else
	{
	}

epilogue:
	if (nodeList)
		ixmlNodeList_free(nodeList);

	return ret;
}
