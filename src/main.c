#include <stdio.h>
#include <stdlib.h>

typedef struct __node
{
    struct __node* next;
    int value;
} node;

void print_list(node* head)
{
    node* n = head;

    printf("[");
    while (n)
    {
        printf("%i", n->value);
        n = n->next;
        if (n)
            printf(" ");
    }
    printf("]\n");
}

void add_first(node** head, int value)
{
    node* n = malloc(sizeof(node));
    if (!n)
        return;
    n->value = value;
    n->next = *head;
    *head = n;
}

void remove_first(node** head)
{
    if (*head == NULL)
        return;
    node* next = (*head)->next;
    free(*head);
    *head = next;
}

void free_list(node** head)
{
    while (*head)
        remove_first(head);
}

int main(int argc, const char* argv[])
{
    printf("Hello World!\n");

    printf("%i\n", sizeof(node));

    node* head = NULL;

    remove_first(&head);

    add_first(&head, 5);
    add_first(&head, 4);
    add_first(&head, 1);

    print_list(head);

    remove_first(&head);

    add_first(&head, 3);
    add_first(&head, 2);

    print_list(head);

    free_list(&head);
    getchar();
}
