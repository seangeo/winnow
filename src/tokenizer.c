// Copyright (c) 2007-2010 The Kaphan Foundation
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// contact@winnowtag.org

#include <libxml/HTMLparser.h>
#include <libxml/xmlreader.h>
#include <libxml/tree.h>
#include <libxml/uri.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include "tokenizer.h"
#include "logging.h"
#include "buffer.h"
#include <string.h>
#include <ctype.h>
#include <Judy.h>
#include <regex.h>
#include "xml.h"

// From http://www.daniweb.com/code/snippet216955.html
int rreplace (char *buf, int size, regex_t *re, char *rp) {
    char *pos;
    int sub, so, n;
    regmatch_t pmatch [10]; /* regoff_t is int so size is int */

    if (regexec (re, buf, 10, pmatch, 0)) return 0;
    for (pos = rp; *pos; pos++)
        if (*pos == '\\' && *(pos + 1) > '0' && *(pos + 1) <= '9') {
            so = pmatch [*(pos + 1) - 48].rm_so;
            n = pmatch [*(pos + 1) - 48].rm_eo - so;
            if (so < 0 || strlen (rp) + n - 1 > size) return 1;
            memmove (pos + n, pos + 2, strlen (pos) - 1);
            memmove (pos, buf + so, n);
            pos = pos + n - 2;
        }
    sub = pmatch [1].rm_so; /* no repeated replace when sub >= 0 */
    for (pos = buf; !regexec (re, pos, 1, pmatch, 0); ) {
        n = pmatch [0].rm_eo - pmatch [0].rm_so;
        pos += pmatch [0].rm_so;
        if (strlen (buf) - n + strlen (rp) > size) return 1;
        memmove (pos + strlen (rp), pos + n, strlen (pos) - n + 1);
        memmove (pos, rp, strlen (rp));
        pos += strlen (rp);
        if (sub >= 0) break;
    }
    return 0;
}

static int replace(char *buf, int size, const char * pattern, char *rp) {
	regex_t regex;
	int regex_error;

	if ((regex_error = regcomp(&regex, pattern, REG_EXTENDED)) != 0) {
		char buffer[1024];
	    regerror(regex_error, &regex, buffer, sizeof(buffer));
	    fatal("Error compiling REGEX: %s", buffer);
	} else if (rreplace(buf, size, &regex, rp)) {
		fatal("Error doing regex replace");
	} else {
	  regfree(&regex);
	}

	return 0;
}

static void processNode(xmlTextReaderPtr reader, Buffer * buf) {
	int type = xmlTextReaderNodeType(reader);

	if (type == XML_TEXT_NODE) {
		unsigned char *s = xmlTextReaderReadString(reader);
		int slen = strlen(s);
		char enc[512];
		int enclen = 512;
		if (!htmlEncodeEntities(enc, &enclen, s, &slen, 0)) {
			buffer_in(buf, enc, enclen);

			if (enc[enclen-1] != ' ') {
				buffer_in(buf, " ", 1);
			}
		}

		free(s);
	}
}

static void foldcase(char * txt) {
	int i;

	for (i = 0; txt[i]; i++) {
		txt[i] = tolower(txt[i]);
	}
}

static Pvoid_t add_token(char *token, Pvoid_t features) {
	Word_t *PValue;
	int toklen = strlen(token);
	char *feature = malloc(toklen * sizeof(char) + 3);
	strncpy(feature, "t:", 3);
	strncat(feature, token, toklen + 3);
	JSLI(PValue, features, (unsigned char*) feature);
	(*PValue)++;
	free(feature);
	return features;
}

static Pvoid_t tokenize_text(char * txt, int length, Pvoid_t features) {
	char *token;

	// Remove HTML entities
	replace(txt, length, "&[^;]+;", " ");
	// Remove all non-alphanums
	replace(txt, length, "[^a-zA-Z0-9\\-]", " ");
	// Remove leading and trailing dashes
	replace(txt, length, "[[:space:]]+[\\-]+", " ");
	replace(txt, length, "\\-+[[:space:]]+", " ");
	// Normalize whitespace
	replace(txt, length, "[[:space:]]+", " ");
	foldcase(txt);

	for (; (token = strsep(&txt, "\t\n ")) != NULL; ) {
		if (token != '\0') {
			int toklen = strlen(token) + 1; // +1 for \0
			if (toklen > 2) {
				features = add_token(token, features);
			}
		}
	}

	return features;
}

