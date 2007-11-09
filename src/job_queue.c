/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <pthread.h>
#include <stdlib.h>
#include "job_queue.h"
#include "logging.h"

typedef struct NODE Node;
struct NODE {
  Job *job;
  Node *next;
};

struct QUEUE {
  pthread_mutex_t lock;
  pthread_mutex_t wait_condition_mutex;
  pthread_cond_t  wait_condition;
  Node *front;
  Node *rear;
};

/** Creates a new empty Queue */
Queue * new_queue() {
  Queue *q = malloc(sizeof(struct QUEUE));
  if (NULL != q) {
    q->front = NULL;
    q->rear = NULL;
    if (pthread_mutex_init(&(q->lock), NULL)) {
      free(q);
      error("Error initializing mutex");
      exit(1);
    }
    
    if (pthread_mutex_init(&(q->wait_condition_mutex), NULL)) {
      free(q);
      error("Error initializing wait condition mutex");
      exit(1);
    }
    
    if (pthread_cond_init(&(q->wait_condition), NULL)) {
      free(q);
      error("Error initializing wait condition");
      exit(1);
    }
  }
  return q;
}

/** Dequeues a Job from the queue.
 *
 *  Returns NULL immediately if the queue is empty.
 */
Job * q_dequeue(Queue * q) {
  Job *return_job = NULL;
  Node *dequeued = NULL;
  
  pthread_mutex_lock(&(q->lock));
  dequeued = q->front;
  if (NULL != dequeued) {
    q->front = q->front->next;
  }
  pthread_mutex_unlock(&(q->lock));
  
  if (NULL != dequeued) {
    return_job = dequeued->job;
    free(dequeued);
  }
  
  return return_job;
}

/** Dequeue a Job from the queue.
 *
 *  Waits until a Job has been added to the queue if
 *  the queue is empty.
 *
 *  This was helped by http://www.yolinux.com/TUTORIALS/LinuxTutorialPosixThreads.html#BASICS
 */
Job * q_dequeue_or_wait(Queue * q) {
  Job *job = NULL;  
  
  /* The algorith here is first check if there is a job in the queue.
   *  - If there is no job wait until a job is added.
   *   - When a job is added the thread is woken then we try and
   *     take the job off the queue.
   *  - Keep doing this until we get a job.
   */
  while (NULL == job) {
    pthread_mutex_lock(&(q->wait_condition_mutex));
    if (NULL == q->front) {
      pthread_cond_wait(&(q->wait_condition), &(q->wait_condition_mutex));
    }
    pthread_mutex_unlock(&(q->wait_condition_mutex));
    
    job = q_dequeue(q);
  }
  //debug("returning %x as job", job);
  return job;
}

/** Enqueues a Job on the Queue.
 */
void q_enqueue(Queue * q, Job * job) {
  Node *new_node = malloc(sizeof(struct NODE));
  if (NULL == new_node) {
    error("Malloc error in enqueue");
    exit(1);
  }
  
  new_node->job = job;
  
  pthread_mutex_lock(&(q->lock));
  if (NULL == q->front) {
    q->front = new_node;
    q->rear = new_node;
  } else {
    q->rear->next = new_node;
    q->rear = new_node;
  }
  pthread_mutex_unlock(&(q->lock));
  
  pthread_mutex_lock(&(q->wait_condition_mutex));
  pthread_cond_signal(&(q->wait_condition));
  pthread_mutex_unlock(&(q->wait_condition_mutex));
}

/** Checks if the Queue is empty.
 *
 *  Returns 0 if the queue is not empty, 1 if it is.
 */
int q_empty(const Queue * queue) {
  return NULL == queue->front;
}