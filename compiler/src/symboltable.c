/**
 * @file    symboltable.c
 * @brief   A symbol table for AMPL-2023.
 * @author  W.H.K. Bester (whkbester@cs.sun.ac.za)
 * @date    2023-07-06
 */

#include "symboltable.h"

#include "boolean.h"
#include "error.h"
#include "hashtable.h"
#include "token.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- global static variables ---------------------------------------------- */

static HashTab *table, *saved_table;
/* TODO: Nothing here, but note that the next variable keeps a running count of
 * the number of variables in the current symbol table.  It will be necessary
 * during code generation to compute the size of the local variable array of a
 * method frame in the Java virtual machine.
 */
static unsigned int curr_offset;

/* --- function prototypes -------------------------------------------------- */

static void valstr(void *key, void *p, char *str);
static void freeprop(void *p);
static unsigned int shift_hash(void *key, unsigned int size);
static int key_strcmp(void *val1, void *val2);

/* --- symbol table interface ----------------------------------------------- */

void init_symbol_table(void)
{
	saved_table = NULL;
	if ((table = ht_init(0.75f, shift_hash, key_strcmp)) == NULL) {
		eprintf("Symbol table could not be initialised");
	}
	curr_offset = 1;
}

Boolean open_subroutine(char *id, IDPropt *prop)
{
	if (table == NULL) {
		return FALSE;
	}
	if (ht_insert(table, id, prop) != EXIT_SUCCESS) {
		return FALSE;
	}

	saved_table = table;

	table = ht_init(0.75f, shift_hash, key_strcmp);

	if (table == NULL) {
		return FALSE;
	}
	curr_offset = 1;
	return TRUE;
}

void close_subroutine(void)
{
	/* TODO: Release the subroutine table, and reactivate the global table. */
	if (table != NULL) {
		release_symbol_table();
		table = saved_table;
		saved_table = NULL;
	}
	curr_offset = 0;
}

Boolean insert_name(char *id, IDPropt *prop)
{
	/* Check if the symbol table is initialized */
	if (table == NULL) {
		return FALSE;
	}

	/* Insert the identifier properties into the hash table */
	if (ht_insert(table, id, prop) != EXIT_SUCCESS) {
		return FALSE;
	}

	if (IS_VARIABLE(prop->type)) {
		prop->offset = curr_offset;
		curr_offset++;
	}

	return TRUE;
}

Boolean find_name(char *id, IDPropt **prop)
{
	*prop = ht_search(table, id);
	if (!*prop && saved_table) {
		*prop = ht_search(saved_table, id);
		if (*prop && !IS_CALLABLE_TYPE((*prop)->type)) {
			*prop = NULL;
		}
	}

	return (*prop ? TRUE : FALSE);
}

int get_variables_width(void)
{
	return curr_offset;
}

void release_symbol_table(void)
{
	if (table != NULL) {
		ht_free(table, free, freeprop);
	}
}

void print_symbol_table(void)
{
	ht_print(table, valstr);
}

/* --- utility functions ---------------------------------------------------- */

static void valstr(void *key, void *p, char *str)
{
	char *keystr = (char *) key;
	IDPropt *idpp = (IDPropt *) p;

	if (IS_CALLABLE_TYPE(idpp->type)) {
		sprintf(str, "%s@_[%s]", keystr, get_valtype_string(idpp->type));
	} else {
		sprintf(str, "%s@%d[%s]", keystr, idpp->offset,
		        get_valtype_string(idpp->type));
	}
}

static void freeprop(void *p)
{
	IDPropt *idpp = (IDPropt *) p;
	free(idpp);
}

static unsigned int shift_hash(void *key, unsigned int size)
{
#ifdef DEBUG_SYMBOL_TABLE

	char *keystr = (char *) key;
	unsigned int i, hash, length;

	hash = 0;
	length = strlen(keystr);
	for (i = 0; i < length; i++) {
		hash += keystr[i];
	}

	return (hash % size);

#else

	char *keystr = (char *) key;
	unsigned int i, hash, length;
	hash = 0;
	length = strlen(keystr);
	for (i = 0; i < length; i++) {
		hash = (hash << 5) | (hash >> 27);
		hash += keystr[i];
	}
	return (hash % size);

#endif /* DEBUG_SYMBOL_TABLE */
}

static int key_strcmp(void *val1, void *val2)
{
	char *key1 = (char *) val1;
	char *key2 = (char *) val2;

	return strcmp(key1, key2);
}
