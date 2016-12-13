#include "timer.h"
#include "math.h"

#define TICKS_IN_1_S 1024.0

/*!
 * Timers list head pointer
 */
static TimerEvent_t *TimerListHead = NULL;

static TimerTime_t getMilliseconds();
static void removeFromList(TimerEvent_t *obj);
static void addToList(TimerEvent_t *obj);
static void scheduleEvent(int32u milliseconds);
static void printList();

EmberEventControl loraMacNodeTimingEventControl;

void TimerInit( TimerEvent_t *obj, void ( *callback )( void ) )
{
    obj->ExpiryTime = 0;
    obj->ReloadValue = 0;
    obj->Callback = callback;
    obj->Next = NULL;
}

void TimerSetValue( TimerEvent_t *obj, int32u value )
{
    removeFromList(obj);
    obj->ReloadValue = value;
//emberAfPrintln(0xFFFF, "%s: Set timer %x value to %X%X%X%X", __func__, obj, (obj->ReloadValue)>>24, (obj->ReloadValue)>>16, (obj->ReloadValue)>>8, obj->ReloadValue);
}

void TimerStart( TimerEvent_t *obj )
{
    boolean scheduleNewEvent = FALSE;
    
    if (TimerListHead == NULL) {
        scheduleNewEvent = TRUE;
    }
    
    obj->ExpiryTime = getMilliseconds() + obj->ReloadValue;
    addToList(obj);
//emberAfPrintln(0xFFFF, "%s: Started timer %x. Will expire at time %X%X%X%X", __func__, obj, (obj->ExpiryTime)>>24, (obj->ExpiryTime)>>16, (obj->ExpiryTime)>>8, obj->ExpiryTime);
//printList();

    if (scheduleNewEvent) {
        scheduleEvent(obj->ExpiryTime - getMilliseconds());
    }
}

void TimerStop( TimerEvent_t *obj )
{
    removeFromList(obj);
//emberAfPrintln(0xFFFF, "%s: Stopped timer %x", __func__, obj);
//printList();
}

TimerTime_t TimerGetElapsedTime( TimerTime_t savedTime )
{
    // Needed at boot, cannot compute with 0 or elapsed time will be equal to current time
    if( savedTime == 0 )
    {
        return 0;
    }
    
    return (getMilliseconds() - savedTime);
}

TimerTime_t TimerGetCurrentTime( void )
{
    return getMilliseconds();
}

void TimerIrqHandler( void )
{
    TimerTime_t currentTime = getMilliseconds();
    TimerTime_t timeToEvent;
    TimerTime_t timeToNextEvent = 0xFFFFFFFF;
    boolean scheduleNewEvent = FALSE;
    
    void ( *currentTimerCallback )( void );
    TimerTime_t currentTimerExpiryTime;
    TimerEvent_t* currentTimerPointer;
    
    TimerEvent_t* cur = TimerListHead;
    
    //scan the entire list
    while (cur != NULL) {
        //store the relevant information of the current timer and move to the next one
        //(just in case the callback changes any property of the timer itself)
        currentTimerCallback = cur->Callback;
        currentTimerExpiryTime = cur->ExpiryTime;
        currentTimerPointer = cur;
        cur = cur->Next;
        
        if (currentTime >= currentTimerExpiryTime) {
            //current timer has expired: time to perform its action
            currentTimerCallback();
            removeFromList(currentTimerPointer);
        }
        else {
            //current timer has not yet expired: determine when the next timer event will occur
            scheduleNewEvent = TRUE;
            timeToEvent = currentTimerExpiryTime - currentTime;
            if (timeToEvent < timeToNextEvent) {
                timeToNextEvent = timeToEvent;
            }
        }
    }
    
    if (scheduleNewEvent) {
        scheduleEvent(timeToNextEvent);
    }
}

void zeroTimer(TimerEvent_t *obj)
{
    obj->ExpiryTime = 0;
    obj->ReloadValue = 0;
    obj->Callback = NULL;
    obj->Next = NULL;
}

extern TimerEvent_t MacStateCheckTimer;
extern TimerEvent_t TxDelayedTimer;
extern TimerEvent_t RxWindowTimer1;
extern TimerEvent_t RxWindowTimer2;
extern TimerEvent_t AckTimeoutTimer;
extern TimerEvent_t TxTimeoutTimer;
extern TimerEvent_t RxTimeoutTimer;
extern TimerEvent_t RxTimeoutSyncWord;
void loraMacNodeInitTimers()
{    
    emberEventControlSetInactive(loraMacNodeTimingEventControl);
    
    //Zero all existing timers and clear timer list
    zeroTimer(&MacStateCheckTimer);
    zeroTimer(&TxDelayedTimer);
    zeroTimer(&RxWindowTimer1);
    zeroTimer(&RxWindowTimer2);
    zeroTimer(&AckTimeoutTimer);
    zeroTimer(&TxTimeoutTimer);
    zeroTimer(&RxTimeoutTimer);
    zeroTimer(&RxTimeoutSyncWord);
    TimerListHead = NULL;
    
    //Zero all other state variables
}

static TimerTime_t getMilliseconds()
{   
    float ms = (float)halCommonGetInt32uMillisecondTick() * (float)1000.0 / (float)TICKS_IN_1_S;
    return (TimerTime_t)roundf(ms);
}

static void removeFromList(TimerEvent_t *obj)
{
    TimerEvent_t **prev, **cur;

    cur = &TimerListHead;
    if (*cur == NULL) {
        //List is empty: nothing to do
        return;
    }
    
    while (*cur != NULL) {
        prev = cur;
        cur = &((*cur)->Next);
        if (*prev == obj) {
            //found the item: remove it
            *prev = (*prev)->Next;
        }
    }
}

static void addToList(TimerEvent_t *obj)
{
    obj->Next = TimerListHead;
    TimerListHead = obj;
}

static void printList()
{
    TimerEvent_t *cur = TimerListHead;
    
    emberAfPrint(0xFFFF, "%s: Timer list =", __func__);
    while (cur != NULL) {
        emberAfPrint(0xFFFF, " %x", cur);
        cur = cur->Next;
    }
    emberAfPrint(0xFFFF, "\n", __func__);
}

static void scheduleEvent(int32u milliseconds)
{
    emberAfEventControlSetDelay(&loraMacNodeTimingEventControl, (int32u)((float)milliseconds * 1.024));
}

void loraMacNodeTimingHandler()
{
    emberEventControlSetInactive(loraMacNodeTimingEventControl);
    TimerIrqHandler();
}
