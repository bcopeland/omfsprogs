/* linked list stack */

#include "stack.h"
#include <stdlib.h>

stack_t *stack_init()
{
	stack_t *stack = malloc(sizeof(stack));
	stack->next = NULL;
	return stack;
}

int stack_empty(stack_t *stack)
{
	return (stack->next == NULL);
}

int stack_push(stack_t *stack, void *user)
{
	stack_t *new = malloc(sizeof(stack));
	if (!new)
		return 0;

	stack_t *tmp = stack->next;
	stack->next = new;
	new->next = tmp;
	new->data = user;
	return 1;
}

void *stack_pop(stack_t *stack)
{
	void *data;
	if (!stack->next) return NULL;

	stack_t *tmp = stack->next;
	stack->next = tmp->next;

	data = tmp->data;
	free(tmp);
	return data;
}

void stack_destroy(stack_t *stack)
{
	while (!stack_empty(stack))
		stack_pop(stack);

	free(stack);
}
