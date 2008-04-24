/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */
 
#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include "assertions.h"
#include "../src/tagger.h"

static ItemCacheOptions item_cache_options = {1, 3650, 2};
static ItemCache *item_cache;
static TaggerCache *tagger_cache;
static TaggerCacheOptions *options;
int load_tag_document_called = 0;
time_t last_updated_called = 0;
char *document;

static void read_document(const char * filename) {
  FILE *file;

  if (NULL != (file = fopen(filename, "r"))) {
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    document = calloc(size, sizeof(char));
    fseek(file, 0, SEEK_SET);
    fread(document, sizeof(char), size, file);
    document[size] = 0;
    fclose(file);
  }
}

static int load_tag_document(const char * tag_training_url, time_t last_updated, char ** tag_document, char ** errmsg) {
  load_tag_document_called++;
  last_updated_called = last_updated;
  
  if (!strcmp(tag_training_url, "http://example.org/missing.atom")) {
    if (errmsg) {
      *errmsg = strdup("Error message");      
    }
    
    return TAG_NOT_FOUND;
  } else if (load_tag_document_called > 1) {
    return TAG_NOT_MODIFIED;
  } else {
    *tag_document = strdup(document);
    return TAG_OK;
  }  
}
 
static void setup(void) {
  read_document("fixtures/complete_tag.atom");
  system("cp -f fixtures/valid.db /tmp/valid-copy.db");
  item_cache_create(&item_cache, "/tmp/valid-copy.db", &item_cache_options);
  load_tag_document_called = 0;
  tagger_cache = create_tagger_cache(item_cache, options);
  tagger_cache->tag_retriever = &load_tag_document;
}

static void setup_for_incomplete(void) {
  read_document("fixtures/incomplete_tag.atom");
  
  system("cp -f fixtures/valid.db /tmp/valid-copy.db");
  item_cache_create(&item_cache, "/tmp/valid-copy.db", &item_cache_options);
  load_tag_document_called = 0;
  tagger_cache = create_tagger_cache(item_cache, options);
  tagger_cache->tag_retriever = &load_tag_document;
}

static void teardown(void) {
  free_tagger_cache(tagger_cache);
  free(document);
}

START_TEST (test_get_tagger_returns_NULL_for_NULL_tag_url) {
  Tagger *tagger = NULL;
  get_tagger(tagger_cache, NULL, &tagger, NULL);
  assert_null(tagger);
} END_TEST

START_TEST (test_get_tagger_returns_TAG_NOT_FOUND_when_missing) {
  Tagger *tagger = NULL;
  int rc = get_tagger(tagger_cache, "http://example.org/missing.atom", &tagger, NULL);
  assert_equal(TAG_NOT_FOUND, rc);
} END_TEST

START_TEST (test_get_tagger_when_tagger_missing_returns_NULL) {
  Tagger *tagger = NULL;
  get_tagger(tagger_cache, "http://example.org/missing.atom", &tagger, NULL);
  assert_null(tagger);
} END_TEST

START_TEST (test_get_tagger_when_tagger_missing_sets_error_message) {
  char *err = "none";
  Tagger *tagger = NULL;
  get_tagger(tagger_cache, "http://example.org/missing.atom", &tagger, &err);
  assert_equal_s("Error message", err);
} END_TEST

START_TEST (test_get_tagger_returns_TAGGER_OK_when_valid) {
  Tagger *tagger = NULL;
  int rc = get_tagger(tagger_cache, "http://trunk.mindloom.org:80/seangeo/tags/a-religion/training.atom", &tagger, NULL);
  assert_equal(TAGGER_OK, rc);
} END_TEST

START_TEST (test_get_tagger_that_returns_a_complete_valid_document_returns_a_tagger_in_precomputed_state) {
  Tagger *tagger = NULL;
  get_tagger(tagger_cache, "http://trunk.mindloom.org:80/seangeo/tags/a-religion/training.atom", &tagger, NULL);
  assert_not_null(tagger);
  assert_equal(TAGGER_PRECOMPUTED, tagger->state);
} END_TEST

START_TEST (test_get_tagger_called_again_without_releasing_the_tagger_returns_TAGGER_CHECKED_OUT) {
  Tagger *tagger = NULL;
  int rc = get_tagger(tagger_cache, "http://trunk.mindloom.org:80/seangeo/tags/a-religion/training.atom", &tagger, NULL);
  assert_equal(TAGGER_OK, rc);
  rc = get_tagger(tagger_cache, "http://trunk.mindloom.org:80/seangeo/tags/a-religion/training.atom", &tagger, NULL);
  assert_equal(TAGGER_CHECKED_OUT, rc);
} END_TEST

START_TEST (test_get_tagger_called_again_without_releasing_the_tagger_sets_error_message) {
  char *err = "none";
  Tagger *tagger = NULL;
  get_tagger(tagger_cache, "http://trunk.mindloom.org:80/seangeo/tags/a-religion/training.atom", &tagger, NULL);
  get_tagger(tagger_cache, "http://trunk.mindloom.org:80/seangeo/tags/a-religion/training.atom", &tagger, &err);
  assert_equal_s("Tagger already being processed", err);
} END_TEST

