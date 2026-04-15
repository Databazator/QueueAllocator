#include "QueueAllocator.h"
#include <iostream> 
#include <vector> 
#include <cassert> 
#include <string>

const char* OOM_MSG = "oom";
const char* ILLEGAL_OP_MSG = "illegalOp";

// fail functions implementation
void on_out_of_memory() { throw std::runtime_error(OOM_MSG); }
void on_illegal_operation() { throw std::runtime_error(ILLEGAL_OP_MSG); }


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

void TestEmptyDequeue()
{
	Q* q = create_queue();

	bool errCaught = false;

	try {
		dequeue_byte(q);
	}
	catch (const std::runtime_error& err)
	{		
		if (strcmp(err.what(), ILLEGAL_OP_MSG) == 0)				
			errCaught = true;		
	}

	assert(errCaught);

	destroy_queue(q);

	std::cout << "TestEmptyDequeue passed" << std::endl;
}

void TestNullptrOp()
{
	Q* q = nullptr;

	bool errCaught = false;

	try {
		dequeue_byte(q);
	}
	catch (const std::runtime_error& err)
	{
		if (strcmp(err.what(), ILLEGAL_OP_MSG) == 0)
			errCaught = true;
	}

	assert(errCaught);

	std::cout << "TestNullptrOp passed" << std::endl;

}

void TestOutOfMemory()
{
	Q* q = create_queue();

	bool errCaught = false;

	try {
		for (int i = 0; i < 5000; i++)
		{
			enqueue_byte(q, i % 256);
		}
	}
	catch (const std::runtime_error& err)
	{
		if (strcmp(err.what(), OOM_MSG) == 0)
			errCaught = true;
	}
	

	assert(errCaught);

	destroy_queue(q);

	std::cout << "TestOutOfMemory passed" << std::endl;

}


int main()
{
	TestBasic();
	TestMultipleQueues();
	TestLargeData();
	TestInterleaved();
	TestManyQueuesLargeData();
	TestEmptyDequeue();
	TestNullptrOp();
	TestOutOfMemory();

	std::cout << std::endl << "===== All tests passed =====" << std::endl;
	return 0;
}

