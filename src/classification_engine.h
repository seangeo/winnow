/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#ifndef CLASSIFICATION_ENGINE_H_
#define CLASSIFICATION_ENGINE_H_

#include "cls_config.h"

typedef struct CLASSIFICATION_ENGINE ClassificationEngine;
typedef struct CLASSIFICATION_JOB ClassificationJob;

typedef enum CLASSIFICATION_JOB_STATE {
  CJOB_STATE_WAITING,
  CJOB_STATE_TRAINING,
  CJOB_STATE_CALCULATING,
  CJOB_STATE_CLASSIFYING,
  CJOB_STATE_COMPLETE,
  CJOB_STATE_CANCELLED,
  CJOB_STATE_ERROR
} ClassificationJobState;

typedef enum CLASSIFICATION_JOB_ERROR {
  CJOB_ERROR_NO_ERROR,
  CJOB_ERROR_NO_SUCH_TAG,
  CJOB_ERROR_NO_TAGS_FOR_USER,
  CJOB_ERROR_BAD_JOB_TYPE,
  CJOB_ERROR_UNKNOWN_ERROR
} ClassificationJobError;

typedef enum CLASSIFICATION_JOB_TYPE {
  CJOB_TYPE_TAG_JOB,
  CJOB_TYPE_USER_JOB
} ClassificationJobType;

typedef struct PERFORMANCE_STATS {  
  int   classification_jobs_processed;
  float classification_wait_time;
  float training_time;
  float calculating_time;
  float classifying_time;
  float tags_classified;
  float items_classified;
  
  int   insertion_jobs_processed;
  float insertion_wait_time;
  float insertion_time;
} PerformanceStats;

extern ClassificationEngine * create_classification_engine(const Config * config);
extern void                   free_classification_engine(ClassificationEngine * engine);
extern int                    ce_is_running(const ClassificationEngine *engine);
extern int                    ce_start(ClassificationEngine *engine);
extern int                    ce_stop(ClassificationEngine *engine);
extern int                    ce_suspend(ClassificationEngine *engine);
extern int                    ce_resume(ClassificationEngine *engine);
extern int                    ce_kill(ClassificationEngine *engine);
extern void                   ce_run(ClassificationEngine *engine);
extern void                   ce_performance_stats(const ClassificationEngine *engine, PerformanceStats *stats);

/* Job tracking and management */
extern int                    ce_num_jobs_in_system(const ClassificationEngine *engine);
extern int                    ce_num_waiting_jobs(const ClassificationEngine *engine);
extern ClassificationJob    * ce_add_classification_job_for_tag(ClassificationEngine *engine, int tag_id);
extern ClassificationJob    * ce_add_classify_new_items_job_for_tag(ClassificationEngine *engine, int tag_id);
extern ClassificationJob    * ce_add_classification_job_for_user(ClassificationEngine *engine, int user_id);
extern ClassificationJob    * ce_fetch_classification_job(const ClassificationEngine *engine, const char * job_id);
extern int                    ce_remove_classification_job(ClassificationEngine *engine, const ClassificationJob *job);

/** Functions for Classification Jobs */
extern const char *           cjob_id(const ClassificationJob * job);
extern int                    cjob_type(const ClassificationJob * job);
extern int                    cjob_tag_id(const ClassificationJob * job);
extern int                    cjob_user_id(const ClassificationJob * job);
extern float                  cjob_progress(const ClassificationJob * job);
extern ClassificationJobState cjob_state(const ClassificationJob *job);
extern const char *           cjob_state_msg(const ClassificationJob *job);
extern ClassificationJobError cjob_error(const ClassificationJob *job);
extern const char *           cjob_error_msg(const ClassificationJob *job);
extern void                   cjob_cancel(ClassificationJob *job);
extern float                  cjob_duration(const ClassificationJob *job);
#endif /*CLASSIFICATION_ENGINE_H_*/
