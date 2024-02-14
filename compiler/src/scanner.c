/**
 * @file    scanner.c
 * @brief   The scanner for AMPL-2023.
 * @author  W.H.K. Bester (whkbester@cs.sun.ac.za)
 * @date    2023-06-29
 */
/* --- #include libarys ----------------------------------------------------- */

#include "scanner.h"

#include "boolean.h"
#include "error.h"
#include "token.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- global static variables ---------------------------------------------- */

static FILE *src_file; /* the source file pointer             */
static int ch;         /* the next source character           */
static int cn;         /* the current column number           */
static int ln;         /* the current line number             */
SourcePos posit;

static struct {
	char *word;     /* the reserved word, i.e., the lexeme */
	TokenType type; /* the associated topen type           */
} reserved[] = {
{"and",TOK_AND},
{"array",TOK_ARRAY},
{"bool",TOK_BOOL},
{"chillax",TOK_CHILLAX},
{"elif",TOK_ELIF},
{"else",TOK_ELSE},
{"end",TOK_END},
{"false",TOK_FALSE},
{"if",TOK_IF},
{"input",TOK_INPUT},
{"int",TOK_INT},
{"let",TOK_LET},
{"main",TOK_MAIN},
{"not",TOK_NOT},
{"or",TOK_OR},
{"output",TOK_OUTPUT},
{"program",TOK_PROGRAM},
{"rem",TOK_REM},
{"return",TOK_RETURN},
{"true",TOK_TRUE},
{"while",TOK_WHILE}
};

#define NUM_RESERVED_WORDS (sizeof(reserved) / sizeof(reserved[0]))
#define MAX_INIT_STR_LEN   (1024)
#define true               1

/* --- function prototypes -------------------------------------------------- */

static void next_char(void);
static void process_number(Token *token);
static void process_string(Token *token);
static void process_word(Token *token);
static void skip_comment(void);

/* --- scanner interface ---------------------------------------------------- */
/**
 * Initialize the scanner with the given input file.
 * @param in_file The input file pointer.
 */
void init_scanner(FILE *in_file)
{
	src_file = in_file;
	position.line = ln = 1;
	position.col = cn = 0;
	next_char();
}
/**
 * Get the next token from the source code.
 * @param token The token structure to fill.
 */
void get_token(Token *token)
{
	SourcePos oldpos;
	int is_it = 0;

	/* remove whitespace */

	while (isspace(ch)) {
		if (ch == ' ' || ch == '\r' || ch == '\v' || ch == '\n' || ch == '\f') {
			next_char();
			if (ch == EOF) {
				position = posit;
				position.col++;
				is_it = 1;
			}
		} else if (ch == '\t') {
			next_char();
			position.col = position.col + 3;
		}
	}
	if (is_it == 0) {
		position.col = cn;
		position.line = ln;
	}

	/* get the next token */
	if (ch != EOF) {

		if (isalpha(ch) || ch == '_') {
			/* process a word */
			process_word(token);
		} else if (isdigit(ch)) {
			/* process a number */
			process_number(token);
		} else {
			switch (ch) { /* Switch for rest of the tokens */
				case '"':
					/* process a string */
					position.col = cn;
					oldpos = position;
					next_char();
					process_string(token);
					position = oldpos;
					break;
				case '{':
					skip_comment();
					next_char();
					get_token(token);
					break;
				case '=':
					token->type = TOK_EQ;
					token->string = "=";
					next_char();
					break;
				case '+':
					token->type = TOK_PLUS;
					token->string = "+";
					next_char();
					break;
				case '*':
					token->type = TOK_MUL;
					token->string = "+";
					next_char();
					break;
				case ':':
					token->type = TOK_COLON;
					token->string = ":";
					next_char();
					break;
				case ',':
					token->type = TOK_COMMA;
					token->string = ",";
					next_char();
					break;
				case '.':
					next_char();
					if (ch == '.') {
						token->type = TOK_DOTDOT;
						token->string = "..";
						next_char();
					} else {
						leprintf("illegal character '%c' (ASCII #%d)", '.', 46);
					}
					break;
				case '[':
					token->type = TOK_LBRACK;
					token->string = "[";
					next_char();
					break;
				case ']':
					token->type = TOK_RBRACK;
					token->string = "]";
					next_char();
					break;
				case '(':
					token->type = TOK_LPAREN;
					token->string = "(";
					next_char();
					break;
				case ')':
					token->type = TOK_RPAREN;
					token->string = ")";
					next_char();
					break;
				case ';':
					token->type = TOK_SEMICOLON;
					token->string = ";";
					next_char();
					break;
				case '>':
					next_char();
					if (ch == '=') {
						token->type = TOK_GE;
						token->string = ">=";
						next_char();
					} else {
						token->type = TOK_GT;
						token->string = ">";
					}
					break;
				case '<':
					next_char();
					if (ch == '=') {
						token->type = TOK_LE;
						token->string = "<=";
						next_char();
					} else {
						token->type = TOK_LT;
						token->string = "<";
					}
					break;
				case '/':
					next_char();
					if (ch == '=') {
						token->type = TOK_NE;
						token->string = "/=";
						next_char();
					} else {
						token->type = TOK_DIV;
						token->string = "/";
					}
					break;
				case '-':
					next_char();
					if (ch == '>') {
						token->type = TOK_ARROW;
						token->string = "->";
						next_char();
					} else {
						token->type = TOK_MINUS;
						token->string = "-";
					}
					break;
				default: /* illegal character */
					leprintf("illegal character '%c' (ASCII #%i)", ch, ch);
					break;
			}
		}
	} else {
		token->type = TOK_EOF;
	}
}

