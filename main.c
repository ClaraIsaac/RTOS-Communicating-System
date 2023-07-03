#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "diag/trace.h"

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"
#include "string.h"

#define CCM_RAM __attribute__((section(".ccmram")))


static void SenderTimerCallback( TimerHandle_t xTimer );
static void ReceiverTimerCallback( TimerHandle_t xTimer );

static TimerHandle_t xTimer1 = NULL;
static TimerHandle_t xTimer2 = NULL;
static TimerHandle_t xTimer3 = NULL;
BaseType_t xTimer1Started, xTimer2Started, xTimer3Started;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

QueueHandle_t stringQueue;
const int no_of_queue_elements = 2;
const int Lowerbound[] = {50, 80, 110, 140, 170, 200};
const int Upperbound[] = {150, 200, 250, 300, 350, 400};

// creating some variables and initializing it
int Blocked_messages = 0;
int Transmitted_messages = 0;
int Received_messages = 0;
int counter = 0;
static TickType_t Tsender1;
static TickType_t Tsender2;
static TickType_t Treceiver;
static SemaphoreHandle_t semSend;
BaseType_t s = pdFALSE;
BaseType_t r = pdFALSE;

// function to get random number following the uniform distribution between some interval
int randomFunction(int rangeLow, int rangeHigh)
{
    srand(time(NULL));
    long int Randno = random(); // get random number
    int range = rangeHigh - rangeLow + 1; // get the range which we want the random number to be within
    int Randno_s = (Randno % range) + rangeLow; // get the random number within the range
    return Randno_s;
}

// Sender Task Function
void SenderTask( void *param )
{
	BaseType_t xStatus;
	TickType_t xTimeNow;
	char message[20]; // for the message we want to send in the queue
	while(1)
	{
		// if s true means the timer is fired and the callback function is called
		if (s == pdTRUE)
		{
			xSemaphoreTake( semSend, 0); // take the semaphore
			xTimeNow = xTaskGetTickCount(); // get the current time in ticks
			int TimeNow = (int) xTimeNow;
			// concatenate "Time is " with the current time in a string (array of char)
			snprintf( message, 20, "Time is %d", TimeNow);
			xStatus = xQueueSend( stringQueue, &message, 0 ); // send in queue
			if ( xStatus != pdPASS ) // check if it failed to send in the queue
			{
				Blocked_messages ++;
			}
			else Transmitted_messages ++;
			// change the period of the timers
			Tsender1 = pdMS_TO_TICKS(randomFunction(Lowerbound[counter], Upperbound[counter]));
			xTimerChangePeriod( xTimer1, Tsender1, 0 );
			Tsender2 = pdMS_TO_TICKS(randomFunction(Lowerbound[counter], Upperbound[counter]));
			xTimerChangePeriod( xTimer2, Tsender2, 0 );
			s = pdFALSE; // return s to false hence it can't enter to the function unless the timer fires
			xSemaphoreGive(semSend); // release the semaphore
		}
	}

}

// Receiver Task function
void ReceiverTask( void *param )
{
	BaseType_t yStatus;
	char receivedValue[20]; // receive what's in the queue in this variable
	while(1)
	{
		// if r true means the timer is fired and the callback function is called
		if (r == pdTRUE)
		{
			xSemaphoreTake( semSend, 0); // take the semaphore
			yStatus = xQueueReceive( stringQueue, &receivedValue, 0 ); // receive the value from the queue
			if( yStatus == pdPASS ) // checks if it succeeded to receive the value
			{
				printf("%s \n", receivedValue ); // print the received value
				Received_messages ++;
			}
			r = pdFALSE; // return r to false hence it can't enter to the function unless the timer fires
			xSemaphoreGive(semSend); // release the semaphore
		}
	}
}

// sender timer callback function
static void SenderTimerCallback( TimerHandle_t xTimer )
{
	xSemaphoreTake(semSend, 0); //take the semaphore
	s = pdTRUE; // change s to true so sender task can enter his function
    xSemaphoreGive(semSend); // release the semaphore
}

