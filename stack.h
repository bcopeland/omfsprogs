#ifndef _STACK_H
#define _STACK_H

struct stack_node
{
	void *data;
	struct stack_node *next;
};

typedef struct stack_node stack_t;

stack_t *stack_init();
int stack_empty(stack_t *);
int stack_push(stack_t *, void *);
void *stack_pop(stack_t *);
void stack_destroy(stack_t *);

#endif
