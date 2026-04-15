#include "QueueAllocator.h"
#include <iostream> 
#include <vector> 
#include <cassert> 
#include <string>

// fail functions implementation
void on_out_of_memory() { std::cerr << "[FAIL] Out of memory triggered\n"; std::abort(); } 
void on_illegal_operation() { std::cerr << "[FAIL] Illegal operation triggered\n"; std::abort(); }

void AssertEqual(unsigned char a, unsigned char b)
{ 
	if (a != b) { std::cerr << "Assert fail!: " << (int)a << " != " << (int)b << "\n"; std::abort(); }
}

void TestBasic()
{
	Q* q0 = create_queue();
	enqueue_byte(q0, 0);
	enqueue_byte(q0, 1);
	Q* q1 = create_queue();
	enqueue_byte(q1, 3);
	enqueue_byte(q0, 2);
	enqueue_byte(q1, 4);

	AssertEqual(dequeue_byte(q0), 0);
	AssertEqual(dequeue_byte(q0), 1);

	enqueue_byte(q0, 5);
	enqueue_byte(q1, 6);

	AssertEqual(dequeue_byte(q0), 2);
	AssertEqual(dequeue_byte(q0), 5);

	destroy_queue(q0);

	AssertEqual(dequeue_byte(q1), 3);
	AssertEqual(dequeue_byte(q1), 4);
	AssertEqual(dequeue_byte(q1), 6);

	destroy_queue(q1);

	std::cout << "TestBasic passed" << std::endl;
}


void TestMultipleQueues()
{
	std::vector<Q*> queues;

	int qCount = 20;
	int qDataCount = 30;

	for (int i = 0; i < qCount; i++)
	{
		queues.push_back(create_queue());

		for (int j = 0; j < qDataCount; j++)
		{
			enqueue_byte(queues[i], j);
		}
	}

	for (int i = 0; i < qCount; i++)
	{	
		for (int j = 0; j < qDataCount; j++)
		{
			AssertEqual(dequeue_byte(queues[i]), j);			
		}
	}

	for (auto q : queues) 
	{
		destroy_queue(q); 
	}

	std::cout << "TestMultipleQueues passed" << std::endl;
}

void TestLargeData()
{
	Q* q = create_queue();

	int n = 1500;

	for (int i = 0; i < n; i++)
	{
		enqueue_byte(q, i % 256);
	}

	for (int i = 0; i < n; i++)
	{
		AssertEqual(dequeue_byte(q), i % 256);
	}
	destroy_queue(q);

	std::cout << "TestLargeData passed" << std::endl;
}

void TestInterleaved()
{
	Q* q = create_queue();
	Q* q1 = create_queue();

	for (int i = 0; i < 120; i++) {
		Q* target = i % 2 ? q : q1;

		enqueue_byte(target, i);
		AssertEqual(dequeue_byte(target), i);
	}

	destroy_queue(q);
	destroy_queue(q1);

	std::cout << "TestInterleaved passed" << std::endl;
}

void TestManyQueuesLargeData()
{
	std::vector<Q*> queues;

	int qCount = 15;
	int qDataCount = 84;
	int queueChunks = 21;

	for (int i = 0; i < qCount; i++)
	{
		queues.push_back(create_queue());
	}

	for (int k = 0; k < qDataCount / queueChunks; k++)
	{
		for (int i = 0; i < qCount; i++)
		{
			for (int j = 0; j < queueChunks; j++)
			{
				enqueue_byte(queues[i], j);
			}
		}
	}

	for (int i = 0; i < qCount; i++)
	{
		for (int j = 0; j < qDataCount; j++)
		{
			AssertEqual(dequeue_byte(queues[i]), j % queueChunks);
		}
	}

	std::cout << "TestManyQueuesLargeData passed" << std::endl;
}


int main()
{
	TestBasic();
	TestMultipleQueues();
	TestLargeData();
	TestInterleaved();
	TestManyQueuesLargeData();

	std::cout << "===== All tests passed =====" << std::endl;
	return 0;
}

