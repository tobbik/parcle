#include <stdio.h>      // printf
#include <stdlib.h>     // calloc

#define LENGTH 10

struct node {
	struct node  *prev;
	struct node  *next;
	struct node  *queue;
	int           data;
};

static void list_list             (struct node *nd);
static void populate_used         ();
static void add_to_useds          (struct node *nd);
static void remove_from_useds     (struct node *nd);
static void from_useds_to_frees   ();

struct node *_frees;
struct node *_useds;
struct node *_queue_head;
struct node *_queue_tail;


int main ()
{
	int          i;
	struct node *tmp;

	// fill the frees
	for (i=0; i<LENGTH; i++) {
		tmp = _frees;
		_frees = (struct node *) calloc (1, sizeof(struct node));
		_frees->next  = tmp;
		_frees->prev  = NULL;
		_frees->queue = NULL;
		_frees->data  = i;
	}

	list_list(_frees);
	populate_used();
	list_list(_frees);
	list_list(_useds);
	from_useds_to_frees();
	list_list(_frees);
	list_list(_useds);

	return 0;
}

static void list_list (struct node *nd)
{
	struct node *tmp, *tmp1;

	tmp=nd;
	printf("prev\tdata\tnext\n" );
	while (NULL != tmp) {
		tmp1 = tmp->next;
		if (NULL != tmp->prev && NULL != tmp->next)
			printf("%d\t%d\t%d\n", tmp->prev->data, tmp->data, tmp->next->data );
		else if (NULL == tmp->prev && NULL != tmp->next)
			printf("  \t%d\t%d\n", tmp->data, tmp->next->data );
		else if (NULL != tmp->prev && NULL == tmp->next)
			printf("%d\t%d\t  \n", tmp->prev->data, tmp->data);
		else
			printf("  \t%d\t  \n", tmp->data);
		tmp=tmp1;
	}
}

static void populate_used ()
{
	struct node *tmp, *tmp1;

	tmp = _frees;
	while (NULL != tmp) {
		tmp1 = tmp->next;
		if (tmp->data>3) {
			_frees = tmp1;
			add_to_useds (tmp);
		}
		else
			break;
		tmp=tmp1;
	}
}

static void from_useds_to_frees ()
{
	struct node *tmp,*tmp1;
	tmp = _useds;

	while (NULL != tmp) {
		printf("%d  ", tmp->data);
		tmp1=tmp->next;
		if (tmp->data%2)
			remove_from_useds (tmp);
		tmp=tmp1;
	}
}

static void add_to_useds (struct node *nd)
{
	//struct node *tmp;
	nd->prev = NULL;
	if (NULL == _useds){
		nd->next = NULL;
	}
	else {
		nd->next    = _useds;
		_useds->prev = nd;
	}
	_useds = nd;
}

static void remove_from_useds (struct node *nd)
{
	//struct node *tmp;
	if (NULL == _useds) {
		return;
	}
	else if (NULL == nd->prev) {    /* tail of _useds */
		if (NULL == nd->next) {      /* only one in the list */
			_useds = NULL;
		}
		else {
			nd->next->prev  = NULL;
			_useds          = nd->next;
		}
	}
	else if (NULL == nd->next) {    /* head of _useds */
		nd->prev->next  = NULL;
		nd->prev        = NULL;
	}
	else {
		nd->prev->next = nd->next;
		nd->next->prev = nd->prev;
	}
	nd->next = _frees;
	nd->prev = NULL;
	_frees = nd;
}
