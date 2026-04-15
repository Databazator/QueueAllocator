#include "QueueAllocator.h"

#include <stdexcept>
#include <iostream>
#include <cstddef>
#include <new>
#include <array>

// Queue Allocator : The "allocator" structure splits the data array into two chunks:
//		1. Header containing structs for Queue handles and Free lists for tracking resources that can be allocated. ~Funnily enough, the 'Header' lives at the end of the array, so it should be called a 'Tailer'?~
//		2. A Data Block that is divided into small DataSegements, each containg raw byte data and an index(pointer) of the next (connected or free) Segment .
// The Free resources and allocated DataSegments are in the form of linked lists. When a resource is needed, 
// the first free list element is returned and when a resource is returned, it is put at the end of the free resource list
// similiarly, when a queue's data spans several DataSegements, they are linked as "next's" during the aquisition of a new segment
// Allocated DataSegments are exclusive to a single Q, so data efficiency and availability could be affected in edge cases, 
// but there are no adjustment done to the structure during enqueue or dequeue that would require number of operation dependant on the structure's sizes.
// So both of these ops have constant complexity.


using SegmentIndex = unsigned char;

constexpr size_t DATA_SIZE = 2048;
constexpr size_t SEGMENT_DATA_SIZE = 13;

// due to the bit packing of offsets, 16 is the max size segment data can have
static_assert(SEGMENT_DATA_SIZE <= 16);

struct DataSegment {
	unsigned char data[SEGMENT_DATA_SIZE];
	SegmentIndex nextSegmentId;
};

struct Q {
	unsigned char frontSegmentId;
	unsigned char backSegmentId;
	unsigned char packedSegmentOffsets;
	
	// packed offset setters and getters
	unsigned char getFrontOffset()
	{
		return (packedSegmentOffsets >> 4) & 0x0F;
	}
	unsigned char getBackOffset()
	{
		return packedSegmentOffsets & 0x0F;
	}
	void setFrontOffset(unsigned char value)
	{
		packedSegmentOffsets = (packedSegmentOffsets & 0x0F) | ((value & 0x0F) << 4);
	}
	void setBackOffset(unsigned char value)
	{
		packedSegmentOffsets = (packedSegmentOffsets & 0xF0) | (value & 0x0F);
	}	
};

// precomputed offsets and sizes of the data's structures
constexpr size_t SEGMENT_SIZE = sizeof(DataSegment);

constexpr size_t Q_COUNT = 64;
constexpr size_t Q_SIZE = sizeof(Q);

constexpr size_t Q_STRUCTURE_SIZE = Q_COUNT * Q_SIZE;
constexpr size_t SEGMENT_ID_SIZE = sizeof(SegmentIndex);
constexpr size_t Q_ID_SIZE = sizeof(unsigned char);
constexpr size_t FREE_SEGMENT_LIST_SIZE = 2 * SEGMENT_ID_SIZE;
constexpr size_t FREE_Q_LIST_SIZE = 2 * Q_ID_SIZE;

constexpr size_t FREE_LISTS_SIZE = FREE_SEGMENT_LIST_SIZE + FREE_Q_LIST_SIZE + sizeof(unsigned char);
constexpr size_t SEGMENT_STRUCTURE_SIZE = DATA_SIZE - (Q_STRUCTURE_SIZE + FREE_LISTS_SIZE);
constexpr size_t SEGMENT_COUNT = SEGMENT_STRUCTURE_SIZE / SEGMENT_SIZE;

constexpr size_t Q_STRUCTURE_START_OFFSET = SEGMENT_STRUCTURE_SIZE;
constexpr size_t FREE_Q_LIST_HEAD_OFFSET = DATA_SIZE - FREE_LISTS_SIZE;
constexpr size_t FREE_Q_LIST_TAIL_OFFSET = DATA_SIZE - FREE_LISTS_SIZE + Q_ID_SIZE;
constexpr size_t FREE_SEGMENT_LIST_HEAD_OFFSET = DATA_SIZE - FREE_LISTS_SIZE + FREE_Q_LIST_SIZE;
constexpr size_t FREE_SEGMENT_LIST_TAIL_OFFSET = DATA_SIZE - FREE_LISTS_SIZE + FREE_Q_LIST_SIZE + SEGMENT_ID_SIZE;
constexpr size_t STRUCTURE_INITIALISED_INDIC_OFFSET = DATA_SIZE - sizeof(unsigned char);
const unsigned char STRUCTURE_INITIALISED = 192; //random flag that marks the array's structures as initialised

