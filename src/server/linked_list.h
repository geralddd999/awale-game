#ifndef LINKED_LIST_H
#define LINKED_LIST_H

typedef struct Node Node;

struct Node{
  void* data;
  Node* next;
  Node* prev;
};

typedef struct LinkedList LinkedList;
struct LinkedList{
  Node* sentinel;
  unsigned int size;
};

void link_between(Node* a, Node* b, Node* node, LinkedList* llist);
static void unlink_(Node* node);
LinkedList* ll_new();
void drop_list(LinkedList* ll);
void push_back(LinkedList *ll, Node*);
void pop(LinkedList *ll, Node* n);

#endif
