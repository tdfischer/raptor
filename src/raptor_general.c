/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * raptor_general.c - Raptor general routines
 *
 * $Id$
 *
 * Copyright (C) 2000-2004, David Beckett http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology http://www.ilrt.bristol.ac.uk/
 * University of Bristol, UK http://www.bristol.ac.uk/
 * 
 * This package is Free Software and part of Redland http://librdf.org/
 * 
 * It is licensed under the following three licenses as alternatives:
 *   1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
 *   2. GNU General Public License (GPL) V2 or any newer version
 *   3. Apache License, V2.0 or any newer version
 * 
 * You may not use this file except in compliance with at least one of
 * the above three licenses.
 * 
 * See LICENSE.html or LICENSE.txt at the top of this package for the
 * complete terms and further detail along with the license texts for
 * the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
 * 
 * 
 */


#ifdef HAVE_CONFIG_H
#include <raptor_config.h>
#endif

#ifdef WIN32
#include <win32_raptor_config.h>
#endif


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

/* Raptor includes */
#include "raptor.h"
#include "raptor_internal.h"


/* prototypes for helper functions */
static void raptor_print_statement_part_as_ntriples(FILE* stream, const void *term, raptor_identifier_type type, raptor_uri* literal_datatype, const unsigned char *literal_language);


/* statics */
static int raptor_initialised;


const char * const raptor_short_copyright_string = "Copyright (C) 2000-2004 David Beckett, ILRT, University of Bristol";

const char * const raptor_copyright_string = "Copyright (C) 2000-2004 David Beckett - http://purl.org/net/dajobe/\nInstitute for Learning and Research Technology - http://www.ilrt.bristol.ac.uk/,\nUniversity of Bristol - http://www.bristol.ac.uk/";

const char * const raptor_version_string = VERSION;

const unsigned int raptor_version_major = RAPTOR_VERSION_MAJOR;
const unsigned int raptor_version_minor = RAPTOR_VERSION_MINOR;
const unsigned int raptor_version_release = RAPTOR_VERSION_RELEASE;

const unsigned int raptor_version_decimal = RAPTOR_VERSION_DECIMAL;



/**
 * raptor_init - Initialise the raptor library
 * 
 * Initialises the library.
 *
 * MUST be called before using any of the raptor APIs.
 **/
void
raptor_init(void) 
{
  if(raptor_initialised)
    return;

#ifdef RAPTOR_PARSER_RSS
  raptor_init_parser_rss();
#endif
#ifdef RAPTOR_PARSER_TURTLE
  raptor_init_parser_turtle();
#endif
#ifdef RAPTOR_PARSER_NTRIPLES
  raptor_init_parser_ntriples();
  raptor_init_serializer_ntriples();
#endif
#ifdef RAPTOR_PARSER_RDFXML
  raptor_init_parser_rdfxml();
  raptor_init_serializer_rdfxml();
#endif
  raptor_init_serializer_simple();

  raptor_uri_init();
  raptor_www_init();

  raptor_initialised=1;
}


/**
 * raptor_finish - Terminate the raptor library
 *
 * Cleans up state of the library.
 **/
void
raptor_finish(void) 
{
  if(!raptor_initialised)
    return;

  raptor_www_finish();
  raptor_delete_parser_factories();
  raptor_delete_serializer_factories();

  raptor_initialised=0;
}



/* 
 * Thanks to the patch in this Debian bug for the solution
 * to the crash inside vsnprintf on some architectures.
 *
 * "reuse of args inside the while(1) loop is in violation of the
 * specs and only happens to work by accident on other systems."
 *
 * http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=104325 
 */

#ifndef va_copy
#ifdef __va_copy
#define va_copy(dest,src) __va_copy(dest,src)
#else
#define va_copy(dest,src) (dest) = (src)
#endif
#endif

