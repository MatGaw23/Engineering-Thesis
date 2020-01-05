/* Includes ------------------------------------------------------------------*/
#include <assert.h>
#include "ring_buffer.h"


bool RingBuffer_Init(RingBuffer *ringBuffer, char *dataBuffer, size_t dataBufferSize) 
{
	assert(ringBuffer);
	assert(dataBuffer);
	assert(dataBufferSize > 0);


	if ((ringBuffer) && (dataBuffer) && (dataBufferSize > 0)) {

	  ringBuffer->head=dataBuffer;
	  ringBuffer->tail=dataBuffer;
	  ringBuffer->startmemory=dataBuffer;
	  ringBuffer->size=dataBufferSize;
	  ringBuffer->storedData=0;
	  return true;
	}
	
	return false;
}

bool RingBuffer_Clear(RingBuffer *ringBuffer)
{
	assert(ringBuffer);
	int i=0;
	char* temp=ringBuffer->startmemory;
	if (ringBuffer) {
		for(i=0;i<ringBuffer->size;i++)
		{
		    *temp++=0;
		}
		ringBuffer->storedData=0;
	    ringBuffer->head=ringBuffer->startmemory;
	    ringBuffer->tail=ringBuffer->startmemory;
	    ringBuffer->head=ringBuffer->startmemory;
	    return true;
	}
	return false;
}

bool RingBuffer_IsEmpty(const RingBuffer *ringBuffer)
{
  assert(ringBuffer);	
	//TODO
	if(ringBuffer->storedData!=0) return false;
	return true;
}

size_t RingBuffer_GetLen(const RingBuffer *ringBuffer)
{
	assert(ringBuffer);
	
	if (ringBuffer) {
	    return ringBuffer->storedData;
	}
	return 0;
	
}

size_t RingBuffer_GetCapacity(const RingBuffer *ringBuffer)
{
	assert(ringBuffer);
	
	if (ringBuffer) {
		return ringBuffer->size;
	}
	return 0;	
}


bool RingBuffer_PutChar(RingBuffer *ringBuffer, char c)
{
	assert(ringBuffer);
	if (ringBuffer) {
	   if(ringBuffer->storedData==ringBuffer->size)return false;
	   
	    *ringBuffer->head=c;
	    if(ringBuffer->head==ringBuffer->startmemory+ringBuffer->size-1)
	    	ringBuffer->head=ringBuffer->startmemory;
	    else
	    	ringBuffer->head++;

	    ringBuffer->storedData++;
	    
	    return true;
	}
	return false;
}

bool RingBuffer_GetChar(RingBuffer *ringBuffer, char *c)
{
	assert(ringBuffer);
	assert(c);
	
  if ((ringBuffer) && (c)) {
		//TODO
		if(ringBuffer->storedData==0) return false;
		*c=*ringBuffer->tail;
		if(ringBuffer->tail==ringBuffer->startmemory+ringBuffer->size-1)ringBuffer->tail=ringBuffer->startmemory;
	    else ringBuffer->tail++;
		ringBuffer->storedData--;
		return true;
	}
	return false;
}
