#ifndef QUEUE_
#define QUEUE_

#include "net_include.h"

typedef struct Node{
	int type; //0 represents sender_queue, 1 represents nacks
	void *data;
	struct Node *next;
} node;

typedef struct Queue {
	node head;
	node *tail;
} queue;

void init_queue(int type, node *head, node **tail);
void *pop_queue(node *head, node **tail);
void push_queue(node *head, node **tail, void *data);
int contains(node *head, void *data);

#endif