/* Compatiblity wrapper */
char*
raptor_vsnprintf(const char *message, va_list arguments) 
{
  char empty_buffer[1];
  int len;
  char *buffer=NULL;
  va_list args_copy;

#ifdef HAVE_C99_VSNPRINTF
  /* copy for re-use */
  va_copy(args_copy, arguments);
  len=vsnprintf(empty_buffer, 1, message, args_copy)+1;
  va_end(args_copy);

  if(len<=0)
    return NULL;
  
  buffer=(char*)RAPTOR_MALLOC(cstring, len);
  if(buffer) {
    /* copy for re-use */
    va_copy(args_copy, arguments);
    vsnprintf(buffer, len, message, args_copy);
    va_end(args_copy);
  }
#else
  /* This vsnprintf doesn't return number of bytes required */
  int size=2;
      
  while(1) {
    buffer=(char*)RAPTOR_MALLOC(cstring, size+1);
    if(!buffer)
      break;
    
    /* copy for re-use */
    va_copy(args_copy, arguments);
    len=vsnprintf(buffer, size, message, args_copy);
    va_end(args_copy);

    if(len>=0)
      break;
    RAPTOR_FREE(cstring, buffer);
    size+=4;
  }
#endif

  return buffer;
}


/* wrapper */
const char*
raptor_basename(const char *name)
{
  char *p;
  if((p=strrchr(name, '/')))
    name=p+1;
  else if((p=strrchr(name, '\\')))
    name=p+1;

  return name;
}


const char * const raptor_xml_literal_datatype_uri_string="http://www.w3.org/1999/02/22-rdf-syntax-ns#XMLLiteral";
const unsigned int raptor_xml_literal_datatype_uri_string_len=53;


/**
 * raptor_print_statement - Print a raptor_statement to a stream
 * @statement: &raptor_statement object to print
 * @stream: &FILE* stream
 *
 **/
void
raptor_print_statement(const raptor_statement * statement, FILE *stream) 
{
  fputc('[', stream);

  if(statement->subject_type == RAPTOR_IDENTIFIER_TYPE_ANONYMOUS) {
    fputs((const char*)statement->subject, stream);
  } else {
#ifdef RAPTOR_DEBUG
    if(!statement->subject)
      RAPTOR_FATAL1("Statement has NULL subject URI\n");
#endif
    fputs((const char*)raptor_uri_as_string((raptor_uri*)statement->subject), stream);
  }

  fputs(", ", stream);

  if(statement->predicate_type == RAPTOR_IDENTIFIER_TYPE_ORDINAL)
    fprintf(stream, "[rdf:_%d]", *((int*)statement->predicate));
  else {
#ifdef RAPTOR_DEBUG
    if(!statement->predicate)
      RAPTOR_FATAL1("Statement has NULL predicate URI\n");
#endif
    fputs((const char*)raptor_uri_as_string((raptor_uri*)statement->predicate), stream);
  }

  fputs(", ", stream);

  if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_LITERAL || 
     statement->object_type == RAPTOR_IDENTIFIER_TYPE_XML_LITERAL) {
    if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_XML_LITERAL) {
      fputc('<', stream);
      fputs(raptor_xml_literal_datatype_uri_string, stream);
      fputc('>', stream);
    } else if(statement->object_literal_datatype) {
      fputc('<', stream);
      fputs((const char*)raptor_uri_as_string((raptor_uri*)statement->object_literal_datatype), stream);
      fputc('>', stream);
    }
    fputc('"', stream);
    fputs((const char*)statement->object, stream);
    fputc('"', stream);
  } else if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_ANONYMOUS)
    fputs((const char*)statement->object, stream);
  else if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_ORDINAL)
    fprintf(stream, "[rdf:_%d]", *((int*)statement->object));
  else {
#ifdef RAPTOR_DEBUG
    if(!statement->object)
      RAPTOR_FATAL1("Statement has NULL object URI\n");
#endif
    fputs((const char*)raptor_uri_as_string((raptor_uri*)statement->object), stream);
  }

  fputc(']', stream);
}


void
raptor_print_statement_detailed(const raptor_statement * statement, 
                                int detailed, FILE *stream) 
{
  raptor_print_statement(statement, stream);
}


/**
 * raptor_print_ntriples_string - Print an UTF-8 string using N-Triples escapes
 * @stream: FILE* stream to print to
 * @string: UTF-8 string to print
 * @delim: Delimiter character for string (such as ") or \0 for no delim
 * escaping.
 * 
 * Return value: non-0 on failure such as bad UTF-8 encoding.
 **/
