#include <stdlib.h>
#include <string.h>
#include "library.h"

// push: push data at the end of node
int push(node_t** head, char data[100]) {
    node_t* current = *head;

    if(current == NULL) {
        node_t* new_node;
        new_node = (node_t*) malloc(sizeof(node_t));

        strcpy(new_node->data, data);
        new_node->next = NULL;
        *head = new_node;
        return 0;
    }

    while(current->next != NULL) {
       current = current->next;
    }

    current->next = (node_t*) malloc(sizeof(node_t));
    strcpy(current->next->data, data);
    current->next->next = NULL;
    return 1;
}

// delete: delete specific data from node
node_t* delete(node_t* head, char* data) {
    node_t* temp = head;
    node_t* prev = NULL;

    while(temp != NULL) {
        if(strcmp(temp->data, data) == 0) {
            if(prev == NULL) {  // if the node is the head
                head = head->next;
                free(temp);
                return head;
            } else {
                prev->next = temp->next;
                free(temp);
                return head;
            }
        }
        prev = temp;
        temp = temp->next;
    }
    return NULL;
}

// list_length: get length of linked list
int list_length(node_t* head) {
    int length = 0;
    node_t* current = head;
    while(current != NULL) {
        length++;
        current = current->next;
    }
    return length;
}

int get_index_of_node(node_t* head, const char* target) {
    int index = 0;
    while (head != NULL) {
        if (strcmp(head->data, target) == 0) {
            return index;
        }
        head = head->next;
        index++;
    }
    return -1; // Not found
}

