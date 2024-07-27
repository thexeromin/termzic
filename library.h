#ifndef LIBRARY_H
#define LIBRARY_H

#define MAXLEN 100

typedef struct node {
    char data[MAXLEN];
    struct node* next;
} node_t;

int push(node_t** head, char data[MAXLEN]);
int list_length(node_t* head);
node_t* delete(node_t* head, char* data);

#endif // LIBRARY_H