int
raptor_print_ntriples_string(FILE *stream,
                             const unsigned char *string,
                             const char delim) 
{
  unsigned char c;
  size_t len=strlen((const char*)string);
  int unichar_len;
  unsigned long unichar;
  
  for(; (c=*string); string++, len--) {
    if((delim && c == delim) || c == '\\') {
      fprintf(stream, "\\%c", c);
      continue;
    }
    
    /* Note: NTriples is ASCII */
    if(c == 0x09) {
      fputs("\\t", stream);
      continue;
    } else if(c == 0x0a) {
      fputs("\\n", stream);
      continue;
    } else if(c == 0x0d) {
      fputs("\\r", stream);
      continue;
    } else if(c < 0x20|| c == 0x7f) {
      fprintf(stream, "\\u%04X", c);
      continue;
    } else if(c < 0x80) {
      fputc(c, stream);
      continue;
    }
    
    /* It is unicode */
    
    unichar_len=raptor_utf8_to_unicode_char(NULL, string, len);
    if(unichar_len < 0 || unichar_len > (int)len)
      /* UTF-8 encoding had an error or ended in the middle of a string */
      return 1;

    unichar_len=raptor_utf8_to_unicode_char(&unichar, string, len);
    
    if(unichar < 0x10000)
      fprintf(stream, "\\u%04lX", unichar);
    else
      fprintf(stream, "\\U%08lX", unichar);
    
    unichar_len--; /* since loop does len-- */
    string += unichar_len; len -= unichar_len;

  }

  return 0;
}


/**
 * raptor_statement_part_as_counted_string - Turns part of raptor statement into a N-Triples format counted string
 * @term: &raptor_statement part (subject, predicate, object)
 * @type: &raptor_statement part type
 * @literal_datatype: &raptor_statement part datatype
 * @literal_language: &raptor_statement part language
 * @len_p: Pointer to location to store length of new string (if not NULL)
 * 
 * Turns the given @term into an N-Triples escaped string using all the
 * escapes as defined in http://www.w3.org/TR/rdf-testcases/#ntriples
 *
 * The part (subject, predicate, object) of the raptor_statement is
 * typically passed in as @term, the part type (subject_type,
 * predicate_type, object_type) is passed in as @type.  When the part
 * is a literal, the @literal_datatype and @literal_language fields
 * are set, otherwise NULL (usually object_datatype,
 * object_literal_language).
 *
 * Return value: the new string or NULL on failure.  The length of
 * the new string is returned in *&len_p if len_p is not NULL.
 **/
unsigned char*
raptor_statement_part_as_counted_string(const void *term, 
                                        raptor_identifier_type type,
                                        raptor_uri* literal_datatype,
                                        const unsigned char *literal_language,
                                        size_t* len_p)
{
  size_t len, term_len, language_len, uri_len;
  unsigned char *s, *buffer, *uri_string;
  
  switch(type) {
    case RAPTOR_IDENTIFIER_TYPE_LITERAL:
    case RAPTOR_IDENTIFIER_TYPE_XML_LITERAL:
      term_len=strlen((const char*)term);
      len=2+term_len;
      if(literal_language && type == RAPTOR_IDENTIFIER_TYPE_LITERAL) {
        language_len=strlen((const char*)literal_language);
        len+= language_len+1;
      }
      if(type == RAPTOR_IDENTIFIER_TYPE_XML_LITERAL)
        len += 4+raptor_xml_literal_datatype_uri_string_len;
      else if(literal_datatype) {
        uri_string=raptor_uri_as_counted_string((raptor_uri*)literal_datatype, &uri_len);
        len += 4+uri_len;
      }
  
      buffer=(unsigned char*)RAPTOR_MALLOC(cstring, len+1);
      if(!buffer)
        return NULL;

      s=buffer;
      *s++ ='"';
      /* raptor_print_ntriples_string(stream, (const char*)term, '"'); */
      strcpy((char*)s, (const char*)term);
      s+= term_len;
      *s++ ='"';
      if(literal_language && type == RAPTOR_IDENTIFIER_TYPE_LITERAL) {
        *s++ ='@';
        strcpy((char*)s, (const char*)literal_language);
        s+= language_len;
      }

      if(type == RAPTOR_IDENTIFIER_TYPE_XML_LITERAL) {
        *s++ ='^';
        *s++ ='^';
        *s++ ='<';
        strcpy((char*)s, raptor_xml_literal_datatype_uri_string);
        s+= raptor_xml_literal_datatype_uri_string_len;
        *s++ ='>';
      } else if(literal_datatype) {
        *s++ ='^';
        *s++ ='^';
        *s++ ='<';
        strcpy((char*)s, (const char*)uri_string);
        s+= uri_len;
        *s++ ='>';
      }
      *s++ ='\0';
      
      break;
      
    case RAPTOR_IDENTIFIER_TYPE_ANONYMOUS:
      len=2+strlen((const char*)term);
      buffer=(unsigned char*)RAPTOR_MALLOC(cstring, len+1);
      if(!buffer)
        return NULL;
      s=buffer;
      *s++ ='_';
      *s++ =':';
      strcpy((char*)s, (const char*)term);
      break;
      
    case RAPTOR_IDENTIFIER_TYPE_ORDINAL:
      /* FIXME - 46 for "<http://www.w3.org/1999/02/22-rdf-syntax-ns#_>" */
      len=46 + 13; 
      buffer=(unsigned char*)RAPTOR_MALLOC(cstring, len+1);
      if(!buffer)
        return NULL;

      sprintf((char*)buffer,
              "<http://www.w3.org/1999/02/22-rdf-syntax-ns#_%d>",
              *((int*)term));
      break;
  
    case RAPTOR_IDENTIFIER_TYPE_RESOURCE:
    case RAPTOR_IDENTIFIER_TYPE_PREDICATE:
      uri_string=raptor_uri_as_counted_string((raptor_uri*)term, &uri_len);
      len=2+uri_len;
      buffer=(unsigned char*)RAPTOR_MALLOC(cstring, len+1);
      if(!buffer)
        return NULL;

      s=buffer;
      *s++ ='<';
      /* raptor_print_ntriples_string(stream, raptor_uri_as_string((raptor_uri*)term), '\0'); */
      strcpy((char*)s, (const char*)uri_string);
      s+= uri_len;
      *s++ ='>';
      *s++ ='\0';
      break;
      
    default:
      RAPTOR_FATAL2("Unknown type %d", type);
  }

  if(len_p)
    *len_p=len;
  
 return buffer;
}


