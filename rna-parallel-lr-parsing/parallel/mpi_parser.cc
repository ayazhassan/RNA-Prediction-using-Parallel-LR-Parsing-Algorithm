#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "stack.h"

//#define ALGOTIME
//#define MPI_DEBUG
//#define DEBUG
#define MULTIPLE_MASTER_PROCESSES_LOGIC

#define MSG_ACTION_COUNT 0
#define MSG_ACTION_DATA 1

unsigned long long int parse_success_count=0, parse_failed_count=0;
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
	double prob;
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
	double maxdtime;
	int ssec, esec, susec, eusec;
	struct timeval tv;
	int action_push_count = 1;
	int myrank, mysize;
	int sent_action = 0;
	MPI_Status status;
	char snd_data[BUFSIZ];
	unsigned long long int total_parse_success_count, 
total_parse_failed_count;
	int master_processes = 1;
	
	if(argc <= 1){
		printf("No String to parse\n");
		exit(0);
	}

	if(strchr(argv[1], '$') == NULL){
		parse_error("input string should be terminated by $ sign");
		exit(1);
	}
	
	if(argc >= 3){
		master_processes = atoi(argv[2]);
	}
#ifdef MULTIPLE_MASTER_PROCESSES_LOGIC
	//printf("Master Processes = %d\n", master_processes);
#endif		
	for(int a=0;a<strlen(argv[1]);a++)
		if(strchr(possibleActions,argv[1][a]) == NULL){
			printf("error: incorrect character sequence\n");
			exit(1);
		}

	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
	MPI_Comm_size(MPI_COMM_WORLD,&mysize);

	int tasks_counter = 0;