static Buffer *extractText(htmlDocPtr doc) {
		char * term = "\0";
    Buffer *buf = new_buffer(256);
    xmlTextReaderPtr reader = xmlReaderWalker(doc);
    while(xmlTextReaderRead(reader)){
        processNode(reader, buf);
    }
    xmlFreeTextReader(reader);

    buffer_in(buf, term, 1);
    return buf;
}

static Pvoid_t add_url_component(char * uri, Pvoid_t features) {
	if (uri) {
		Word_t *PValue;
		int size = strlen(uri) + 8;
		char * token = malloc(sizeof(char) * size);
		strncpy(token, "URLSeg:", size);
		strncat(token, uri, size);
		JSLI(PValue, features, (unsigned char*) token);
		(*PValue)++;
		free(token);
	}

	return features;
}

static Pvoid_t tokenize_uri(const char * uristr, Pvoid_t features) {
	xmlURIPtr uri = xmlParseURI(uristr);
	if (uri) {
		features = add_url_component(uri->path, features);

		if (uri->server) {
			replace(uri->server, strlen(uri->server), "www\\.", "");
			features = add_url_component(uri->server, features);
		}

		xmlFreeURI(uri);
	}

	return features;
}

static Pvoid_t tokenize_uris(htmlDocPtr doc, Pvoid_t features) {
	xmlTextReaderPtr reader = xmlReaderWalker(doc);

	while (xmlTextReaderRead(reader)) {
		int type = xmlTextReaderNodeType(reader);
		if (type == XML_ELEMENT_NODE) {
			char *uri = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST "href");
			if (uri) {
				features = tokenize_uri(uri, features);
				free(uri);
			}

			uri = (char*) xmlTextReaderGetAttribute(reader, BAD_CAST "src");
			if (uri) {
				features = tokenize_uri(uri, features);
				free(uri);
			}
		}
	}

	xmlFreeTextReader(reader);

	return features;
}

Pvoid_t html_tokenize_into_features(const char * html, Pvoid_t features) {
	xmlSubstituteEntitiesDefault(0);
	htmlDocPtr doc = htmlParseDoc(BAD_CAST html, "UTF-8");

	if (doc) {
		Buffer *buf = extractText(doc);
		features = tokenize_text(buf->buf, buf->length, features);
		features = tokenize_uris(doc, features);
		free_buffer(buf);
		xmlFreeDoc(doc);
	}

	return features;
}

/** Tokenize a string of HTML.
 *
 * @param html The HTML string.
 * @return an Array of Features.
 */
Pvoid_t html_tokenize(const char * html) {
	return html_tokenize_into_features(html, NULL);
}

Pvoid_t atom_tokenize(const char * atom) {
	Pvoid_t features = NULL;

	if (atom) {
		xmlDocPtr doc = xmlParseDoc(BAD_CAST atom);
		if (doc) {
			xmlXPathContextPtr context = xmlXPathNewContext(doc);
			xmlXPathRegisterNs(context, BAD_CAST "atom", BAD_CAST "http://www.w3.org/2005/Atom");

			char *html = get_element_value(context, "/atom:entry/atom:content/text()");
			if (html) {
				features = html_tokenize_into_features(html, features);
				xmlFree(html);
			}

			char *title = get_element_value(context, "/atom:entry/atom:title/text()");
			if (title) {
				features = tokenize_text(title, strlen(title), features);
				xmlFree(title);
			}

			char *author = get_element_value(context, "/atom:entry/atom:author/atom:name/text()");
			if (author) {
				features = add_token(author, features);
				xmlFree(author);
			}

			char *link = get_attribute_value(context, "/atom:entry/atom:link[@rel='alternate']", "href");
			if (link) {
				features = tokenize_uri(link, features);
				xmlFree(link);
			}

			xmlXPathFreeContext(context);
		}

		xmlFreeDoc(doc);
	}

	return features;
}