/**
 * raptor_statement_part_as_string - Turns part of raptor statement into a N-Triples format string
 * @term: &raptor_statement part (subject, predicate, object)
 * @type: &raptor_statement part type
 * @literal_datatype: &raptor_statement part datatype
 * @literal_language: &raptor_statement part language
 * 
 * Turns the given @term into an N-Triples escaped string using all the
 * escapes as defined in http://www.w3.org/TR/rdf-testcases/#ntriples
 *
 * The part (subject, predicate, object) of the raptor_statement is
 * typically passed in as @term, the part type (subject_type,
 * predicate_type, object_type) is passed in as @type.  When the part
 * is a literal, the @literal_datatype and @literal_language fields
 * are set, otherwise NULL (usually object_datatype,
 * object_literal_language).
 *
 * Return value: the new string or NULL on failure.
 **/
unsigned char*
raptor_statement_part_as_string(const void *term, 
                                raptor_identifier_type type,
                                raptor_uri* literal_datatype,
                                const unsigned char *literal_language) {
     return raptor_statement_part_as_counted_string(term, type,
                                                    literal_datatype,
                                                    literal_language,
                                                    NULL);
}


static void
raptor_print_statement_part_as_ntriples(FILE* stream,
                                        const void *term, 
                                        raptor_identifier_type type,
                                        raptor_uri* literal_datatype,
                                        const unsigned char *literal_language) 
{
  switch(type) {
    case RAPTOR_IDENTIFIER_TYPE_LITERAL:
    case RAPTOR_IDENTIFIER_TYPE_XML_LITERAL:
      fputc('"', stream);
      raptor_print_ntriples_string(stream, (const unsigned char*)term, '"');
      fputc('"', stream);
      if(literal_language && type == RAPTOR_IDENTIFIER_TYPE_LITERAL) {
        fputc('@', stream);
        fputs((const char*)literal_language, stream);
      }
      if(type == RAPTOR_IDENTIFIER_TYPE_XML_LITERAL) {
        fputs("^^<", stream);
        fputs(raptor_xml_literal_datatype_uri_string, stream);
        fputc('>', stream);
      } else if(literal_datatype) {
        fputs("^^<", stream);
        fputs((const char*)raptor_uri_as_string((raptor_uri*)literal_datatype), stream);
        fputc('>', stream);
      }

      break;
      
    case RAPTOR_IDENTIFIER_TYPE_ANONYMOUS:
      fputs("_:", stream);
      fputs((const char*)term, stream);
      break;
      
    case RAPTOR_IDENTIFIER_TYPE_ORDINAL:
      fprintf(stream, "<http://www.w3.org/1999/02/22-rdf-syntax-ns#_%d>",
              *((int*)term));
      break;
  
    case RAPTOR_IDENTIFIER_TYPE_RESOURCE:
    case RAPTOR_IDENTIFIER_TYPE_PREDICATE:
      fputc('<', stream);
      raptor_print_ntriples_string(stream, raptor_uri_as_string((raptor_uri*)term), '\0');
      fputc('>', stream);
      break;
      
    default:
      RAPTOR_FATAL2("Unknown type %d", type);
  }
}


