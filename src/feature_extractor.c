/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include "feature_extractor.h"
#include "logging.h"
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>

struct RESPONSE {
  int size;
  char *data;
};

static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream) {
  struct RESPONSE *response = (struct RESPONSE*) stream;
  
  if (response->size == 0) {
    response->size = size * nmemb;
    response->data = calloc(size + 1, nmemb);
    memcpy(response->data, ptr, size * nmemb);    
  } else {
    response->data = realloc(response->data, response->size + (size * nmemb) + 1);
    memcpy(&response->data[response->size], ptr, size * nmemb);
    response->size += (size * nmemb);
    response->data[response->size] = '\0';
  }
  
  return size * nmemb;
}

Item * tokenize_entry(ItemCache * item_cache, const ItemCacheEntry * entry, void *memo) {
  Item *item = NULL;
  const char *tokenizer_url = (char *) memo;
  
  if (item_cache && entry) {
    int code;
    char curlerr[512];
    const char * data = item_cache_entry_atom(entry);
    struct RESPONSE response;
    response.size = 0;
    
    if (!data) {
      error("TODO encode into atom?");
    } else {
      int entry_id = item_cache_entry_id(entry);
      info("tokenizing entry %i using %s", entry_id, tokenizer_url);
    
      CURL *curl = curl_easy_init();
      curl_easy_setopt(curl, CURLOPT_URL, tokenizer_url);
      curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
      curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlerr);
      curl_easy_setopt(curl, CURLOPT_POST, 1);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(data));
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);

      if (curl_easy_perform(curl)) {
        error("HTTP server not accessible: %s", curlerr);
      } else if (CURLE_OK != curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code)) {
        error("Could not get response code from tokenizer");
      } else if (code != 200) {
        error("Got %i for tokenization, expected 201", code);
      }  else {
        item = item_from_xml(item_cache, response.data);
      }

      curl_easy_cleanup(curl);
    }
  }
  
  return item;
}