START_TEST (test_get_tagger_called_again_after_releasing_the_tagger_gets_the_same_tagger) {
  Tagger *tagger = NULL;
  Tagger *second = NULL;
  get_tagger(tagger_cache, "http://trunk.mindloom.org:80/seangeo/tags/a-religion/training.atom", &tagger, NULL);
  release_tagger(tagger_cache, tagger);
  int rc = get_tagger(tagger_cache, "http://trunk.mindloom.org:80/seangeo/tags/a-religion/training.atom", &second, NULL);  
  assert_equal(TAGGER_OK, rc);
  assert_not_null(tagger);
  assert_not_null(second);
  assert_equal(tagger, second);
} END_TEST

/******* Missing item tests *********/

START_TEST (test_get_tagger_that_returns_a_incomplete_valid_document_returns_TAG_PENDING_ITEM_ADDITION) {
  Tagger *tagger = NULL;
  int rc = get_tagger(tagger_cache, "http://trunk.mindloom.org:80/seangeo/tags/a-religion/training.atom", &tagger, NULL);
  assert_equal(TAGGER_PENDING_ITEM_ADDITION, rc);
} END_TEST

START_TEST (test_get_tagger_that_returns_a_incomplete_valid_document_has_null_tagger) {
  Tagger *tagger = NULL;
  get_tagger(tagger_cache, "http://trunk.mindloom.org:80/seangeo/tags/a-religion/training.atom", &tagger, NULL);
  assert_null(tagger);
} END_TEST

START_TEST (test_get_tagger_twice_that_returns_a_incomplete_valid_document_returns_TAG_PENDING_ITEM_ADDITION) {
  Tagger *tagger = NULL;
  get_tagger(tagger_cache, "http://trunk.mindloom.org:80/seangeo/tags/a-religion/training.atom", &tagger, NULL);
  int rc = get_tagger(tagger_cache, "http://trunk.mindloom.org:80/seangeo/tags/a-religion/training.atom", &tagger, NULL);
  assert_equal(TAGGER_PENDING_ITEM_ADDITION, rc);
} END_TEST

START_TEST (test_get_tagger_with_missing_items_should_add_the_items_to_the_item_cache) {
  get_tagger(tagger_cache, "http://trunk.mindloom.org:80/seangeo/tags/a-religion/training.atom", NULL, NULL);
  
  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select count(*) from entries;", -1, &stmt, NULL);
  
  if (SQLITE_ROW == sqlite3_step(stmt)) {
    int count = sqlite3_column_int(stmt, 0);
    assert_equal(13, count);
  } else {
    fail("SQL count didn't work");
  }
  
} END_TEST


Suite *
check_get_tagger_suite(void) {
  Suite *s = suite_create("check get_tagger");  
  TCase *tc_case = tcase_create("Case");

  tcase_add_checked_fixture(tc_case, setup, teardown);
  tcase_add_test(tc_case, test_get_tagger_returns_NULL_for_NULL_tag_url);
  tcase_add_test(tc_case, test_get_tagger_returns_TAG_NOT_FOUND_when_missing);
  tcase_add_test(tc_case, test_get_tagger_when_tagger_missing_returns_NULL);
  tcase_add_test(tc_case, test_get_tagger_when_tagger_missing_sets_error_message);
  tcase_add_test(tc_case, test_get_tagger_returns_TAGGER_OK_when_valid);
  tcase_add_test(tc_case, test_get_tagger_that_returns_a_complete_valid_document_returns_a_tagger_in_precomputed_state);
  tcase_add_test(tc_case, test_get_tagger_called_again_without_releasing_the_tagger_returns_TAGGER_CHECKED_OUT);
  tcase_add_test(tc_case, test_get_tagger_called_again_without_releasing_the_tagger_sets_error_message);
  tcase_add_test(tc_case, test_get_tagger_called_again_after_releasing_the_tagger_gets_the_same_tagger);
  
  TCase *tc_incomplete_case = tcase_create("Incomplete Case");

  tcase_add_checked_fixture(tc_incomplete_case, setup_for_incomplete, teardown);
  
  tcase_add_test(tc_incomplete_case, test_get_tagger_that_returns_a_incomplete_valid_document_returns_TAG_PENDING_ITEM_ADDITION);
  tcase_add_test(tc_incomplete_case, test_get_tagger_that_returns_a_incomplete_valid_document_has_null_tagger);
  tcase_add_test(tc_incomplete_case, test_get_tagger_twice_that_returns_a_incomplete_valid_document_returns_TAG_PENDING_ITEM_ADDITION);
  tcase_add_test(tc_incomplete_case, test_get_tagger_with_missing_items_should_add_the_items_to_the_item_cache);
  
  suite_add_tcase(s, tc_incomplete_case);
  suite_add_tcase(s, tc_case);
  return s;
}

int main(void) {
  initialize_logging("test.log");
  int number_failed;

  SRunner *sr = srunner_create(check_get_tagger_suite());
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  close_log();
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
