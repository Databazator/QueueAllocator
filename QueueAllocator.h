#pragma once

#include <cstddef>

struct Q;

void on_out_of_memory();
void on_illegal_operation();

Q* create_queue();
void destroy_queue(Q* q);
void enqueue_byte(Q* q, unsigned char byte);
unsigned char dequeue_byte(Q* q);

// debug printout func
void PrintStructureContents();