
#include "hashtable.h"

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_DELTA_INDEX 4
#define PRINT_BUFFER_SIZE   1024

/** an entry in the hash table */
typedef struct htentry HTentry;
struct htentry {
	void *key;         /*<< the key                      */
	void *value;       /*<< the value                    */
	HTentry *next_ptr; /*<< the next entry in the bucket */
};

/** a hash table container */
struct hashtab {
	/** a pointer to the underlying table                              */
	HTentry **table;
	/** the current size of the underlying table                       */
	unsigned int size;
	/** the current number of entries                                  */
	unsigned int num_entries;
	/** the maximum load factor before the underlying table is resized */
	float max_loadfactor;
	/** the index into the delta array                                 */
	unsigned short idx;
	/** a pointer to the hash function                                 */
	unsigned int (*hash)(void *, unsigned int);
	/** a pointer to the comparison function                           */
	int (*cmp)(void *, void *);
};

/* --- function prototypes -------------------------------------------------- */

static int getsize(HashTab *ht);
static HTentry **talloc(int tsize);
static void rehash(HashTab *ht);

/**
 * the array of differences between a power-of-two and the largest prime less
 * than that power-of-two.
 */
unsigned short delta[] = {0,  0, 1, 1, 3, 1, 3, 1,  5, 3,  3, 9,  3,  1, 3, \
19, 15, 1, 5, 1, 3, 9, 3, 15, 3, 39, 5, 39, 57, 3, 35, 1};

#define MAX_IDX (sizeof(delta) / sizeof(short))

/* --- hash table interface ------------------------------------------------- */

HashTab *ht_init(float loadfactor, \
unsigned int (*hash)(void *key, unsigned int size), \
int (*cmp)(void *val1, void *val2))
{
	HashTab *ht;

	ht = (HashTab *) malloc(sizeof(HashTab));

	if (ht == NULL) {
		return NULL;
	}

	ht->size = 13;
	ht->num_entries = 0;
	ht->idx = INITIAL_DELTA_INDEX;
	ht->max_loadfactor = loadfactor;
	ht->hash = hash;
	ht->cmp = cmp;

	size_t starter_table_size = 1 << INITIAL_DELTA_INDEX;
	ht->table = talloc(starter_table_size);

	if (ht->table == NULL) {
		free(ht);
		return NULL;
	} else {
		return ht;
	}
}

int ht_insert(HashTab *ht, void *key, void *value)
{
	if (ht == NULL || key == NULL || value == NULL) {
		return EXIT_FAILURE;
	}

	int hash_val = ht->hash(key, ht->size);

	HTentry *entering_entry = malloc(sizeof(*entering_entry));

	if (!entering_entry) {
		return EXIT_FAILURE;
	}
	entering_entry->key = key;
	entering_entry->value = value;
	entering_entry->next_ptr = NULL;

	float potential_new_loadfactor =
	    (float) (ht->num_entries + 1) / (float) ht->size;

	if (potential_new_loadfactor > ht->max_loadfactor) {
		rehash(ht);

		hash_val = ht->hash(key, ht->size);
	}

	HTentry *present_entry = ht->table[hash_val];

	while (present_entry != NULL) {
		if (ht->cmp(key, present_entry->key) == 0) {
			free(entering_entry);
			return HASH_TABLE_KEY_VALUE_PAIR_EXISTS;
		}
		present_entry = present_entry->next_ptr;
	}

	entering_entry->next_ptr = ht->table[hash_val];
	ht->table[hash_val] = entering_entry;
	ht->num_entries++;

	return EXIT_SUCCESS;
}

void *ht_search(HashTab *ht, void *key)
{
	int k;
	HTentry *p;

	if (!(ht && key)) {
		return NULL;
	}

	k = ht->hash(key, ht->size);
	for (p = ht->table[k]; p; p = p->next_ptr) {
		if (ht->cmp(key, p->key) == 0) {
			return p->value;
		}
	}

	return NULL;
}

int ht_free(HashTab *ht, void (*freekey)(void *k), void (*freeval)(void *v))
{
	unsigned int i;
	HTentry *p, *q;

	if (!(ht && freekey && freeval)) {
		return EXIT_FAILURE;
	}

	for (i = 0; i < ht->size; i++) {
		p = ht->table[i];
		while (p != NULL) {
			q = p->next_ptr;
			if (freekey) {
				freekey(p->key);
			}
			if (freeval) {
				freeval(p->value);
			}
			free(p);
			p = q;
		}
	}
	free(ht->table);
	free(ht);

	return EXIT_SUCCESS;
}

void ht_print(HashTab *ht, void (*keyval2str)(void *k, void *v, char *b))
{
	unsigned int i;
	HTentry *p;
	char buffer[PRINT_BUFFER_SIZE];

	if (ht && keyval2str) {
		for (i = 0; i < ht->size; i++) {
			printf("bucket[%2i]", i);
			for (p = ht->table[i]; p != NULL; p = p->next_ptr) {
				keyval2str(p->key, p->value, buffer);
				printf(" --> %s", buffer);
			}
			printf(" --> NULL\n");
		}
	}
}

void keyval2str(void *k, void *v, char *b)
{
	char *key_str = (char *) k;
	char *value_str = (char *) v;

	snprintf(b, PRINT_BUFFER_SIZE, "%s:[%s]", key_str, value_str);
}

/* --- utility functions ---------------------------------------------------- */

static int getsize(HashTab *ht)
{
	int size_now, newest_size, pow, number;
	size_now = ht->size;
	newest_size = 0;
	pow = 1;
	number = 2;
	while (number < size_now) {
		number *= 2;
		pow++;
	}
	pow++;
	newest_size = number * 2 - delta[pow];

	return newest_size;
}

static HTentry **talloc(int tsize)
{
	HTentry **future_table = malloc(tsize * sizeof(HTentry *));

	if (future_table == NULL) {
		fprintf(stderr, "Memory allocation failed in talloc\n");
		return NULL;
	}

	for (int i = 0; i < tsize; i++) {
		future_table[i] = NULL;
	}

	return future_table;
}

static void rehash(HashTab *ht)
{
	int updated_size = getsize(ht);
	HTentry **future_table = malloc(updated_size * sizeof(HTentry *));

	if (future_table == NULL) {
		/* Handle memory allocation failure */
		return;
	}

	int i;
	for (i = 0; i < updated_size; i++) {
		future_table[i] = NULL;
	}
	unsigned int j;
	for (j = 0; j < ht->size; j++) {
		HTentry *current_ent = ht->table[j];
		while (current_ent != NULL) {
			unsigned int new_hash_val =
			    ht->hash(current_ent->key, updated_size);
			HTentry *newest_ent = malloc(sizeof(HTentry));
			if (newest_ent == NULL) {
				return;
			}
			newest_ent->key = current_ent->key;
			newest_ent->value = current_ent->value;
			newest_ent->next_ptr = future_table[new_hash_val];
			future_table[new_hash_val] = newest_ent;
			current_ent = current_ent->next_ptr;
		}
	}
	free(ht->table);
	ht->table = future_table;
	ht->size = updated_size;
}
