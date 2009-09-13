%{
#include <stdio.h>
#include <string.h>
#include <xcb/xcb.h>

#include "data.h"

extern int yylex(void);
extern FILE *yyin;

int yydebug = 1;

void yyerror(const char *str) {
        fprintf(stderr,"error: %s\n",str);
}

int yywrap() {
        return 1;
}

void parse_file(const char *f) {
	printf("opening %s\n", f);
        if ((yyin = fopen(f, "r")) == NULL) {
		perror("fopen");
		exit(1);
	}
	if (yyparse() != 0) {
		fprintf(stderr, "Could not parse configfile\n");
		exit(1);
	}
	fclose(yyin);
}

#if 0
main()
{
        yyparse();
	printf("parsing done\n");
}
#endif

%}

%union {
	int number;
	char *string;
}

%token <number>NUMBER
%token <string>WORD
%token <string>STR
%token <string>STR_NG
%token <string>VARNAME
%token <string>HEX
%token TOKBIND
%token TOKTERMINAL
%token TOKCOMMENT
%token TOKFONT
%token TOKBINDSYM
%token MODIFIER
%token TOKCONTROL
%token TOKSHIFT
%token WHITESPACE
%token TOKFLOATING_MODIFIER
%token QUOTEDSTRING
%token TOKWORKSPACE
%token TOKSCREEN
%token TOKASSIGN
%token TOKSET
%token TOKIPCSOCKET
%token TOKEXEC
%token TOKCOLOR
%token TOKARROW

%%

lines: /* empty */
        | lines WHITESPACE line
	| lines line
        ;

line:
        bind
	| bindsym
	| floating_modifier
	| workspace
	| assign
	| set
	| ipcsocket
	| exec
	| color
	| terminal
	| font
	| comment
        ;

comment:
	TOKCOMMENT
	;

command:
	STR
	;

bind:
        TOKBIND WHITESPACE binding_modifiers '+' NUMBER WHITESPACE command
        {
                printf("\tFound binding mod%d with key %d and command %s\n", $<number>3, $5, $<string>7);
        }
        ;

bindsym:
        TOKBINDSYM WHITESPACE binding_modifiers '+' WORD WHITESPACE command
        {
                printf("\tFound symbolic mod%d with key %s and command %s\n", $<number>3, $5, $<string>7);
        }
        ;

floating_modifier:
	TOKFLOATING_MODIFIER WHITESPACE binding_modifiers
	{
		printf("\tfloating modifier %d\n", $<number>3);
	}
	;

workspace:
	TOKWORKSPACE WHITESPACE NUMBER WHITESPACE TOKSCREEN WHITESPACE screen
	{
		printf("\t workspace %d to screen %d\n", $<number>3, $<number>7);
	}
	| TOKWORKSPACE WHITESPACE NUMBER WHITESPACE TOKSCREEN WHITESPACE screen WHITESPACE QUOTEDSTRING
	{
		printf("\t quoted: %s\n", $<string>9);
	}
	;

screen:
	NUMBER
	;


assign:
	TOKASSIGN WHITESPACE window_class WHITESPACE optional_arrow NUMBER
	{
		printf("assignment of %s to %d\n", $<string>3, $<number>6);
	}
	;

window_class:
	QUOTEDSTRING
	| STR_NG
	;

optional_arrow:
	/* NULL */
	| TOKARROW WHITESPACE
	;

set:
	TOKSET WHITESPACE variable WHITESPACE STR
	{
		printf("set %s to %s\n", $<string>3, $<string>5);
	}
	;

variable:
	'$' WORD 	{ asprintf(&$<string>$, "$%s", $<string>2); }
	| '$' VARNAME 	{ asprintf(&$<string>$, "$%s", $<string>2); }
	;

ipcsocket:
	TOKIPCSOCKET WHITESPACE STR
	{
		printf("ipc %s\n", $<string>3);
	}
	;

exec:
	TOKEXEC WHITESPACE STR
	{
		printf("exec %s\n", $<string>3);
	}
	;

terminal:
	TOKTERMINAL WHITESPACE STR
	{
		printf("terminal %s\n", $<string>3);
	}
	;

font:
	TOKFONT WHITESPACE STR
	{
		printf("font %s\n", $<string>3);
	}
	;


color:
	TOKCOLOR WHITESPACE '#' HEX
	{
		printf("color %s\n", $<string>4);
	}
	;


binding_modifiers:
	binding_modifier
	|
	binding_modifiers '+' binding_modifier
	{
		$<number>$ = $<number>1 | $<number>3;
	}
	;

binding_modifier:
	MODIFIER 	{ $<number>$ = $<number>1; }
	| TOKCONTROL 	{ $<number>$ = BIND_CONTROL; }
	| TOKSHIFT 	{ $<number>$ = BIND_SHIFT; }
	;
