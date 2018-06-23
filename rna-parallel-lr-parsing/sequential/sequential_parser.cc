#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "stack.h"

//#define ALGOTIME

int parse_success_count=0, parse_failed_count=0;
stackT parse_tree;

struct Rules{
	char *left;
	char *right;
	float prob;
};

static struct Rules rt[] = {	{"S","LS",0.40},
						{"S","", 0.60},
						{"L","a",0.18},
						{"L","c",0.09},
						{"L","g",0.10},
						{"L","u",0.17},
						{"L","aSu",0.07},
						{"L","uSa",0.09},
						{"L","cSg",0.14},
						{"L","gSc",0.11},
						{"L","gSu",0.02},
						{"L","uSg",0.03}
					};

struct Actions{
	char *list[5];
};

static char *possibleActions = "acgu$";

struct Goto{
	int list[2];
};

static char *possibleGoto = "SL";

struct parsingTable{
	struct Actions actions;
	struct Goto vgoto;
};

static struct parsingTable pt[] = 	{	{{"s4 r2","s5 r2","s6 r2","s7 r2","r2"},{2,3}},
										{{"null","null","null","null","accept"},{0,0}},
										{{"s4 r2","s5 r2","s6 r2","s7 r2","r2"},{8,3}},
										{{"s4 r2 r3","s5 r2 r3","s6 r2 r3","s7 r2 r3","r2 r3"},{9,3}},
										{{"s4 r2 r4","s5 r2 r4","s6 r2 r4","s7 r2 r4","r2 r4"},{10,3}},
										{{"s4 r2 r5","s5 r2 r5","s6 r2 r5","s7 r2 r5","r2 r5"},{11,3}},
										{{"s4 r2 r6","s5 r2 r6","s6 r2 r6","s7 r2 r6","r2 r6"},{12,3}},
										{{"r1","r1","r1","r1","r1"},{0,0}},
										{{"null","null","null","s13","null"},{0,0}},
										{{"null","null","s14","null","null"},{0,0}},
										{{"null","s15","null","s16","null"},{0,0}},
										{{"s17","null","s18","null","null"},{0,0}},
										{{"r7","r7","r7","r7","r7"},{0,0}},
										{{"r9","r9","r9","r9","r9"},{0,0}},
										{{"r10","r10","r10","r10","r10"},{0,0}},
										{{"r11","r11","r11","r11","r11"},{0,0}},
										{{"r8","r8","r8","r8","r8"},{0,0}},
										{{"r12","r12","r12","r12","r12"},{0,0}}
									};

int strpos(char *s, char c)
{
	char *sp;
	sp = strchr(s, c);
	return strlen(s)-strlen(sp);
}

void parse_error(char *s)
{
	printf("parsing error: %s\n", s);
}

void shift(stackT *stk, int nextState, char data)
{
	char ch;
	StackPush(stk, data);
//	sprintf(&ch, "%d", nextState);
	StackPush(stk, nextState);
#ifdef DEBUG
	printf("current stack top = %d: ", stk->top);
	for(int a=0;a<=stk->top;a++)
		printf("%c(%d) ", stk->contents[a], stk->contents[a]);
	printf("\n");
#endif
}

void reduce(stackT *stk, int ruleNumber, int gt)
{
	int ch_to_pop = strlen(rt[ruleNumber].right) * 2;
	char ch;
	int nextState;

#ifdef DEBUG
	printf("reduce function: rule number %d, nextState = %d, char to pop = %d\n", ruleNumber, gt, ch_to_pop);
#endif

	for(int a=0;a<ch_to_pop;a++)
		ch = StackPop(stk);

	ch = StackPop(stk);
	StackPush(stk, ch);
	nextState = (int)ch-1;
	StackPush(stk, rt[ruleNumber].left[0]);
//	sprintf(&ch, "%d", gt);
	StackPush(stk, pt[nextState].vgoto.list[gt]);
	StackPush(&parse_tree, ruleNumber);
#ifdef DEBUG
	printf("putting parse tree rule %d\n", ruleNumber);
	printf("current stack top = %d: ", stk->top);
	for(int a=0;a<=stk->top;a++)
		printf("%c(%d) ", stk->contents[a], stk->contents[a]);
	printf("\n");
#endif
}