//	for(int a=0; a < mysize; a++)
//		tasks_count[a] = 0;
	
	StackInit(&stack, 40 * BUFSIZ);
	StackInit(&bt_stack, 100000 * BUFSIZ);
	StackInit(&parse_tree, 40 * BUFSIZ);

	if(myrank == 0)
		StackPush(&stack, 1);
	else{
		do{
#ifdef MPI_DEBUG
		printf("<%d>: waiting for action count\n", myrank);
#endif
#ifdef MULTIPLE_MASTER_PROCESSES_LOGIC
		MPI_Recv(&action_push_count, 1, MPI_INT, MPI_ANY_SOURCE, MSG_ACTION_COUNT, MPI_COMM_WORLD, &status);
#else
		MPI_Recv(&action_push_count, 1, MPI_INT, 0, MSG_ACTION_COUNT, MPI_COMM_WORLD, &status);
#endif
#ifdef MPI_DEBUG
		printf("<%d>: received action count = %d from node %d\n", myrank, action_push_count, status.MPI_SOURCE);
#endif
		if(action_push_count > 0){
#ifdef MULTIPLE_MASTER_PROCESSES_LOGIC
			MPI_Recv(&bt_stack.contents[bt_stack.top+1], action_push_count, MPI_CHAR, status.MPI_SOURCE, MSG_ACTION_DATA, MPI_COMM_WORLD, &status);
#else
			MPI_Recv(&bt_stack.contents[bt_stack.top+1], action_push_count, MPI_CHAR, 0, MSG_ACTION_DATA, MPI_COMM_WORLD, &status);
#endif
			bt_stack.top += action_push_count;
#ifdef MPI_DEBUG
		printf("<%d>: action received from node %d\n", myrank, status.MPI_SOURCE);
#endif
			backtrack = 1;
			tasks_counter++;
		}
		}while(action_push_count > 0);
	}

	if(myrank == 0)		
		printf("Rank, Received Tasks, Parse Success, Parse Failure, Execution Time\n");
	
	gettimeofday(&tv, NULL);
	ssec = tv.tv_sec;
	susec = tv.tv_usec;

	if(myrank == 0 || backtrack == 1)
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
#ifdef MULTIPLE_MASTER_PROCESSES_LOGIC
				if(myrank < master_processes /* && state_index > 2 && sent_action < mysize - 1 */){
#else
				if(myrank == 0 /* && state_index > 2 && sent_action < mysize - 1 */){
#endif
					sent_action++;
					if(myrank + sent_action >= mysize) sent_action = 1;
					action_push_count = 0;
					snd_data[action_push_count++] = state_index;
					snd_data[action_push_count++] = current_ch;
					snd_data[action_push_count++] = actions_to_skip;
					elements_in_stack = stack.top + 1;
					for(int a=elements_in_stack-1; a>=0; a--)
						snd_data[action_push_count++] = stack.contents[a];
					snd_data[action_push_count++] = elements_in_stack;
					elements_in_stack = parse_tree.top + 1;
					for(int a=elements_in_stack-1; a>=0; a--)
						snd_data[action_push_count++] = parse_tree.contents[a];
					snd_data[action_push_count++] = elements_in_stack;
					
#ifdef MPI_DEBUG
		printf("<%d>: sending action count = %d to node %d\n", myrank, action_push_count, myrank + sent_action);
#endif
					MPI_Send(&action_push_count, 1, MPI_INT, myrank + sent_action, MSG_ACTION_COUNT, MPI_COMM_WORLD);
#ifdef MPI_DEBUG
		printf("<%d>: sending action to node %d\n", myrank, myrank+sent_action);
#endif
					MPI_Send(&snd_data[0], action_push_count, MPI_CHAR, myrank + sent_action, MSG_ACTION_DATA, MPI_COMM_WORLD);
#ifdef MPI_DEBUG
		printf("<%d>: action sent to node %d\n", myrank, myrank + sent_action);
#endif
					// printf("<%d>: current send stack top = %d: ", myrank, action_push_count-1);
					// for(int a=0;a<action_push_count;a++)
						// printf("%c(%d) ", snd_data[a], snd_data[a]);
					// printf("\n");
//					tasks_count[sent_action]++;
				}
				else{
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
	#ifdef DEBUG
					printf("<%d>: backtrack stack top = %d: ", myrank, bt_stack.top);
					for(int a=0;a<=bt_stack.top;a++)
						printf("%c(%d) ", bt_stack.contents[a], bt_stack.contents[a]);
					printf("\n");
	#endif
				}
				dump_action = strtok(NULL, " ");
			}
		}
		else{
#ifdef DEBUG
			printf("<%d>: backtracking...\n", myrank);
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
			printf("<%d>: current stack top = %d: ", myrank, stack.top);
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
		
#ifdef DEBUG
		printf("<%d>: state index = %d, action index = %d, current action = %s, actions to skip = %d\n", myrank, state_index, action_index, current_action, actions_to_skip);
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
		printf("<%d>: rule number = %d, Goto = %d\n", myrank, rn, goto_index);
#endif
			reduce(&stack, rn, goto_index);
		}
		else if(strcmp(current_action, "accept") == 0){
			parse_success_count++;
			int parse_list[BUFSIZ];
			prob = 1.0;
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

//			printf("<%d> parse success count = %d\n", myrank, parse_success_count);
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

#ifdef MULTIPLE_MASTER_PROCESSES_LOGIC
	action_push_count = 0;
	if(myrank < master_processes - 1)
		MPI_Send(&action_push_count, 1, MPI_INT, myrank + 1, MSG_ACTION_COUNT, MPI_COMM_WORLD);
	else if(myrank == master_processes - 1){
		sent_action = myrank + 1;
		while(sent_action < mysize){
			MPI_Send(&action_push_count, 1, MPI_INT, sent_action, MSG_ACTION_COUNT, MPI_COMM_WORLD);
			sent_action++;
		}
	}
#else
	if(myrank == 0){
		sent_action = 1;
		while(sent_action < mysize){
			action_push_count = 0;
			MPI_Send(&action_push_count, 1, MPI_INT, myrank + sent_action, MSG_ACTION_COUNT, MPI_COMM_WORLD);
			sent_action++;
		}
	}
#endif
	printf("<%d>, %d, %llu, %llu, %g\n", myrank, tasks_counter, parse_success_count, parse_failed_count, dtime);
		//printf("<%d>: waiting for results\n", myrank);
#ifdef MPI_DEBUG
		printf("<%d>: waiting for results\n", myrank);
#endif
	MPI_Reduce(&dtime, &maxdtime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
	MPI_Reduce(&parse_success_count, &total_parse_success_count, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	MPI_Reduce(&parse_failed_count, &total_parse_failed_count, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	//printf("<%d>: results collected\n", myrank);
	
#ifdef MPI_DEBUG
		printf("<%d>: results collected\n", myrank);
#endif
#ifdef MPI_DEBUG
		printf("<%d>: waiting for max probability\n", myrank);
#endif
	
	MPI_Allreduce(&max_prob, &prob, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
#ifdef MPI_DEBUG
		printf("<%d>: received max prob = %f, local max prob = %f\n", myrank, prob, max_prob);
#endif
	
	if(max_prob == prob){

		printf("Parse Tree (Generated at node %d)\n", myrank);
		printf("---------------------------------\n");
		for(int a=0;a<parse_tree_rules_count;a++){
			r = max_prob_parse_tree[a];		
			printf("%s -> %s\n", rt[r].left, rt[r].right);
		}

		printf("\nMax. Probability = %g\n\n", max_prob);
	}
	
	if(myrank == 0){
		printf("Parse Success = %llu\n", total_parse_success_count);
		printf("Parse Failed = %llu\n", total_parse_failed_count);
		printf("Parsing Time = %g seconds\n", maxdtime);
	}

/*
	printf("<%d>: Parsing Time = %g seconds\n", myrank, dtime);
	printf("<%d>: Parsing Success = %d, Parsing Fails = %d\n", myrank, parse_success_count, parse_failed_count);
*/
#ifdef ALGOTIME
	printf("Core Algorithm Time = %g seconds\n", algo_time);
#endif

	StackDestroy(&stack);
	StackDestroy(&bt_stack);
	StackDestroy(&parse_tree);
	MPI_Finalize();
	return 0;
}
