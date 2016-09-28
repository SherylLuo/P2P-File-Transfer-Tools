#include "queue.h"

void init_queue(int type, node *head, node **tail) {
	head->type = type;
	head->data = NULL;
	head->next = NULL;
	*tail = head;
}

void *pop_queue(node *head, node **tail) {
	if(head->next == NULL) {
		printf("Queue is empty\n");
		return NULL;
	} else {
		node *temp = head->next;
		head->next = head->next->next;
		if(head->next == NULL) 
			*tail = head;
		return temp->data;
	}
}

void push_queue(node *head, node **tail, void *data) {
	node *new_node;
	new_node = malloc(sizeof(node));
	if(new_node == NULL) {
		perror("malloc error");
		exit(1);
	}
	if(head->type == 0) {
		new_node->data = malloc(sizeof(struct sockaddr_in));
		*((struct sockaddr_in *)new_node->data) = *((struct sockaddr_in *)data);
	} else {
		new_node->data = malloc(sizeof(int));
		*((int *)new_node->data) = *((int *)data);
	}
	new_node->type = head->type;
	(*tail)->next = new_node;
	new_node->next = NULL;
	*tail = new_node;
}

int contains(node *head, void *data) {
	node *current = head->next;
	while(current != NULL) {
		if(head->type == 0) {
			if((*((struct sockaddr_in *)current->data)).sin_addr.s_addr == (*((struct sockaddr_in *)data)).sin_addr.s_addr)
				return 0;
		} else {
			if(*(int *)current->data == *(int *)data)
				return 0;
		}
		current = current->next;
	}
	return 1;
}
