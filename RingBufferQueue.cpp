#include "RingBufferQueue.h"
#include <iostream>

unsigned char qdata[20];

class Queue {
public:
	int offset;
	int size;
	int qFront;
	int qBack;
	bool empty;

	Queue() : offset(0), size(8), qFront(0), qBack(0), empty(true) {}
};


void enqueue(Queue* q, unsigned char byte)
{
	if (q != nullptr && q->size > 0)
	{
		if (q->empty) //init
		{
			qdata[q->offset] = byte;
			q->qBack = 1;
			q->empty = false;
		}
		else
		{
			if (q->qFront != q->qBack)
			{
				qdata[q->offset + q->qBack] = byte;
				q->qBack = (q->qBack + 1) % q->size; // increment and over
			}
			else // FULL NOW
			{
				std::cout << "QUEUE FULL" << std::endl;
			}
		}
	}
	else
		throw "Invalid Q*";
}
unsigned char dequeue(Queue* q)
{
	if (q != nullptr && q->size > 0)
	{
		if (q->empty) // empty
		{
			std::cout << "QUEUE EMPTY" << std::endl;
			return 0;
		}
		unsigned char ret = qdata[q->offset + q->qFront];

		q->qFront = (q->qFront + 1) % q->size;
		if (q->qFront == q->qBack)
		{
			q->empty = true;
		}
	}
	else
		throw "Invalid Q*";
}
int getCharValue(Queue* q, int index)
{
	return static_cast<int>(qdata[q->offset + index]);
}

void printQueue(Queue* q)
{
	if (q != nullptr && q->size > 0 && !q->empty)
	{
		std::cout << "FRONT> ";
		if (q->qFront >= q->qBack) // loop over
		{
			for (int i = q->qFront; i < q->size; i++)
			{
				std::cout << getCharValue(q, i) << ", ";
			}
			for (int i = 0; i < q->qBack; i++)
			{
				std::cout << getCharValue(q, i) << ", ";
			}
		}
		else
		{
			for (int i = q->qFront; i < q->qBack; i++)
			{
				std::cout << getCharValue(q, i) << ", ";
			}
		}
		std::cout << " <BACK" << std::endl;
	}
}