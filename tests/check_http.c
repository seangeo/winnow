/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <stdlib.h>
#include <check.h>
#include <config.h>
#include <stdio.h>
#include <string.h>
#include "../src/cls_config.h"
#include "../src/classification_engine.h"
#include "../src/httpd.h"
#include "assertions.h"

#define PORT 8008

Config *config;
ClassificationEngine *ce;
Httpd *httpd;
static FILE *test_data;
static FILE *devnull;

static void setup_httpd() {
  config = load_config("fixtures/real-db.conf");
  ce = create_classification_engine(config);  
  httpd = httpd_start(config, ce);
  test_data = fopen("http_test_data.log", "a");
  devnull = fopen("/dev/null", "w");
}

static void teardown_httpd() {
  httpd_stop(httpd);
  ce_stop(ce);
  free_classification_engine(ce);
  free_config(config);
  fclose(test_data);
  fclose(devnull);
}

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
    
START_TEST(test_http_initialization) {
  assert_get("http://localhost:8008/", 404, devnull);
} END_TEST

START_TEST(test_missing_job_returns_404) {
  assert_get("http://localhost:8008/classifier/jobs/missing", 404, devnull);
} END_TEST

START_TEST(test_missing_job_id_returns_405) {
  /* 405 is Method NOT allowed.
   * We get this because you can only POST to classifier/jobs/
   */
  assert_get("http://localhost:8008/classifier/jobs/", 405, test_data);
} END_TEST

START_TEST(test_post_to_create_job_with_tag_id_missing_returns_422) {
  char *post_data = "<?xml version='1.0'?>\n<classification-job><tag-id></tag-id></classification-job>\n";
  assert_post("http://localhost:8008/classifier/jobs", post_data, 422, devnull, devnull);
} END_TEST

/* Missing XML returns Unsupported media type (415) */
START_TEST(test_post_to_create_job_with_invalid_xml_returns_415) {
  char *post_data = "xxx";
  assert_post("http://localhost:8008/classifier/jobs", post_data, 415, devnull, devnull);
} END_TEST

START_TEST(test_post_to_create_job_without_xml_returns_415) {
  char *post_data = "";
  assert_post("http://localhost:8008/classifier/jobs", post_data, 415, devnull, devnull);
} END_TEST

START_TEST(test_post_with_valid_tag_id_queues_job) {
  FILE *headers = fopen("headers.txt", "w");
  FILE *data    = fopen("test_data.xml", "w"); 
  assert_equal(0, ce_num_jobs_in_system(ce));
  char *post_data = "<?xml version='1.0'?>\n<classification-job><tag-id>48</tag-id></classification-job>";
  assert_post("http://localhost:8008/classifier/jobs", post_data, 201, data, headers);
  fclose(headers);
  fclose(data);
  mark_point();
  
  assert_equal(1, ce_num_jobs_in_system(ce));
  xmlDocPtr doc = xmlReadFile("test_data.xml", NULL, 0);
  fail_unless(doc != NULL, "Failed to parse xml");
  assert_xpath("/classification-job/id/text()", doc);
  mark_point();
  
  xmlXPathContextPtr context = xmlXPathNewContext(doc);
  xmlXPathObjectPtr result = xmlXPathEvalExpression(BAD_CAST "/classification-job/id/text()", context);
  xmlNodeSetPtr nodeset = result->nodesetval;
  mark_point();
  char *id = (char *) nodeset->nodeTab[0]->content;
  assert_not_null(id);
  
  ClassificationJob *job = ce_fetch_classification_job(ce, id);
  assert_not_null(job);
  assert_equal(48, cjob_tag_id(job));
  
  char grep[1024];
  sprintf(grep, "grep 'Location: /classifier/jobs/%s' headers.txt", id);
  fail_if(system(grep), "Headers were missing location: %s", id);

  xmlXPathFreeObject(result);  
  xmlXPathFreeContext(context);
  xmlFree(doc);
}
END_TEST

// Expected xml should look like this:
//
//  <classification-job>
//    <id>ID</id>
//    <progress type="float">0.0</progress>
//  </classification-job>
//
START_TEST(test_job_status) {
  char url[256];
  ClassificationJob *job = ce_add_classification_job(ce, 39);
  sprintf(url, "http://localhost:8008/classifier/jobs/%s", cjob_id(job));
  FILE *data = fopen("test_data.xml", "w");
  assert_get(url, 200, data);
  fclose(data);
  
  char idpath[1024];
  sprintf(idpath, "/classification-job/id[text() = '%s']", cjob_id(job));
                
  xmlDocPtr doc = xmlReadFile("test_data.xml", NULL, 0);
  if (doc == NULL) fail("Failed to parse xml");
  
  assert_xpath(idpath, doc);
  assert_xpath("/classification-job/progress[text() = '0.0']", doc);
  
  xmlFree(doc);
} END_TEST

#endif

Suite * http_suite(void) {
  Suite *s = suite_create("http");
  TCase *tc_case = tcase_create("case");
  tcase_add_checked_fixture(tc_case, setup_httpd, teardown_httpd);
  
#ifdef HAVE_LIBCURL
  // START_TESTS
  tcase_add_test(tc_case, test_http_initialization);
  tcase_add_test(tc_case, test_job_status);  
  tcase_add_test(tc_case, test_missing_job_returns_404);
  tcase_add_test(tc_case, test_missing_job_id_returns_405);
  
  tcase_add_test(tc_case, test_post_to_create_job_without_xml_returns_415);
  tcase_add_test(tc_case, test_post_to_create_job_with_invalid_xml_returns_415);
  tcase_add_test(tc_case, test_post_to_create_job_with_tag_id_missing_returns_422);
  tcase_add_test(tc_case, test_post_with_valid_tag_id_queues_job);
  
  // END_TESTS
#endif
  suite_add_tcase(s, tc_case);
  return s;
}

int main(void) {
  initialize_logging("http_test.log");
  int number_failed;
  
  SRunner *sr = srunner_create(http_suite());
  
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  close_log();
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}