constexpr size_t UNUSED_SPACE = DATA_SIZE - (FREE_LISTS_SIZE + Q_STRUCTURE_SIZE + SEGMENT_COUNT * SEGMENT_SIZE);
constexpr size_t USED_SPACE = SEGMENT_COUNT * SEGMENT_DATA_SIZE;
constexpr float DATA_USED_PERCENT = (USED_SPACE * 100.0F / DATA_SIZE);

// precalcualted offsets for q and segment structs
constexpr auto Q_OFFSETS = [] {
	std::array<std::size_t, Q_COUNT> array{};
	for (size_t i = 0; i < Q_COUNT; ++i) {
		array[i] = Q_STRUCTURE_START_OFFSET + i * Q_SIZE;
	}
	return array;
	}();

constexpr auto SEGMENT_OFFSETS = [] {
	std::array<std::size_t, SEGMENT_COUNT> array{};
	for (size_t i = 0; i < SEGMENT_COUNT; ++i) {
		array[i] = i * SEGMENT_SIZE;
	}
	return array;
	}();

// -------------------------------------------------------------------------------
// Provided data array
unsigned char data[2048];

// -------------------------------------------------------------------------------
// helper functions - getters and setters of values and structs from the data array

const unsigned char GetCharAtOffset(size_t offset)
{
	if (offset >= DATA_SIZE) throw "GetCharAtOffset: Offset out of range";
	return data[offset];
}

void SetCharAtOffset(size_t offset, unsigned char value)
{
	if (offset >= DATA_SIZE) throw "SetCharAtOffset: Offset out of range";
	data[offset] = value;
}

Q* GetQById(size_t index)
{
	if (index >= Q_COUNT) throw "GetQById: Index out of range!";
	size_t offset = Q_OFFSETS[index];
	return reinterpret_cast<Q*>(&data[offset]);
}

DataSegment* GetSegmentById(size_t index)
{
	if (index >= SEGMENT_COUNT) throw std::out_of_range("GetSegmentById: Index out of range!");
	size_t offset = SEGMENT_OFFSETS[index];
	return reinterpret_cast<DataSegment*>(&data[offset]);
}

const unsigned char GetFreeQHead()
{
	return GetCharAtOffset(FREE_Q_LIST_HEAD_OFFSET);
}

void SetFreeQHead(unsigned char id)
{
	SetCharAtOffset(FREE_Q_LIST_HEAD_OFFSET, id);
}

const unsigned char GetFreeQTail()
{
	return GetCharAtOffset(FREE_Q_LIST_TAIL_OFFSET);
}

void SetFreeQTail(unsigned char id)
{
	SetCharAtOffset(FREE_Q_LIST_TAIL_OFFSET, id);
}

const unsigned char GetFreeSegHead()
{
	return GetCharAtOffset(FREE_SEGMENT_LIST_HEAD_OFFSET);
}

void SetFreeSegHead(unsigned char id)
{
	SetCharAtOffset(FREE_SEGMENT_LIST_HEAD_OFFSET, id);
}

const unsigned char GetFreeSegTail()
{
	return GetCharAtOffset(FREE_SEGMENT_LIST_TAIL_OFFSET);
}

void SetFreeSegTail(unsigned char id)
{
	SetCharAtOffset(FREE_SEGMENT_LIST_TAIL_OFFSET, id);
}

const size_t GetQIdFromPointer(Q* q)
{
	size_t offset = reinterpret_cast<unsigned char*>(q) - data;
	size_t Q_struct_offset = offset - Q_STRUCTURE_START_OFFSET;

	if (Q_struct_offset > Q_STRUCTURE_SIZE)
	{
		on_illegal_operation();
	}

	return Q_struct_offset / Q_SIZE;
}

// nextFreeQId is only used when q is in the free list, so one of it's indices can be used for storing the nextId
// although it is a bit confusing...
const unsigned char GetNextFreeQId(Q* q)
{
	return q->frontSegmentId;
}

void SetNextFreeQId(Q* q, unsigned char id)
{
	q->frontSegmentId = id;
}

