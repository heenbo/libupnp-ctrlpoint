#ifndef __CTRLPOINTSTART_H_
#define __CTRLPOINTSTART_H_

#define SERVICE_SERVCOUNT		3
#define SERVICE_AVTRANSPORT		0
#define SERVICE_CONNECTIONMANAGER	1
#define SERVICE_RENDERINGCONTROL	2

#define AVTRANSPORT_VARCOUNT		4
#define CONNECTIONMANAGER_VARCOUNT	4
#define RENDERINGCONTROL_VARCOUNT	4

/* This should be the maximum VARCOUNT from above */
#define MAXVARS		AVTRANSPORT_VARCOUNT
#define MAX_VAL_LEN 5
#define _NAME_SIZE	(NAME_SIZE*2)

struct service
{
    char ServiceId[_NAME_SIZE];
    char ServiceType[_NAME_SIZE];
    char *VariableStrVal[MAXVARS];
    char EventURL[_NAME_SIZE];
    char ControlURL[_NAME_SIZE];
    char SID[_NAME_SIZE];
};

struct Device 
{
    char UDN[_NAME_SIZE];
    char DescDocURL[_NAME_SIZE];
    char FriendlyName[_NAME_SIZE];
    char PresURL[_NAME_SIZE];
    int  AdvrTimeOut;
    struct service Service[SERVICE_SERVCOUNT];
};

struct DeviceNode 
{
    struct Device device;
    struct DeviceNode *next;
};

typedef enum
{
	STATE_UPDATE = 0,
	DEVICE_ADDED = 1,
	DEVICE_REMOVED = 2,
	GET_VAR_COMPLETE = 3
} eventType;

extern int CtrlPointStart(void);
extern int CtrlPointCallbackEventHandler(Upnp_EventType EventType, void *Event, void *Cookie);
extern int CtrlPointRefresh(void);
extern void *CtrlPointTimerLoop(void *args);
extern int CtrlPointDeleteNode(struct DeviceNode *node);
extern int CtrlPointRemoveAll(void);
extern void CtrlPointVerifyTimeouts(int incr);
extern int CtrlPointStop(void);
extern void *CtrlPointCommandLoop(void *args);
extern int CtrlPointProcessCommand(char *cmdline);
extern int CtrlPointSendAction( int service, int devnum, const char *actionname, 
				const char **param_name, char **param_val, int param_count);
extern int CtrlPointGetDevice(int devnum, struct DeviceNode **devnode);
extern int CtrlPointPrintList(void);
extern void CtrlPointAddDevice( IXML_Document *DescDoc, const char *location, int expires);
extern char *SampleUtil_GetFirstDocumentItem(IXML_Document *doc, const char *item);
extern int SampleUtil_FindAndParseService(IXML_Document *DescDoc, const char *location,
	const char *serviceType, char **serviceId, char **eventURL, char **controlURL);
extern IXML_NodeList *SampleUtil_GetFirstServiceList(IXML_Document *doc);
extern char *SampleUtil_GetFirstElementItem(IXML_Element *element, const char *item);
extern int CtrlPointRemoveDevice(const char *UDN);
extern void CtrlPointHandleSubscribeUpdate( const char *eventURL, const Upnp_SID sid, int timeout);
extern void CtrlPointHandleEvent( const char *sid, int evntkey, IXML_Document *changes);
extern void StateUpdate(char *UDN, int Service, IXML_Document *ChangedVariables, char **State);
extern char *SampleUtil_GetElementValue(IXML_Element *element);


#endif //__CTRLPOINTSTART_H_
