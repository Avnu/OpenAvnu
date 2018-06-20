#ifndef STACK_H
#define STACK_H

#include <linked_list.h>

typedef linked_list_t stack_t;
typedef linked_list_element_t stack_element_t;

#define STATIC_STACK_INIT NULL

static inline stack_t stack_init()
{
	return ll_init();
}

static inline stack_t stack_alloc()
{
	return ll_alloc();
}

static inline bool stack_is_valid( stack_t stack )
{
	return ll_is_valid( stack );
}

static inline void stack_init_element( stack_element_t *element )
{
	ll_init_element( element );
}


static inline bool push( stack_t stack, stack_element_t *element )
{
	return ll_add_head( stack, element );
}

static inline void free_stack_element( stack_element_t *element )
{
	ll_free_element( element );
}

static inline linked_list_t get_list( stack_t stack )
{
	return (linked_list_t) stack;
}

stack_element_t *
pop( stack_t stack );

#endif/*STACK_H*/