// -------------------------------------------------------------------------------
// Debug print utils
void Print(Q* q)
{
	std::cout << "HEAD: id:" << (int)q->frontSegmentId << ", offset:"
		<< (int)q->getFrontOffset() << " TAIL: id:"
		<< (int)q->backSegmentId << ", offset:"
		<< (int)q->getBackOffset() << " NextFreeQId:"
		<< (int)GetNextFreeQId(q) << std::endl;
}
void Print(DataSegment* d)
{
	std::cout << "SegmentData: ";
	for (int i = 0; i < sizeof(d->data); i++)
	{
		std::cout << " " << (int)d->data[i];
		if (i != sizeof(d->data) - 1)
			std::cout << ",";
	}
	std::cout << " | NextSegmentId: " << (int)d->nextSegmentId << std::endl;
}
void PrintFreeListsIndices()
{
	std::cout << "Free Q head id: " << (int)GetFreeQHead()
		<< ", tail id: " << (int)GetFreeQTail()
		<< std::endl
		<< "Free Segments head id: " << (int)GetFreeSegHead()
		<< ", tail id: " << (int)GetFreeSegTail()
		<< std::endl;
}
void PrintStructureContents()
{
	DataSegment* d;
	for (size_t i = 0; i < SEGMENT_COUNT; i++)
	{
		std::cout << "id: " << i << " ";
		d = GetSegmentById(i);
		Print(d);
	}
	Q* q;
	for (size_t i = 0; i < Q_COUNT; i++)
	{
		std::cout << "id: " << i << " ";
		q = GetQById(i);
		Print(q);
	}
	PrintFreeListsIndices();
}

// -------------------------------------------------------------------------------
// Init structure

void InitialiseStructure()
{
	// init data segments
	unsigned char nextId = 1;
	for (const size_t& offset : SEGMENT_OFFSETS)
	{		
		new (&data[offset]) DataSegment{ {}, nextId++ };
	}
	// init qs
	nextId = 1;
	for (const size_t& offset : Q_OFFSETS)
	{
		//	   Q struct is : { frontId,  backId,  packedOffsets}
		new (&data[offset]) Q{ nextId++, SEGMENT_COUNT, 0};
	}
	// init free lists indices
	SetFreeQHead(0);
	SetFreeQTail(Q_COUNT - 1);
	SetFreeSegHead(0);
	SetFreeSegTail(SEGMENT_COUNT - 1);	
	// set structure as initialised
	SetCharAtOffset(STRUCTURE_INITIALISED_INDIC_OFFSET, STRUCTURE_INITIALISED);
}

void TryInitialise()
{
	if (GetCharAtOffset(STRUCTURE_INITIALISED_INDIC_OFFSET) != STRUCTURE_INITIALISED)
	{
		InitialiseStructure();
	}
}

// -------------------------------------------------------------------------------------
// Free list modification functions

const unsigned char GetFreeQ()
{
	unsigned char freeHead = GetFreeQHead();
	// no more free queues
	if (freeHead >= Q_COUNT) on_out_of_memory();

	if (freeHead == GetFreeQTail()) // tail points at head, list will now be empty
	{
		SetFreeQTail(Q_COUNT);
	}
	// set the next free q in list as new head
	Q* q = GetQById(freeHead);
	unsigned char nextFreeQ = GetNextFreeQId(q);

	SetFreeQHead(nextFreeQ);

	return freeHead;
}

void PutQInFreeList(const unsigned char id)
{
	// illegal id supplied
	if (id >= Q_COUNT) on_illegal_operation();

	// set next Q to invalid
	SetNextFreeQId(GetQById(id), Q_COUNT);

	unsigned char tailQId = GetFreeQTail();
	if (tailQId >= Q_COUNT) // free list is empty
	{
		SetFreeQHead(id);
		SetFreeQTail(id);
		return;
	}
	// set current tail q's nextFreeId to the newly added q's id 
	Q* tailQ = GetQById(tailQId);
	SetNextFreeQId(tailQ, id);

	SetFreeQTail(id);
}

const unsigned char GetFreeSegment()
{
	unsigned char freeHead = GetFreeSegHead();

	// no more free segments
	if (freeHead >= SEGMENT_COUNT) on_out_of_memory();

	if (freeHead == GetFreeSegTail()) // tail points at head, list will now be empty
	{
		SetFreeSegTail(SEGMENT_COUNT);
	}
	// set the next free segment in list as new head
	DataSegment* s = GetSegmentById(freeHead);
	unsigned char nextFreeSeg = s->nextSegmentId;

	SetFreeSegHead(nextFreeSeg);

	return freeHead;
}

