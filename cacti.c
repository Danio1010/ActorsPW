#include "cacti.h"
#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>

typedef struct queue_mes {
    int front;
    int rear;
    int size;
    int max_size;

    message_t* arr;
} queue_mes;

queue_mes* new_queue_mes(int size) {
    queue_mes* res = (queue_mes*) malloc(sizeof(struct queue_mes));
    if(res == NULL) {
        exit(1);
    }
    res->max_size = size;
    res->front = 0;
    res->rear = size - 1;
    res->size = 0;

    res->arr = (message_t*) malloc(sizeof(message_t) * size);
    if(res->arr == NULL) {
        exit(1);
    }

    return res;
}

void realloc_queue_mes(queue_mes* mes, int new_size) {
    if(mes->max_size > new_size) {
        return;
    }
    mes->arr = realloc(mes->arr, sizeof(message_t) * new_size);
    if(mes->arr == NULL) {
        exit(1);
    }
    int temp = mes->front;

    if(mes->front <= mes->rear) {
        mes->max_size = new_size;
        return;
    } else {
        int j = new_size - (mes->max_size - mes->front);
        temp = j;
        for(int i = mes->front; i < mes->size; i++, j++) {
            mes->arr[j] = mes->arr[i];
        }
    }
    mes->front = temp;
    mes->max_size = new_size;
}

void put_queue_mes(queue_mes* q, message_t mes) {
    if(q->max_size == q->size) {
        realloc_queue_mes(q,q->max_size * 2);
    }
    q->rear = (q->rear + 1) % q->max_size;
    q->arr[q->rear] = mes; // czy nei kopjuje messages??
    q->size++;
}

// Przed uzyciem sprawdzic czy nie jest pusta
message_t pop_queue_mes(queue_mes* q) {
    message_t res = q->arr[q->front];
    q->front = (q->front + 1) % q->max_size;
    q->size--;
    return res;
}

typedef struct actor_state {
    queue_mes* messages;
    role_t* rola;
    void *stateptr;
    bool is_alive;
    actor_id_t id;
    actor_id_t parent_id;
    bool is_waiting;
    pthread_mutex_t mutex;
} actor_state;

struct actor_state** aktory;

typedef struct queue_actor {
    int front;
    int rear;
    int size;
    int max_size;

    struct actor_state** arr;
} queue_actor;


queue_actor* new_queue_actor(int size) {
    queue_actor* res = (queue_actor *) malloc(sizeof(struct queue_actor));
    if(res == NULL) {
        exit(1);
    }
    res->max_size = size;
    res->front = 0;
    res->rear = size - 1;
    res->size = 0;

    res->arr = (actor_state**) malloc(sizeof(actor_state*) * size);
    if(res->arr == NULL) {
        exit(1);
    }

    return res;
}

void realloc_queue_actor(queue_actor* mes, int new_size) {
    if(mes->max_size > new_size) {
        return;
    }
    mes->arr = realloc(mes->arr, sizeof(actor_state*) * new_size);
    if(mes->arr == NULL) {
        exit(1);
    }
    int temp = mes->front;

    if(mes->front <= mes->rear) {
        mes->max_size = new_size;
        return;
    } else {
        int j = new_size - (mes->max_size - mes->front);
        temp = j;
        for(int i = mes->front; i < mes->size; i++, j++) {
            mes->arr[j] = mes->arr[i];
        }
    }
    mes->front = temp;
    mes->max_size = new_size;
}

void put_queue_actor(queue_actor* q, actor_state* ac) {
    if(q->max_size == q->size) {
        realloc_queue_actor(q,q->max_size * 2);
    }

    if(q->max_size != q->size) {
        q->rear = (q->rear + 1) % q->max_size;
        q->arr[q->rear] = ac;
        q->size++;
    }
}

// Przed uzyciem sprawdzic czy nie jest pusta
actor_state* pop_queue_actor(queue_actor* q) {
    actor_state* res = q->arr[q->front];
    q->front = (q->front + 1) % q->max_size;
    q->size--;
    return res;
}

bool empty(queue_actor* q) {
    return q->size == 0;
}

typedef struct synchro {
    queue_actor* actor_queue;
    pthread_mutex_t mutex;
    pthread_cond_t actor_wait;
    pthread_mutex_t create_actor;
    size_t actors_working;
    int num_of_actors_alive;
    int num_of_actors;
    int curr_id;
    int size_of_actors;
    bool stop;
    bool sigint_recieved;
    int num_of_working_threads;

} synchro_t;

synchro_t* init();
pthread_t th[POOL_SIZE];

synchro_t* synchro;

void sigintHandler() {
    pthread_mutex_lock(&(synchro->mutex));

    synchro->sigint_recieved = true;
    for(int i = 0; i < synchro->curr_id; i++) {
        aktory[i]->is_alive = false;
    }
    synchro->num_of_actors_alive = 0;

    pthread_mutex_unlock(&(synchro->mutex));
}

