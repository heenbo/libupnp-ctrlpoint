#ifndef __CTRLPOINTSTART_H_
#define __CTRLPOINTSTART_H_


extern int CtrlPointStart(void);
extern int CtrlPointCallbackEventHandler(Upnp_EventType EventType, void *Event, void *Cookie);
extern int CtrlPointRefresh(void);
extern void *CtrlPointTimerLoop(void *args);

#endif //__CTRLPOINTSTART_H_