// Reset function
void rreset()
{
	printf("Sent messages = %d\n", Transmitted_messages);
	printf("Blocked messages = %d\n", Blocked_messages);
	// clears the variables and the queue
	Transmitted_messages = 0;
	Blocked_messages = 0;
	Received_messages = 0;
	xQueueReset( stringQueue );
	// counter represents how many elements are used in the arrays (lowerbound and upperbound)
	// it increments every cycle
	counter ++;
	// counter > 5 means we have used all elements in the two arrays
	if (counter > 5)
	{
		// delete timers, queue, print "game over", stop the scheduler
		xTimerDelete( xTimer1, 0);
		xTimerDelete( xTimer2, 0);
		xTimerDelete( xTimer3, 0);
		vQueueDelete( stringQueue );
		printf("Game Over\n");
		vTaskEndScheduler();
	}
}

// receiver timer callback function
static void ReceiverTimerCallback( TimerHandle_t xTimer )
{
	xSemaphoreTake(semSend, 0); // take the semaphore
	r = pdTRUE; // change r to true so sender task can enter his function
	// check if the received values from sender task reached 500 messages.
	if (Received_messages == 500)
	{
		// if yes, go to reset function
		rreset();
	}
	xSemaphoreGive(semSend); // release the semaphore
}


int
main(int argc, char* argv[])
{
	// create queue
	stringQueue = xQueueCreate( no_of_queue_elements , 20 * sizeof( char ) );
	if( stringQueue != NULL )
	{
		// create tasks
	    xTaskCreate( SenderTask, "Sender1", 3000 , NULL, 1, NULL );
	    xTaskCreate( SenderTask, "Sender2", 3000, NULL, 1, NULL );
	    xTaskCreate( ReceiverTask, "Receiver", 3000, NULL, 1, NULL );
	}
	// initialize the period of timers
	Tsender1 = pdMS_TO_TICKS(randomFunction(Lowerbound[counter], Upperbound[counter]));
	Tsender2 = pdMS_TO_TICKS(randomFunction(Lowerbound[counter], Upperbound[counter]));
    Treceiver = pdMS_TO_TICKS( 100 );

    // create timers
	xTimer1 = xTimerCreate( "Timer1", Tsender1, pdTRUE, 0, SenderTimerCallback);
	xTimer2 = xTimerCreate( "Timer2", Tsender2, pdTRUE, 0, SenderTimerCallback	);
	xTimer3 = xTimerCreate( "Timer3", Treceiver, pdTRUE, 0, ReceiverTimerCallback);

	if( ( xTimer1 != NULL ) && ( xTimer2 != NULL ) && ( xTimer3 != NULL ) )
	{
		// start the timers
		xTimer1Started = xTimerStart( xTimer1, 0 );
		xTimer2Started = xTimerStart( xTimer2, 0 );
		xTimer3Started = xTimerStart( xTimer3, 0 );
	}
	semSend = xSemaphoreCreateBinary(); // create semaphore

	if( xTimer1Started == pdPASS && xTimer2Started == pdPASS && xTimer3Started == pdPASS)
	{
		// start scheduling
		vTaskStartScheduler();
	}

	return 0;
}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------

void vApplicationMallocFailedHook( void )
{

	for( ;; );
}


void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	for( ;; );
}


void vApplicationIdleHook( void )
{
volatile size_t xFreeStackSpace;


	xFreeStackSpace = xPortGetFreeHeapSize();

	if( xFreeStackSpace > 100 )
	{

	}
}

void vApplicationTickHook(void) {
}

StaticTask_t xIdleTaskTCB CCM_RAM;
StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE] CCM_RAM;

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) {

  *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;


  *ppxIdleTaskStackBuffer = uxIdleTaskStack;


  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB CCM_RAM;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH] CCM_RAM;


void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize) {
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
  *ppxTimerTaskStackBuffer = uxTimerTaskStack;
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
