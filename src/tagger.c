/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include "tagger.h"

#include <string.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/uri.h>
#include "xml.h"
#include "uri.h"
#include "logging.h"


/************************************************************************************************
 *  Functions for building taggers from Atom documents.
*************************************************************************************************/

#define ATOM "http://www.w3.org/2005/Atom"
#define CLASSIFIER "http://peerworks.org/classifier"
#define OK 0
#define FAIL 1

static int load_negative_examples(Tagger * tagger, xmlXPathContextPtr ctx) {
  int rc = OK;
  
  /* Get the negative taggings */
  xmlXPathObjectPtr xp = xmlXPathEvalExpression(BAD_CAST "/atom:feed/atom:entry/atom:link[@rel = "
                                                         "'http://peerworks.org/classifier/negative-example']"
                                                         "/../atom:id/text()", ctx);
  
  if (!xmlXPathNodeSetIsEmpty(xp->nodesetval)) {
    int i;
    tagger->negative_example_count = xp->nodesetval->nodeNr;
    tagger->negative_examples = calloc(tagger->negative_example_count, sizeof(char*));
    // Allocate enough space to store all the negative examples in the missing_positive_examples array
    tagger->missing_negative_examples = calloc(tagger->negative_example_count, sizeof(char*));
    
    if (NULL == tagger->negative_examples) {
      rc = FAIL;
    } else {
      for (i = 0; i < tagger->negative_example_count; i++) {
        tagger->negative_examples[i] = strdup((char*) xp->nodesetval->nodeTab[i]->content);
      }
    }
  }
  
  xmlXPathFreeObject(xp);  
  
  return rc;  
}

/** Loads the positive example ids from the document. */
static int load_positive_examples(Tagger *tagger, xmlXPathContextPtr ctx) {
  int rc = OK;
  
  xmlXPathObjectPtr xp = xmlXPathEvalExpression(BAD_CAST "/atom:feed/atom:entry/atom:category/../atom:id/text()", ctx);
  if (!xmlXPathNodeSetIsEmpty(xp->nodesetval)) {
    int i;
    tagger->positive_example_count = xp->nodesetval->nodeNr;
    tagger->positive_examples = calloc(tagger->positive_example_count, sizeof(char*));
    // Allocate enough space to store all the positive examples in the missing_positive_examples array
    tagger->missing_positive_examples = calloc(tagger->positive_example_count, sizeof(char*));
    
    if (NULL == tagger->positive_examples) {
      rc = FAIL;
    } else {
      for (i = 0; i < tagger->positive_example_count; i++) {
        tagger->positive_examples[i] = strdup((char*) xp->nodesetval->nodeTab[i]->content);
      }
    }    
  }
  xmlXPathFreeObject(xp);
  
  return rc;
}

/** Builds a Tagger from an atom document.
 *
 * This will parse the atom document given by the 'atom' parameter and
 * return a Tagger that contains the definition for the tag described
 * in the document.
 *
 * TODO Document the atom format somewhere.
 */
Tagger * build_tagger(const char * atom) {
  Tagger * tagger = calloc(1, sizeof(struct TAGGER));
  
  if (tagger) {
    xmlDocPtr doc = xmlParseDoc(BAD_CAST atom);
    
    if (doc) {
      xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
      xmlXPathRegisterNs(ctx, BAD_CAST "atom", BAD_CAST ATOM);
      xmlXPathRegisterNs(ctx, BAD_CAST "classifier", BAD_CAST CLASSIFIER);
      
      tagger->tag_id = get_element_value(ctx, "/atom:feed/atom:id/text()");
      tagger->training_url = get_attribute_value(ctx, "/atom:feed/atom:link[@rel = 'self']", "href");
      tagger->classifier_taggings_url = get_attribute_value(ctx, "/atom:feed/atom:link[@rel = 'http://peerworks.org/classifier/edit']", "href");
      tagger->updated = get_element_value_time(ctx, "/atom:feed/atom:updated/text()");
      tagger->last_classified = get_element_value_time(ctx, "/atom:feed/classifier:classified/text()");
      tagger->bias = get_element_value_double(ctx, "/atom:feed/classifier:bias/text()");
      
      if (FAIL == load_positive_examples(tagger, ctx)) {
        // TODO unwind and return NULL
      }  else if (FAIL == load_negative_examples(tagger, ctx)) {
        // TODO unwind and return NULL
      }
            
      xmlXPathFreeContext(ctx);
      xmlFreeDoc(doc);
    }
    
    tagger->state = TAGGER_LOADED;
    tagger->atom = strdup(atom);
  }
  
  return tagger;
}

static void train_pool(Pool * pool, ItemCache * item_cache, char ** examples, 
                       int size, char ** missing_examples, int * missing_size) {
  int i;
  *missing_size = 0;
  
  for (i = 0; i < size; i++) {
    int free_when_done = 0;
    Item * item = item_cache_fetch_item(item_cache, (unsigned char*) examples[i], &free_when_done);
    if (item) {
      pool_add_item(pool, item);
      if (free_when_done) free_item(item);
    } else {
      missing_examples[(*missing_size)++] = examples[i];
    }
  }
}

/** Trains the tagger using it's examples.
 *
 *  This will build the positive and negative pools for the the tagger.
 *  If this is successful the state of the tagger will be set to 
 *  TAGGER_TRAINED and postive_pool and negative_pool members will
 *  be trained up with the items in the positive and negative examples.
 *
 *  This function uses the item_cache to get the items.  If an example
 *  is not in the item cache it's id will be recorded in either the
 *  missing_positive_examples or missing_negative_examples array. 
 *  It will train as many items that it can and only store missing ids
 *  in the missing id arrays.  If this happens the tagger state will be
 *  set to TAGGER_PARTIALLY_TRAINED and this will be returned.
 *
 *  @params tagger The tagger to train.
 *  @params item_cache The item cache to get the items from.
 *  @return The new state of the tagger.
 */
TaggerState train_tagger(Tagger * tagger, ItemCache * item_cache) {
  TaggerState state = UNKNOWN;
  
  if (tagger && item_cache) {
    state = tagger->state = TAGGER_TRAINED;    
    tagger->positive_pool = new_pool();
    tagger->negative_pool = new_pool();
    
    train_pool(tagger->positive_pool, item_cache, 
               tagger->positive_examples, tagger->positive_example_count, 
               tagger->missing_positive_examples, &tagger->missing_positive_example_count);
    train_pool(tagger->negative_pool, item_cache, 
               tagger->negative_examples, tagger->negative_example_count,
               tagger->missing_negative_examples, &tagger->missing_negative_example_count);
               
    if (tagger->missing_positive_example_count > 0 || tagger->missing_negative_example_count > 0) {
      state = tagger->state = TAGGER_PARTIALLY_TRAINED;
    }
  }
  
  return state;
}