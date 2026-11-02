#include "linked_list.h"
#include <stdlib.h>
#include <unistd.h>
LinkedList* ll_new(){
  // init the sentinel
  Node *sentinel = (Node *)malloc(sizeof(Node));
  sentinel->next = sentinel;
  sentinel->prev = sentinel;
  // init the list
  LinkedList *list = (LinkedList*)malloc(sizeof(LinkedList));

  list->sentinel = sentinel;

  return list;
}

void drop_list(LinkedList* ll){
  //iterate through the list
  Node* sentinel = ll->sentinel;
  while(sentinel->next != sentinel){
    Node* next_ = sentinel->next;
    free(sentinel);
    sentinel = next_;
  }

  free(sentinel);
  free(ll);
}

/*
 * Considering node's A and B are neighbors, introduce node N between them
 */
static void link_between_(Node *a, Node *b, Node* node){
  //check if a and b are neighbors
  // interconnect node with the rest
  node->prev = a;
  node->next = b;
  b->prev = node;
  a->next = node;
}

static void unlink_(Node *node){
  //get node's neighbors:
  Node *prev = node->prev;
  Node *next = node->next;

  //connect the neighbors
  node->prev->next = next;
  node->next->prev = prev;

  node->next = NULL;
  node->prev = NULL;
}

void push_back(LinkedList *ll, Node *node){
  Node* last_node = ll->sentinel->prev;
  link_between_(last_node, ll->sentinel, node);
  ++(ll->size);
}
