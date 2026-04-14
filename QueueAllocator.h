#pragma once

#include <cstddef>

struct Q;

void PrintStructureContents();

void on_out_of_memory();
void on_illegal_operation();

Q* create_queue();
void destroy_queue(Q* q);
void enqueue_byte(Q* q, unsigned char byte);
unsigned char dequeue_byte(Q* q);

// temp
size_t GetQIdFromPointer(Q* q);
Q* GetQById(size_t index);