int main(int argc, char *argv[])
{
	stackT stack;
	stackT bt_stack; //backtracking stack
	int action_index, state_index, goto_index, rn;
	int actions_to_skip = 0;
	int parse_tree_rules_count = 0;
	double max_prob = 0.0;
	char ch;
	char *current_action;
	char *dump_action;
	int current_ch=0;
	int backtrack=0;
	char str[10];
	int elements_in_stack;
	int max_prob_parse_tree[BUFSIZ];
	int r;
	double dtime, algo_time=0.0;
	int ssec, esec, susec, eusec;
	struct timeval tv;
	int action_push_count = 0;
	
	if(argc <= 1){
		printf("No String to parse\n");
		exit(0);
	}

	if(strchr(argv[1], '$') == NULL){
		parse_error("input string should be terminated by $ sign");
		exit(1);
	}
		
	for(int a=0;a<strlen(argv[1]);a++)
		if(strchr(possibleActions,argv[1][a]) == NULL){
			printf("error: incorrect character sequence\n");
			exit(1);
		}

	StackInit(&stack, BUFSIZ+BUFSIZ);
	StackInit(&bt_stack, BUFSIZ+BUFSIZ);
	StackInit(&parse_tree, BUFSIZ+BUFSIZ);
	StackPush(&stack, 1);

	gettimeofday(&tv, NULL);
	ssec = tv.tv_sec;
	susec = tv.tv_usec;
	
	while(1){
		if(backtrack == 0){
			ch = StackPop(&stack);
			StackPush(&stack, ch);
			state_index = (int)ch-1;
			if(state_index < 0){
				if(!StackIsEmpty(&bt_stack)){
					backtrack = 1;
					parse_failed_count++;
					StackFlush(&parse_tree);
					continue;
				}
				else{
					StackDestroy(&stack);
					StackDestroy(&bt_stack);
					parse_failed_count++;
					StackFlush(&parse_tree);
#ifdef DEBUG
					parse_error("incorrect state");
#endif
					break;
				}
			}
			action_index = strpos(possibleActions,argv[1][current_ch]);
			strcpy(str, pt[state_index].actions.list[action_index]);
			current_action = strtok(str, " ");
			dump_action = strtok(NULL, " ");
			actions_to_skip = 0;
			while(dump_action != NULL){
				actions_to_skip++;
				StackPush(&bt_stack, state_index);
				StackPush(&bt_stack, current_ch);
				StackPush(&bt_stack, actions_to_skip);
				elements_in_stack = stack.top + 1;
				for(int a=elements_in_stack-1;a>=0;a--)
					StackPush(&bt_stack, stack.contents[a]);
				StackPush(&bt_stack, elements_in_stack);
				elements_in_stack = parse_tree.top + 1;
				for(int a=elements_in_stack-1;a>=0;a--)
					StackPush(&bt_stack, parse_tree.contents[a]);
				StackPush(&bt_stack, elements_in_stack);
				action_push_count++;
#ifdef DEBUG
				printf("backtrack stack top = %d: ", bt_stack.top);
				for(int a=0;a<=bt_stack.top;a++)
					printf("%c(%d) ", bt_stack.contents[a], bt_stack.contents[a]);
				printf("\n");
#endif
				dump_action = strtok(NULL, " ");
			}
		}
		else{
#ifdef DEBUG
			printf("backtracking...\n");
#endif
			backtrack = 0;
			elements_in_stack = StackPop(&bt_stack);
			for(int a=0;a<elements_in_stack;a++)
				parse_tree.contents[a] = StackPop(&bt_stack);
			parse_tree.top = elements_in_stack - 1;
			elements_in_stack = StackPop(&bt_stack);
			for(int a=0;a<elements_in_stack;a++)
				stack.contents[a] = StackPop(&bt_stack);
			stack.top = elements_in_stack - 1;
#ifdef DEBUG
			printf("current stack top = %d: ", stack.top);
			for(int a=0;a<=stack.top;a++)
				printf("%c(%d) ", stack.contents[a], stack.contents[a]);
			printf("\n");
#endif
			actions_to_skip = StackPop(&bt_stack);
			current_ch = StackPop(&bt_stack);
			state_index = StackPop(&bt_stack);
			action_index = strpos(possibleActions,argv[1][current_ch]);
			strcpy(str, pt[state_index].actions.list[action_index]);
			current_action = strtok(str, " ");
			for(int a=0;a<actions_to_skip;a++){
				if(current_action == NULL) parse_error("find_action");
				current_action = strtok(NULL, " ");
			}
		}
/*
		printf("<%d>: ", current_ch / (strlen(argv[1])/2));
		for(int a=0; a <= stack.top; a++)
			printf("%c(%d) ", stack.contents[a], stack.contents[a]);
		printf("\n");
		printf("state index = %d, action index = %d, current action = %s\n", state_index, action_index, current_action);
		fflush(stdout);
*/
#ifdef DEBUG
		printf("state index = %d, action index = %d, current action = %s\n", state_index, action_index, current_action);
		fflush(stdout);
#endif
#ifdef ALGOTIME
		gettimeofday(&tv, NULL);
		ssec = tv.tv_sec;
		susec = tv.tv_usec;
#endif
		if(current_action[0] == 's'){ //shift operation
			shift(&stack, atoi(&current_action[1]), argv[1][current_ch]);
			current_ch++;
		}
		else if(current_action[0] == 'r'){
			rn = atoi(&current_action[1])-1;
			goto_index = strpos(possibleGoto, rt[rn].left[0]);
#ifdef DEBUG
		printf("rule number = %d, Goto = %d\n", rn, goto_index);
#endif
			reduce(&stack, rn, goto_index);
		}
		else if(strcmp(current_action, "accept") == 0){
			parse_success_count++;
			int parse_list[BUFSIZ];
			double prob = 1.0;
			int parse_tree_counters = parse_tree.top + 1;
			for(int a=parse_tree.top, counter=0;a>-1;a--){
				r = (int)StackPop(&parse_tree);
				parse_list[counter++] = r;
				prob *= rt[r].prob;
			}
			
			if(prob > max_prob){
				for(int a=0;a<parse_tree_counters;a++)
					max_prob_parse_tree[a] = parse_list[a];
				max_prob = prob;
				parse_tree_rules_count = parse_tree_counters;
			}
/*
			printf("Stack And Parse Tree\n");
			printf("====================\n");
			for(int a=0; a <= stack.top; a++)
				printf("%c(%d) ", stack.contents[a], stack.contents[a]);
			printf("\n");
			printf("tree: ");
			for(int a=0; a < parse_tree_counters; a++)
				printf("%d ", parse_list[a]);
			printf("\n");
*/
			StackFlush(&parse_tree);
			if(StackIsEmpty(&bt_stack))
				break;
			else
				backtrack = 1;
		}
		else if(!StackIsEmpty(&bt_stack)){
			backtrack = 1;
			parse_failed_count++;
			StackFlush(&parse_tree);
		}
		else{
			StackDestroy(&stack);
			StackDestroy(&bt_stack);
			parse_failed_count++;
			StackFlush(&parse_tree);
#ifdef DEBUG
			parse_error("no action");
#endif
			break;
		}
#ifdef ALGOTIME
		gettimeofday(&tv, NULL);
		esec = tv.tv_sec;
		eusec = tv.tv_usec;

		algo_time += ((esec * 1.0) + ((eusec * 1.0)/1000000.0)) - ((ssec * 1.0) + ((susec * 1.0)/1000000.0));
#endif
	}
	
	gettimeofday(&tv, NULL);
	esec = tv.tv_sec;
	eusec = tv.tv_usec;

	dtime = ((esec * 1.0) + ((eusec * 1.0)/1000000.0)) - ((ssec * 1.0) + ((susec * 1.0)/1000000.0));
	
#ifdef DEBUG	
		for(int a=stack.top;a>-1;a--){
			ch = StackPop(&stack);
			printf("%c(%d)\n", ch, ch);
		}
#endif

	printf("Parse Tree\n");
	printf("----------\n");
	for(int a=0;a<parse_tree_rules_count;a++){
		r = max_prob_parse_tree[a];		
		printf("%s -> %s\n", rt[r].left, rt[r].right);
	}
	printf("\nMax. Probability = %g\n\n", max_prob);
	printf("Parse Success = %d\n", parse_success_count);
	printf("Parse Failed = %d\n", parse_failed_count);
	printf("Parsing Time = %g seconds\n", dtime);

#ifdef ALGOTIME
	printf("Core Algorithm Time = %g seconds\n", algo_time);
#endif

	StackDestroy(&stack);
	StackDestroy(&bt_stack);
	StackDestroy(&parse_tree);

	return 0;
}