/**
 * raptor_print_statement_as_ntriples - Print a raptor_statement in N-Triples form
 * @statement: &raptor_statement to print
 * @stream: &FILE* stream
 * 
 **/
void
raptor_print_statement_as_ntriples(const raptor_statement * statement,
                                   FILE *stream) 
{
  raptor_print_statement_part_as_ntriples(stream,
                                          statement->subject,
                                          statement->subject_type,
                                          NULL, NULL);
  fputc(' ', stream);
  raptor_print_statement_part_as_ntriples(stream,
                                          statement->predicate,
                                          statement->predicate_type,
                                          NULL, NULL);
  fputc(' ', stream);
  raptor_print_statement_part_as_ntriples(stream,
                                          statement->object,
                                          statement->object_type,
                                          statement->object_literal_datatype,
                                          statement->object_literal_language);
  fputs(" .", stream);
}



int
raptor_check_ordinal(const unsigned char *name) {
  int ordinal= -1;
  unsigned char c;

  while((c=*name++)) {
    if(c < '0' || c > '9')
      return -1;
    if(ordinal <0)
      ordinal=0;
    ordinal *= 10;
    ordinal += (c - '0');
  }
  return ordinal;
}


/**
 * raptor_free_memory - Free memory allocated inside raptor.
 * @ptr: memory pointer
 * 
 * Some systems require memory allocated in a library to
 * be deallocated in that library.  This function allows
 * memory allocated by raptor to be freed.
 *
 * Examples include the result of the '_to_' methods that returns
 * allocated memory such as raptor_uri_filename_to_uri_string,
 * raptor_uri_filename_to_uri_string
 * and raptor_uri_uri_string_to_filename_fragment
 *
 **/
void
raptor_free_memory(void *ptr)
{
  RAPTOR_FREE(void, ptr);
}


/**
 * raptor_alloc_memory - Allocate memory inside raptor.
 * @size: size of memory to allocate
 * 
 * Some systems require memory allocated in a library to
 * be deallocated in that library.  This function allows
 * memory to be allocated inside the raptor shared library
 * that can be freed inside raptor either internally or via
 * raptor_free_memory.
 *
 * Examples include using this in the raptor_generate_id handler
 * code to create new strings that will be used internally
 * as short identifiers and freed later on by the parsers.
 *
 * Return value: the address of the allocated memory or NULL on failure
 *
 **/
void*
raptor_alloc_memory(size_t size)
{
  return RAPTOR_MALLOC(void, size);
}


/**
 * raptor_calloc_memory - Allocate zeroed array of items inside raptor.
 * @nmemb: number of members
 * @size: size of item
 * 
 * Some systems require memory allocated in a library to
 * be deallocated in that library.  This function allows
 * memory to be allocated inside the raptor shared library
 * that can be freed inside raptor either internally or via
 * raptor_free_memory.
 *
 * Examples include using this in the raptor_generate_id handler
 * code to create new strings that will be used internally
 * as short identifiers and freed later on by the parsers.
 *
 * Return value: the address of the allocated memory or NULL on failure
 *
 **/
void*
raptor_calloc_memory(size_t nmemb, size_t size)
{
  return RAPTOR_CALLOC(void, nmemb, size);
}


#if defined (RAPTOR_DEBUG) && defined(HAVE_DMALLOC_H) && defined(RAPTOR_MEMORY_DEBUG_DMALLOC)

#undef malloc
void*
raptor_system_malloc(size_t size)
{
  return malloc(size);
}

#undef free
void
raptor_system_free(void *ptr)
{
  free(ptr);
}

#endif
