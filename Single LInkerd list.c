/* Yanling Wang CMPSC311 hw5 Spring 2019 */
#include <stdio.h>
#include <stdlib.h>
#include "list.h"

int 
countNodes(const listNode * list)
{
	const listNode *newlist = list;
	int		n = 0;
	if (list != NULL) {
		do {
			newlist = newlist->next;
			n++;
		} while (newlist != NULL);
		return n;
	} else {
		return 0;
	}
	return 0;
}
void 
insertAfter(listNode ** listPtr, int findValue, int value)
{
	listNode       *tmp = *listPtr;
	if (*listPtr == NULL) {
		*listPtr = (listNode *) malloc(sizeof(listNode));
		(*listPtr)->value = value;
		(*listPtr)->next = NULL;
		return;
	}
	while (tmp->next != NULL && tmp->value != findValue) {
		tmp = tmp->next;
	}
	listNode       *next_node = tmp->next;
	tmp->next = (listNode *) malloc(sizeof(listNode));
	tmp->next->value = value;
	tmp->next->next = next_node;
	return;
}
void 
findAndMove(listNode ** listPtr, int findValue)
{
	listNode       *tmp = *listPtr;
	if (tmp == NULL)
		return;
	listNode       *pre = NULL;
	while (tmp != NULL) {
		if (tmp->value == findValue) {
			if (pre == NULL)
				return;
			listNode       *origin_head = *listPtr;
			(*listPtr) = tmp;
			pre->next = tmp->next;
			tmp->next = origin_head;
			break;
		} else {
			pre = tmp;
			tmp = tmp->next;
		}
	}
	return;
}
void 
removeAll(listNode ** listPtr, int findValue)
{
	listNode       *tmp = *listPtr;
	listNode       *pre = NULL;
	while (tmp != NULL) {
		if (tmp->value == findValue) {
			if (pre == NULL) {
				*listPtr = tmp->next;
				free(tmp);
				tmp = *listPtr;
			} else {
				pre->next = tmp->next;
				free(tmp);
				tmp = pre->next;
			}
		} else {
			pre = tmp;
			tmp = tmp->next;
		}
	}
	return;
}