void PutSegmentInFreeList(const unsigned char id)
{
	// illegal id supplied
	if (id >= SEGMENT_COUNT) on_illegal_operation();

	// set next segment to invalid
	GetSegmentById(id)->nextSegmentId = SEGMENT_COUNT;

	unsigned char tailSegId = GetFreeSegTail();
	if (tailSegId >= SEGMENT_COUNT) // free list is empty
	{
		SetFreeSegHead(id);
		SetFreeSegTail(id);
		return;
	}
	// set current tail segment's nextId to the newly added segment's id
	DataSegment* tailSeg = GetSegmentById(tailSegId);
	tailSeg->nextSegmentId = id;
	// set newly added segment as tail
	SetFreeSegTail(id);
}
// free all segments belonging to a give Q
void FreeQSegments(Q* q)
{
	unsigned char currId = q->frontSegmentId;
	unsigned char nextId;
	// traverse the segment linked list and free them
	while (currId < SEGMENT_COUNT)
	{		
		nextId = GetSegmentById(currId)->nextSegmentId;
		PutSegmentInFreeList(currId);
		currId = nextId;
	}
}

//---------------------------------------------------------------------------------
// api

Q* create_queue() {
	TryInitialise(); // initilise structures if it isn't already

	unsigned char newQId = GetFreeQ();
	Q* q = GetQById(newQId);
	q->frontSegmentId = SEGMENT_COUNT;
	q->backSegmentId = SEGMENT_COUNT;
	return q;
}
void destroy_queue(Q* q)
{
	if (q == nullptr) on_illegal_operation();

	// put all used segments back to free list
	FreeQSegments(q);

	//reset front, back indices
	q->frontSegmentId = SEGMENT_COUNT;
	q->backSegmentId = SEGMENT_COUNT;
	// free q
	size_t qId = GetQIdFromPointer(q);
	PutQInFreeList(qId);
}
void enqueue_byte(Q* q, unsigned char byte)
{
	if (q == nullptr) on_illegal_operation();

	// Something has gone wrong
	if (q->getBackOffset() >= SEGMENT_DATA_SIZE) on_illegal_operation();

	// if Q is empty
	if (q->backSegmentId >= SEGMENT_COUNT) 
	{
		// Get new free segment
		unsigned char newSegmentId = GetFreeSegment();
		DataSegment* newSegment = GetSegmentById(newSegmentId);
		// setup queue front and back
		q->frontSegmentId = newSegmentId;
		q->setFrontOffset(0);
		q->backSegmentId = newSegmentId;
		q->setBackOffset(0);
		//write data and return
		newSegment->data[q->getBackOffset()] = byte;
		newSegment->nextSegmentId = SEGMENT_COUNT;
		return;
	}

	DataSegment* currentBackSegment = GetSegmentById(q->backSegmentId);

	// back segment is full -> gonna need new segment
	if (q->getBackOffset() >= SEGMENT_DATA_SIZE - 1)
	{
		// get new free segment
		unsigned char newSegmentId = GetFreeSegment();
		DataSegment* newSegment = GetSegmentById(newSegmentId);
		// set q back segment and previous segment's nextSegmentId to new segment
		currentBackSegment->nextSegmentId = newSegmentId;
		q->backSegmentId = newSegmentId;
		q->setBackOffset(0);
		// write data and return
		newSegment->data[q->getBackOffset()] = byte;
		newSegment->nextSegmentId = SEGMENT_COUNT;
		return;
	}
	else // there is space in current back segment, just write the data to it
	{
		q->setBackOffset(q->getBackOffset() + 1);
		currentBackSegment->data[q->getBackOffset()] = byte;
		return;
	}
}
unsigned char dequeue_byte(Q* q)
{
	if (q == nullptr) on_illegal_operation();
	// queue is empty -> illegal op
	if (q->frontSegmentId >= SEGMENT_COUNT) on_illegal_operation();
	// if q segment offset somehow got changed from outside to bad value
	if (q->getFrontOffset() >= SEGMENT_DATA_SIZE) on_illegal_operation();

	DataSegment* frontSegment = GetSegmentById(q->frontSegmentId);
	unsigned char element = frontSegment->data[q->getFrontOffset()];

	// is last element in q -> free segment and set q to empty
	if (q->frontSegmentId == q->backSegmentId && q->getFrontOffset() == q->getBackOffset())
	{		
		PutSegmentInFreeList(q->frontSegmentId);
		q->frontSegmentId = SEGMENT_COUNT;
		q->backSegmentId = SEGMENT_COUNT;
		return element;
	}
	// popping last element in segment -> segment can be freed;
	if (q->getFrontOffset() == SEGMENT_DATA_SIZE - 1)
	{
		// free front segment
		unsigned char nextSegment = frontSegment->nextSegmentId;
		PutSegmentInFreeList(q->frontSegmentId);
		//set ids to next segment
		q->frontSegmentId = nextSegment;
		q->setFrontOffset(0);
		return element;
	}
	else // more elements remain in segment, just pop front element
	{
		q->setFrontOffset(q->getFrontOffset() + 1);
		return element;
	}	
}