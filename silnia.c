#include <stdio.h>
#include <stdlib.h>
#include "cacti.h"
#include <stddef.h>

typedef struct factorial {
    long fact;
    int k;
    int n;
} factorial;

typedef struct info {
    factorial* factor;
    role_t* role;
    actor_id_t me;
    //void* father_memory;
} info_t;

actor_id_t* first;

void silnia(void **stateptr, __attribute__((unused)) size_t nbytes, void *data) {
    info_t* info = data;
    *stateptr = info;
    factorial* fac = info->factor;
    fac->k++;
    fac->fact = fac->fact * (fac->k);
    *stateptr = info;


    if(fac->k == fac->n) {
        printf("%ld\n",fac->fact);

        send_message(actor_id_self(),(message_t)
                {.message_type = MSG_GODIE, .data = NULL, .nbytes = 0});
    } else {
        send_message(actor_id_self(),(message_t)
                {.message_type = MSG_SPAWN, .data = info->role, .nbytes = sizeof(role_t)});
    }
}

void send_data_to_next(void **stateptr, __attribute__((unused)) size_t nbytes, void *data) {
    actor_id_t son = ((actor_id_t) data);
    info_t* info = *stateptr;

    send_message(son,(message_t)
            {.message_type = 1,.data = info, .nbytes = sizeof(factorial)});

    send_message(actor_id_self(),(message_t)
            {.message_type = MSG_GODIE, .data = NULL, .nbytes = 0});

}

void helloFirst(__attribute__((unused)) void **stateptr,
                __attribute__((unused)) size_t nbytes,
                __attribute__((unused)) void *data) {

}

void hello(__attribute__((unused)) void **stateptr, 
           __attribute__((unused)) size_t nbytes, void *data) {
    actor_id_t parent = (actor_id_t) data;

    send_message(parent,(message_t)
            {.message_type = 2, .data = (void *)actor_id_self(), .nbytes = sizeof(actor_id_t)});
}

int main() {

    int n = 0;
    scanf("%d",&n);

    actor_id_t first_actor;

    role_t* first_role = (role_t *)malloc(sizeof(role_t));
    first_role->nprompts = 3;
    act_t temp[] = {&helloFirst,&silnia,&send_data_to_next};
    first_role->prompts = temp;

    role_t* second_role = (role_t *)malloc(sizeof(role_t));
    second_role->nprompts = 3;
    act_t temp2[] = {&hello,&silnia,&send_data_to_next};
    second_role->prompts = temp2;

    actor_system_create(&first_actor,first_role);

    info_t* info = malloc(sizeof(info_t));
    info->role = second_role;

    factorial* fac = malloc(sizeof(factorial));
    fac->k = 0;
    fac->fact = 1;
    fac->n = n;

    info->factor = fac;
    info->me = -1;

    send_message(first_actor,(message_t)
    {.message_type = 1, .data = info, .nbytes = sizeof(info_t)});

    actor_system_join(first_actor);

    free(info);
    free(first_role);
    free(second_role);
    free(fac);

	return 0;
}