actor_state* initActor(actor_id_t id, role_t* const role, actor_id_t parent_id) {
    actor_state* s = malloc(sizeof(actor_state));
    if(s == NULL) {
        exit(1);
    }
    s->messages = new_queue_mes(ACTOR_QUEUE_LIMIT);
    s->id = id;
    s->rola = role;
    s->is_alive = true;
    s->stateptr = NULL;
    s->parent_id = parent_id;
    s->is_waiting = false;
    pthread_mutex_init(&(s->mutex), NULL);

    return s;
}

actor_id_t createActor(role_t *const role, actor_id_t parent_id) {
    int curr_id = synchro->curr_id;
    int size_of_actors = synchro->size_of_actors;
    if(curr_id == size_of_actors - 1) {
        aktory = realloc(aktory, sizeof(struct actor_state*) * size_of_actors * 2);
        if(aktory == NULL) {
        exit(1);
        }
        synchro->size_of_actors = size_of_actors * 2;
    }

    aktory[curr_id] = initActor(curr_id,role,parent_id);
    synchro->num_of_actors++;
    actor_id_t temp = curr_id;
    synchro->curr_id++;

    return temp;
}

void freeMemory() {
    for(int i = 0; i < synchro->curr_id; i++) {
        free(aktory[i]->messages->arr);
        free(aktory[i]->messages);
        pthread_mutex_destroy(&(aktory[i]->mutex));
        free(aktory[i]);
    }
    free(aktory);
    free(synchro->actor_queue->arr);
    free(synchro->actor_queue);
    pthread_mutex_destroy(&(synchro->mutex));
    pthread_mutex_destroy(&(synchro->create_actor));
    pthread_cond_destroy(&(synchro->actor_wait));
    free(synchro);

}

void use_message(actor_state* ac, message_t mes) {
    int index = mes.message_type;

    if(index == MSG_SPAWN) {
        pthread_mutex_lock(&(synchro->mutex));

        //pthread_mutex_lock(&(synchro->create_actor));
        if(!synchro->sigint_recieved) {

            if (synchro->num_of_actors == CAST_LIMIT) {
                //pthread_mutex_unlock(&(synchro->create_actor));
                pthread_mutex_unlock(&(synchro->mutex));

                //zwalnia to co sie da
                freeMemory();
                exit(1);
            }

            actor_id_t new = createActor(mes.data, actor_id_self());

            synchro->num_of_actors_alive++;
            pthread_mutex_unlock(&(synchro->mutex));
            //pthread_mutex_unlock(&(synchro->create_actor));

            send_message(new, (message_t)
                    {.message_type = MSG_HELLO, .nbytes = sizeof(actor_id_t *),
                            .data = (void *) actor_id_self()});
        } else {
            pthread_mutex_unlock(&(synchro->mutex));
	    }
        return;
    }

    if(index == MSG_GODIE) {
        pthread_mutex_lock(&(synchro->mutex));
        //pthread_mutex_lock(&(synchro->create_actor));

        if(ac->is_alive) {
            ac->is_alive = false;
            synchro->num_of_actors_alive--;
        }

        //pthread_mutex_unlock(&(synchro->create_actor));
        pthread_mutex_unlock(&(synchro->mutex));

        return;
    }

    act_t func = (ac->rola->prompts)[index];
    func(&(ac->stateptr),mes.nbytes,mes.data);
}

pthread_key_t currentActor;

void* thread(void* arg) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask,SIGINT);

    sigprocmask(SIG_BLOCK, &mask, 0);

    signal(SIGINT,sigintHandler);

    synchro_t* sync = (synchro_t*) arg;
    actor_state* aktor = NULL;
    actor_id_t *actorId = malloc(sizeof(actor_id_t));
    if(actorId == NULL) {
        exit(1);
    }

    while(true) {
        pthread_mutex_lock(&(sync->mutex));

        if(synchro->num_of_actors_alive == 0 && empty(sync->actor_queue)
           && sync->actors_working == 0) {
            synchro->stop = true;
            pthread_cond_broadcast(&(synchro->actor_wait));
        }
        // Tak długo jak nie ma aktora ktory czeka na przetworzenie
        // czekamy
        while(empty(sync->actor_queue) && !sync->stop) {
            pthread_cond_wait(&(sync->actor_wait),&(sync->mutex));
        }

        if(sync->stop) {
            pthread_cond_broadcast(&(sync->actor_wait));
            pthread_mutex_unlock(&(sync->mutex));
            break;
        }

        aktor = pop_queue_actor(sync->actor_queue);
        *actorId = aktor->id;
        pthread_setspecific(currentActor, actorId);
        sync->actors_working++;

        pthread_mutex_unlock(&(sync->mutex));

        message_t mes = (message_t) {.message_type = -1, .data = NULL, .nbytes = 0};

        pthread_mutex_lock(&(aktor->mutex));
        if(aktor->messages->size > 0) {
            mes = pop_queue_mes(aktor->messages);
        }
        pthread_mutex_unlock(&(aktor->mutex));

        if(mes.message_type != -1) {
            use_message(aktor, mes);
        }

        pthread_mutex_lock(&(sync->mutex));

        aktor->is_waiting = false;
        sync->actors_working--;

        if(!aktor->is_waiting && aktor->messages->size > 0) {
            put_queue_actor(synchro->actor_queue,aktor);
            if(synchro->actor_queue->size == 1) {
                // moze broadcast???
                pthread_cond_broadcast(&(sync->actor_wait));
            }
            aktor->is_waiting = true;
        }

        if(synchro->num_of_actors_alive == 0 && empty(sync->actor_queue)
            && sync->actors_working == 0) {
            synchro->stop = true;
            pthread_cond_broadcast(&(synchro->actor_wait));
        }

        pthread_mutex_unlock(&(sync->mutex));

        sigprocmask(SIG_UNBLOCK, &mask, 0);
        sigprocmask(SIG_BLOCK, &mask, 0);
    }

    free(actorId);

    pthread_mutex_lock(&(sync->mutex));
    sync->num_of_working_threads--;
    if(sync->num_of_working_threads == 0) {
        pthread_mutex_unlock(&(sync->mutex));
        freeMemory();
        return NULL;
    }

    pthread_mutex_unlock(&(sync->mutex));
    return NULL;
}

