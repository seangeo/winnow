/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <stdlib.h>
#include <check.h>

Suite * db_item_source_suite (void);
Suite * tag_db_suite (void);
Suite * tagging_store_suite (void);

int main(void) {
  int number_failed;
  
  SRunner *sr = srunner_create(db_item_source_suite());
  srunner_add_suite(sr, tag_db_suite());
  srunner_add_suite(sr, tagging_store_suite());
  
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}