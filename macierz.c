#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "cacti.h"

typedef struct info {
   int column;
   int k;
   int rows;

   bool is_next_actor;

   actor_id_t me;
   actor_id_t next_actor;
   role_t* role;
} info_t;

typedef struct sum {
    int row;
    int row_sum;
} sum_t;

actor_id_t* first;
int** m_v;
int** m_t;
int* sums;
bool* finished;

info_t* copyInfo(info_t* org, void **stateptr) {
    info_t* new = *stateptr;
    new->k = org->k;
    new->is_next_actor = org->is_next_actor;
    new->role = org->role;
    new->column = org->column;
    new->rows = org->rows;

    return new;
}

void createActors(void **stateptr, __attribute__((unused)) size_t nbytes, void *data) {

    info_t* info = copyInfo(data,stateptr);
    info->column++;
    info->me = actor_id_self();
    *stateptr = info;

    if(info->column == info->k - 1) {
        for(int i = 0; i < info->rows; i++) {
            info->is_next_actor = false;
            sum_t* sum = malloc(sizeof(sum_t));
            sum->row_sum = 0;
            sum->row = i;
            send_message(*first, (message_t) {
                    .message_type = 1, .data = sum, .nbytes = sizeof(sum_t)
            });
        }
    } else {
        send_message(actor_id_self(),(message_t)
                {.message_type = MSG_SPAWN, .data = info->role, .nbytes = sizeof(role_t*)});
    }
}


void calcSums(void **stateptr, __attribute__((unused)) size_t nbytes, void *data) {
    info_t* info = (info_t*) *stateptr;
    sum_t* sum = data;

    usleep(m_t[sum->row][info->column] * 1000);

    sum->row_sum += m_v[sum->row][info->column];

    if(info->column == info->k - 1) {
        sums[sum->row] = sum->row_sum;
        finished[sum->row] = true;
        bool all_finished = true;
        for(int i = 0; i < info->rows; i++) {
            if(!finished[i]) {
                all_finished = false;
                break;
            }
        }
        free(sum);
        if(all_finished) {
            send_message(*first,(message_t)
                    {.message_type = 4, .nbytes = 0, .data = NULL});
        }
        return;
    }
    send_message(info->next_actor,(message_t)
    {.message_type = 1, .data = sum, .nbytes = sizeof(sum_t)});
}

void send_data_to_next(void **stateptr, __attribute__((unused)) size_t nbytes, void *data) {
    info_t* info = (info_t*) *stateptr;
    actor_id_t son = *((actor_id_t*) data);
    info->is_next_actor = true;
    info->next_actor = son;

    send_message(son,(message_t)
            {.message_type = 2,.data = *stateptr, .nbytes = sizeof(info_t*)});

}

void helloFirst(void **stateptr, 
		        __attribute__((unused)) size_t nbytes, 
		        __attribute__((unused)) void *data) {
		
    *stateptr = malloc(sizeof(info_t));
    info_t* info = (info_t*) *stateptr;
    info->me = actor_id_self();
}

void hello(void **stateptr, __attribute__((unused)) size_t nbytes, void *data) {
    actor_id_t parent = (actor_id_t) data;
    *stateptr = malloc(sizeof(info_t));

    info_t* info = (info_t*) *stateptr;
    info->me = actor_id_self();

    send_message(parent,(message_t)
            {.message_type = 3, .data = &(info->me), .nbytes = sizeof(info_t*)});
}

void killAll(void **stateptr, 
	         __attribute__((unused)) size_t nbytes, 
		     __attribute__((unused)) void *data) {
    info_t* info = *stateptr;
    if(info->is_next_actor) {
        send_message(info->next_actor, (message_t)
                {.message_type = 4, .nbytes = 0, .data = NULL});
    }
    free(*stateptr);
    send_message(actor_id_self(),(message_t)
            {.message_type = MSG_GODIE, .nbytes = 0, .data = NULL});
}

int main(){
    int n = 0;
    int k = 0;

    scanf("%d",&n);
    scanf("%d",&k);

    int** matrix_wart = malloc(sizeof(int*) * n);
    int** matrix_wait = malloc(sizeof(int*) * n);;


    for(int i = 0; i < n; i++) {
        matrix_wart[i] = malloc(sizeof(int) * k);
        matrix_wait[i] = malloc(sizeof(int) * k);
        for(int j = 0; j < k; j++) {
            scanf("%d %d",&matrix_wart[i][j], &matrix_wait[i][j]);
        }
    }

    m_v = matrix_wart;
    m_t = matrix_wait;

    actor_id_t first_actor;

    //kazdy z nich ma w sumie ta sama role
    role_t* first_role = (role_t *)malloc(sizeof(role_t));
    first_role->nprompts = 5;
    act_t temp[] = {&helloFirst,&calcSums,&createActors,&send_data_to_next,&killAll};
    first_role->prompts = temp;

    info_t* info_org = malloc(sizeof(info_t));

    role_t* second_role = (role_t *)malloc(sizeof(role_t));
    second_role->nprompts = 5;
    act_t temp2[] = {&hello,&calcSums,&createActors,&send_data_to_next,&killAll};
    second_role->prompts = temp2;

    info_org->column = -1;
    info_org->k = k;
    info_org->rows = n;

    sums = malloc(sizeof(int) * n);
    finished = malloc(sizeof(bool) * n);

    for(int i = 0; i < n; i++) {
        sums[i] = 0;
        finished[i] = false;
    }

    actor_system_create(&first_actor,first_role);
    first = &first_actor;
    info_org->me = first_actor;
    info_org->role = second_role;

    send_message(first_actor,(message_t)
    {.message_type = 2, .data = info_org, .nbytes = sizeof(info_t)});

    actor_system_join(first_actor);

    for(int i = 0; i < n; i++) {
        printf("%d\n",sums[i]);
    }

    free(sums);
    free(finished);
    free(info_org);
    free(first_role);
    free(second_role);

    for(int i = 0; i < n; i++) {
        free(matrix_wart[i]);
        free(matrix_wait[i]);
    }
    free(matrix_wart);
    free(matrix_wait);

	return 0;
}