int actor_system_create(actor_id_t *actor, role_t *const role) {

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask,SIGINT);
    if(sigprocmask(SIG_BLOCK, &mask, 0) == -1) {
        exit(1);
    }

    synchro = init();

    aktory = (actor_state **) malloc(sizeof(actor_state*) * synchro->size_of_actors);
    if(aktory == NULL) {
        exit(1);
    }

    *actor = createActor(role,0);
    synchro->num_of_actors_alive++;

    pthread_key_create(&currentActor,NULL);

    for(int i = 0; i < POOL_SIZE; i++) {
        pthread_create(&th[i],NULL,thread,synchro);
    }

    synchro->num_of_working_threads = POOL_SIZE;

    send_message(*actor,(message_t)
    {.message_type = MSG_HELLO, .data = NULL, .nbytes = 0});

    return 0;
}

int send_message(actor_id_t actor, message_t message) {

    pthread_mutex_lock(&(synchro->mutex));
    if(synchro->sigint_recieved) {
        pthread_mutex_unlock(&(synchro->mutex));
        return -1;
    }
    if(actor >= synchro->curr_id) {
        pthread_mutex_unlock(&(synchro->mutex));
        return -2;
    }

    actor_state* actorState = aktory[actor];
    pthread_mutex_lock(&(actorState->mutex));

    if(!actorState->is_alive) {
        pthread_mutex_unlock(&(actorState->mutex));
        pthread_mutex_unlock(&(synchro->mutex));

        return -1;
    }

    if(actorState->messages->size == ACTOR_QUEUE_LIMIT) {
        pthread_mutex_unlock(&(actorState->mutex));
        pthread_mutex_unlock(&(synchro->mutex));

        return -3;
    }

    if((size_t) message.message_type >= actorState->rola->nprompts &&
        message.message_type != MSG_SPAWN &&
        message.message_type != MSG_HELLO &&
        message.message_type != MSG_GODIE) {
        pthread_mutex_unlock(&(actorState->mutex));
        pthread_mutex_unlock(&(synchro->mutex));

        return -4;
    }

    put_queue_mes(actorState->messages,message);

    if(actorState->messages->size == 1 && !actorState->is_waiting) {

        put_queue_actor(synchro->actor_queue,actorState);
        if(synchro->actor_queue->size == 1) {
            pthread_cond_broadcast(&(synchro->actor_wait));
        }
        actorState->is_waiting = true;
    }

    pthread_mutex_unlock(&(actorState->mutex));
    pthread_mutex_unlock(&(synchro->mutex));

    return 0;
}

actor_id_t actor_id_self() {
    return *((actor_id_t*) pthread_getspecific(currentActor));
}

void actor_system_join(__attribute__((unused)) actor_id_t actor) {
    for(int i = 0; i < POOL_SIZE; i++) {
        pthread_join(th[i], NULL);
    }
}

synchro_t* init() {
    synchro_t* s;
    s = malloc(sizeof(synchro_t));
    if(s == NULL) {
        exit(1);
    }
    s->stop = false;
    s->actors_working = 0;
    pthread_mutex_init(&(s->mutex), NULL);
    pthread_cond_init(&(s->actor_wait), NULL);
    pthread_mutex_init(&(s->create_actor), NULL);
    s->actor_queue = new_queue_actor(1);
    s->num_of_actors_alive = 0;
    s->curr_id = 0;
    s->size_of_actors = 1000;
    s->num_of_actors = 0;
    s->num_of_working_threads = 0;
    s->sigint_recieved = false;
    return s;
}