/* --- utility functions ---------------------------------------------------- */
/**
 * Advance to the next character in the input file.
 */
void next_char(void)
{
	ch = getc(src_file);

	if (ch == '\n') {
		posit.line = ln;
		posit.col = cn;
		ln++;
		cn = 0;
	} else {
		cn++;
	}
}

/**
 * Process numbers and assigns then to a token type.
 * @param token The token structure to fill.
 */
void process_number(Token *token)
{

	int value;
	int v_updated;
	int d;

	value = 0;
	d = ch - '0';

	/* builds number */
	while (true) {
		v_updated = (10 * value) + d;
		/* Number is too long */
		if (v_updated < 0 || v_updated > INT_MAX) {
			leprintf("number too large");
			break;
		}
		value = v_updated;
		next_char();
		/* End of Number */
		if (!isdigit(ch)) {
			break;
		}
		d = ch - '0';
	}

	token->type = TOK_NUM;
	token->value = value;
}
/**
 * Process string's and stores them in the updated_string varible.
 * @param token pointer to the token structure to fill.
 */
void process_string(Token *token)
{

	size_t i, nstring = MAX_INIT_STR_LEN;
	int j;
	char *current_string = malloc(nstring);
	char *updated_string = current_string;
	/* clear the updated string for next string */
	for (j = 0; j < MAX_INIT_STR_LEN; j++) {
		updated_string[j] = '\0';
	}
	updated_string[MAX_INIT_STR_LEN] = '\0';

	int temp_col_number;
	int temp_line_number;

	SourcePos ip = position;

	i = 0;
	while (ch != '"') {

		/* If ch is a non-printibale character */
		if ((isascii(ch) == 0 || ch < 32) && ch != 10 && ch != -1) {
			leprintf("non-printable character (ASCII #%i) in string", ch);
		} else if (ch == 10) {
			position.line = temp_line_number;
			position.col = temp_col_number + 1;
			leprintf("non-printable character (ASCII #%i) in string", 10);
		}

		/* For a '\' variants */
		if (ch == 92) {
			updated_string[i] = '\\';
			i++;
			next_char();
			switch (ch) {
				case 'n':
				case 't':
					break;
				case 34:
					ch = '"';
					break;
				case 92:
					ch = '\\';
					break;
				default:
					position.col = cn - 1;
					position.line = ln;
					leprintf("illegal escape code '%c%c' in string", '\\', ch);
					break;
			}
		}

		/* Doubles the size of the updated_string */
		if (i == nstring) {
			nstring *= 2;
			updated_string = realloc(updated_string, nstring);
		}

		temp_line_number = ln;
		temp_col_number = cn;
		updated_string[i++] = ch;

		next_char();

		if (ch == EOF) {
			position = ip;
			leprintf("string not closed");
			break;
		} else { /* Updates the line and column numbers */
			position.col = cn;
			position.line = ln;
		}
	}
	token->type = TOK_STR;
	token->string = updated_string;

	next_char();
}
/**
 * Process words and checks if they are reserved words or identifiers.
 * @param token pointer to the token structure to fill.
 */
void process_word(Token *token)
{
	char lexeme[MAX_ID_LEN+1];
	int cmp, lowest, middle, highest;
	position.col = cn;

	/* Fills array with 0 and a \0 at the end */
	memset(lexeme, 0, MAX_ID_LEN);
	lexeme[MAX_ID_LEN] = '\0';

	/* check that the id length is less than the maximum and
	   populates the lexeme[] array */
	int x = 0;
	do {
		if (x >= MAX_ID_LEN) {
			leprintf("identifier too long");
			break;
		}

		lexeme[x] = ch;
		next_char();
		x++;

	} while ((ch >= 48 && ch <= 57) || isalpha(ch) || ch == '_');

	/* do a binary search through the array of reserved words */
	lowest = 0;
	middle = NUM_RESERVED_WORDS / 2;
	highest = NUM_RESERVED_WORDS;
	int token_found = 0;

	while (lowest < highest) {

		middle = lowest + (highest - lowest) / 2;
		cmp = strcmp(lexeme, reserved[middle].word);

		/* If Lexeme is a reserved words */
		if (cmp == 0) {
			token->type = reserved[middle].type;
			strcpy(token->lexeme, lexeme);
			token_found = 1;
			break;
		} else if (cmp < 0) {
			highest = middle;
		} else {
			lowest = middle + 1;
		}
	}
	/* Otherwise Lexeme is an ID */
	if (token_found == 0) {
		token->type = TOK_ID;
		strcpy(token->lexeme, lexeme);
	}
}

/**
 * Skip comments in the input file.
 */
void skip_comment(void)
{
	SourcePos start_pos;

	start_pos.line = ln;
	start_pos.col = cn;

	next_char();

	/* Recursivly skips nested comments */
	while (true) {
		if (ch == EOF) {
			position = start_pos;
			leprintf("comment not closed");
			break;
		} else if (ch == '{') {
			skip_comment();
			next_char();
		} else if (ch == '}') {
			return;
		} else {
			next_char();
		}
	}
}
