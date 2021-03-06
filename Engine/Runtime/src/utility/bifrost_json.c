/******************************************************************************/
/*!
* @file   bf_json.c
* @author Shareef Abdoul-Raheem (https://blufedora.github.io/)
* @brief
*   Basic Json parser with an Event (SAX) API.
*   Has some extensions to make wrting json easier.
*   Just search for '@JsonSpecExtension' in the source file.
*
* @version 0.0.1
* @date    2020-03-14
*
* @copyright Copyright (c) 2020-2021
*/
/******************************************************************************/
#include "bf/utility/bifrost_json.h"

#include <ctype.h>  /* isspace, isalpha, isdigit, tolower, isxdigit */
#include <float.h>  /* DBL_DECIMAL_DIG                              */
#include <setjmp.h> /* jmp_buf, setjmp, longjmp                     */
#include <stdio.h>  /* snprintf                                     */
#include <stdlib.h> /* strtol, strtod, malloc                       */

/*
  Reader API
*/

typedef enum bfJsonTokenType
{
  BF_JSON_L_CURLY   = '{',
  BF_JSON_R_CURLY   = '}',
  BF_JSON_L_SQR_BOI = '[',
  BF_JSON_R_SQR_BOI = ']',
  BF_JSON_COMMA     = ',',
  BF_JSON_QUOTE     = '"',
  BF_JSON_COLON     = ':',
  BF_JSON_TRUE      = 't',
  BF_JSON_FALSE     = 'f',
  BF_JSON_NULL      = 'n',
  BF_JSON_NUMBER    = '#',
  BF_JSON_EOF       = '!',

} bfJsonTokenType;

typedef struct bfJsonToken
{
  bfJsonTokenType type;
  const char*     source_bgn;
  const char*     source_end;

} bfJsonToken;

typedef struct bfJsonParserContext
{
  char*       source_bgn;
  const char* source_end;
  bfJsonToken key_token;
  bfJsonToken val_token;
  char*       current_location;
  size_t      line_no;
  bfJsonFn    callback;
  void*       user_data;
  char        array_idx_number_buffer[26];
  char        error_message[k_bfErrorBufferSize];
  size_t      error_message_len;
  jmp_buf     error_handling;

} bfJsonParserContext;

static bool   bfJsonIsSpace(char character);
static bool   bfJsonIsSameCharacterI(char a, char b);
static bool   bfJsonIsSameStringI(const char* a, const char* b, size_t n);
static bool   bfJsonIs(bfJsonParserContext* ctx, bfJsonTokenType type);
static char   bfJsonCurrentChar(const bfJsonParserContext* ctx);
static void   bfJsonIncrement(bfJsonParserContext* ctx);
static void   bfJsonSkipSpace(bfJsonParserContext* ctx);
static bool   bfJsonIsKeyword(const bfJsonParserContext* ctx);
static void   bfJsonSkipKeyword(bfJsonParserContext* ctx);
static bool   bfJsonIsDigit(const bfJsonParserContext* ctx);
static bool   bfJsonIsNumber(const bfJsonParserContext* ctx);
static void   bfJsonSkipNumber(bfJsonParserContext* ctx);
static void   bfJsonSkipString(bfJsonParserContext* ctx);
static char   bfJsonUnescapeStringHelper(char character, const char** inc);
static size_t bfJsonUnescapeString(char* str);
static void   bfJsonSetToken(bfJsonParserContext* ctx, bfJsonTokenType type, const char* bgn, const char* end);
static void   bfJsonNextToken(bfJsonParserContext* ctx);
static bool   bfJsonEat(bfJsonParserContext* ctx, bfJsonTokenType type, bool optional);
static void   bfJsonInterpret(bfJsonParserContext* ctx);

void bfJsonParser_fromString(char* source, size_t source_length, bfJsonFn callback, void* user_data)
{
  bfJsonParserContext ctx;

  ctx.source_bgn           = source;
  ctx.source_end           = source + source_length;
  ctx.current_location     = source;
  ctx.line_no              = 1;
  ctx.callback             = callback;
  ctx.val_token.type       = BF_JSON_EOF;
  ctx.val_token.source_bgn = NULL;
  ctx.val_token.source_end = NULL;
  ctx.user_data            = user_data;

  if (setjmp(ctx.error_handling) == 0)
  {
    bfJsonEat(&ctx, BF_JSON_EOF, false);
    callback(&ctx, BF_JSON_EVENT_BEGIN_DOCUMENT, user_data);
    bfJsonInterpret(&ctx);
    callback(&ctx, BF_JSON_EVENT_END_DOCUMENT, user_data);
  }
}

bfJsonString bfJsonParser_errorMessage(const bfJsonParserContext* ctx)
{
  bfJsonString result;

  result.string = ctx->error_message;
  result.length = ctx->error_message_len;

  return result;
}

bfJsonString bfJsonParser_key(const bfJsonParserContext* ctx)
{
  bfJsonString result;

  result.string = ctx->key_token.source_bgn;
  result.length = ctx->key_token.source_end - ctx->key_token.source_bgn;

  return result;
}

bfJsonValueType bfJsonParser_valueType(const bfJsonParserContext* ctx)
{
  switch (ctx->val_token.type)
  {
    case BF_JSON_QUOTE:
      return BF_JSON_VALUE_STRING;
    case BF_JSON_TRUE:
    case BF_JSON_FALSE:
      return BF_JSON_VALUE_BOOLEAN;
    case BF_JSON_NULL:
      return BF_JSON_VALUE_NULL;
    case BF_JSON_NUMBER:
      return BF_JSON_VALUE_NUMBER;
    case BF_JSON_L_CURLY:
    case BF_JSON_L_SQR_BOI:
    case BF_JSON_R_CURLY:
    case BF_JSON_R_SQR_BOI:
    case BF_JSON_COMMA:
    case BF_JSON_COLON:
    case BF_JSON_EOF:
    default:
      return BF_JSON_VALUE_STRING;
  }
}

bool bfJsonParser_valueIs(const bfJsonParserContext* ctx, bfJsonValueType type)
{
  return bfJsonParser_valueType(ctx) == type;
}

bfJsonString bfJsonParser_valAsString(const bfJsonParserContext* ctx)
{
  bfJsonString result;

  result.string = ctx->val_token.source_bgn;
  result.length = ctx->val_token.source_end - ctx->val_token.source_bgn;

  return result;
}

double bfJsonParser_valAsNumber(const bfJsonParserContext* ctx)
{
  return strtod(ctx->val_token.source_bgn, NULL);
}

bool bfJsonParser_valAsBoolean(const bfJsonParserContext* ctx)
{
  return ctx->val_token.type != BF_JSON_FALSE;
}

/*
  Writer API
*/

struct bfJsonStringBlock
{
  char                      string[k_bfJsonStringBlockSize];
  size_t                    string_length;
  struct bfJsonStringBlock* next;
};

typedef struct bfJsonWriter
{
  void*              user_data;
  bfJsonAllocFn      alloc_fn;
  bfJsonStringBlock  base_block;
  bfJsonStringBlock* current_block;
  size_t             string_size;

} bfJsonWriter;

bfJsonWriter* bfJsonWriter_new(bfJsonAllocFn alloc_fn, void* user_data)
{
  bfJsonWriter* self = alloc_fn(sizeof(bfJsonWriter), user_data);

  self->user_data                = user_data;
  self->alloc_fn                 = alloc_fn;
  self->base_block.string_length = 0;
  self->base_block.next          = NULL;
  self->current_block            = &self->base_block;
  self->string_size              = 0;

  return self;
}

static void* mallocWrapper(size_t size, void* _)
{
  (void)_;

  return malloc(size);
}

static void freeWrapper(void* ptr, void* _)
{
  (void)_;

  free(ptr);
}

bfJsonWriter* bfJsonWriter_newCRTAlloc(void)
{
  return bfJsonWriter_new(&mallocWrapper, NULL);
}

size_t bfJsonWriter_length(const bfJsonWriter* self)
{
  return self->string_size;
}

void bfJsonWriter_beginArray(bfJsonWriter* self)
{
  bfJsonWriter_write(self, "[", 1);
}

void bfJsonWriter_endArray(bfJsonWriter* self)
{
  bfJsonWriter_write(self, "]", 1);
}

void bfJsonWriter_beginObject(bfJsonWriter* self)
{
  bfJsonWriter_write(self, "{", 1);
}

void bfJsonWriter_key(bfJsonWriter* self, bfJsonString key)
{
  bfJsonWriter_valueString(self, key);
  bfJsonWriter_write(self, " : ", 3);
}

void bfJsonWriter_valueString(bfJsonWriter* self, bfJsonString value)
{
  bfJsonWriter_write(self, "\"", 1);

  const char* const value_end = value.string + value.length;

  for (const char* it = value.string; it != value_end; ++it)
  {
    const char* write;
    size_t      write_length;

    switch (it[0])
    {
      case '"':
      {
        write        = "\\\"";
        write_length = 2;
        break;
      }
      case '\n':
      {
        write        = "\\n";
        write_length = 2;
        break;
      }
      case '\r':
      {
        write        = "\\r";
        write_length = 2;
        break;
      }
      case '\t':
      {
        write        = "\\t";
        write_length = 2;
        break;
      }
      case '\\':
      {
        write        = "\\\\";
        write_length = 2;
        break;
      }
      default:
      {
        write        = it;
        write_length = 1;
        break;
      }
    }

    bfJsonWriter_write(self, write, write_length);
  }

  bfJsonWriter_write(self, "\"", 1);
}

void bfJsonWriter_valueNumber(bfJsonWriter* self, double value)
{
  char         number_buffer[26];
  const size_t number_buffer_length = (size_t)snprintf(number_buffer, sizeof(number_buffer), "%.*g", DBL_DECIMAL_DIG, value);

  bfJsonWriter_write(self, number_buffer, number_buffer_length);
}

void bfJsonWriter_valueBoolean(bfJsonWriter* self, bool value)
{
  if (value)
  {
    bfJsonWriter_write(self, "true", 4);
  }
  else
  {
    bfJsonWriter_write(self, "false", 5);
  }
}

void bfJsonWriter_valueNull(bfJsonWriter* self)
{
  bfJsonWriter_write(self, "null", 4);
}

void bfJsonWriter_next(bfJsonWriter* self)
{
  bfJsonWriter_write(self, ",", 1);
}

void bfJsonWriter_indent(bfJsonWriter* self, int num_spaces)
{
  for (int i = 0; i < num_spaces; ++i)
  {
    bfJsonWriter_write(self, " ", 1);
  }
}

void bfJsonWriter_write(bfJsonWriter* self, const char* str, size_t length)
{
  while (length > 0)
  {
    const size_t bytes_left = k_bfJsonStringBlockSize - self->current_block->string_length;

    if (bytes_left < length)
    {
      for (size_t i = 0; i < bytes_left; ++i)
      {
        self->current_block->string[self->current_block->string_length++] = str[i];
      }

      bfJsonStringBlock* const new_block = self->alloc_fn(sizeof(bfJsonStringBlock), self->user_data);
      new_block->next                    = NULL;
      new_block->string_length           = 0;

      self->current_block->next = new_block;
      self->current_block       = new_block;

      str += bytes_left;
      length -= bytes_left;
      self->string_size += bytes_left;
      continue;
    }

    for (size_t i = 0; i < length; ++i)
    {
      self->current_block->string[self->current_block->string_length++] = str[i];
    }

    self->string_size += length;
    break;
  }
}

void bfJsonWriter_endObject(bfJsonWriter* self)
{
  bfJsonWriter_write(self, "}", 1);
}

void bfJsonWriter_forEachBlock(const bfJsonWriter* self, bfJsonWriterForEachFn fn, void* user_data)
{
  const bfJsonStringBlock* current = &self->base_block;

  while (current)
  {
    const bfJsonStringBlock* next = current->next;
    fn(current, user_data);
    current = next;
  }
}

void bfJsonWriter_delete(bfJsonWriter* self, bfJsonFreeFn free_fn)
{
  bfJsonStringBlock* current = self->base_block.next;

  while (current)
  {
    bfJsonStringBlock* next = current->next;
    free_fn(current, self->user_data);
    current = next;
  }

  free_fn(self, self->user_data);
}

void bfJsonWriter_deleteCRT(bfJsonWriter* self)
{
  bfJsonWriter_delete(self, &freeWrapper);
}

bfJsonString bfJsonStringBlock_string(const bfJsonStringBlock* block)
{
  bfJsonString result;

  result.string = block->string;
  result.length = block->string_length;

  return result;
}

/*
  Private Helpers
*/

/* Reader */
static bool bfJsonIsSpace(char character)
{
  return isspace(character);
}

static bool bfJsonIsSameCharacterI(char a, char b)
{
  return tolower(a) == tolower(b);
}

static bool bfJsonIsSameStringI(const char* a, const char* b, size_t n)
{
  for (size_t i = 0; i < n; ++i)
  {
    if (!bfJsonIsSameCharacterI(a[i], b[i]))
    {
      return false;
    }
  }

  return true;
}

static bool bfJsonIs(bfJsonParserContext* ctx, bfJsonTokenType type)
{
  return ctx->val_token.type == type;
}

static char bfJsonCurrentChar(const bfJsonParserContext* ctx)
{
  return ctx->current_location[0];
}

static void bfJsonIncrement(bfJsonParserContext* ctx)
{
  if (ctx->current_location < ctx->source_end)
  {
    ++ctx->current_location;

    if (bfJsonCurrentChar(ctx) == '\n' || bfJsonCurrentChar(ctx) == '\r')
    {
      ++ctx->line_no;
    }
  }
}

static void bfJsonSkipSpace(bfJsonParserContext* ctx)
{
  while (ctx->current_location < ctx->source_end && bfJsonIsSpace(bfJsonCurrentChar(ctx)))
  {
    bfJsonIncrement(ctx);
  }
}

static bool bfJsonIsKeyword(const bfJsonParserContext* ctx)
{
  const char character = bfJsonCurrentChar(ctx);

  return character == '(' || character == ')' || character == '_' || isalpha(character);
}

static void bfJsonSkipKeyword(bfJsonParserContext* ctx)
{
  while (ctx->current_location < ctx->source_end && bfJsonIsKeyword(ctx))
  {
    bfJsonIncrement(ctx);
  }
}

static bool bfJsonIsDigit(const bfJsonParserContext* ctx)
{
  const char character = bfJsonCurrentChar(ctx);

  return isdigit(character) ||
         character == '-' ||
         character == '+';
}

static bool bfJsonIsNumber(const bfJsonParserContext* ctx)
{
  const char character = bfJsonCurrentChar(ctx);

  /*
    @JsonSpecExtension
      Added support for other types of numbers such as
      hexadecimal and a trailing 'f' / 'F'.

      Supports anything that can be passed into 'strtod'.
  */

  return bfJsonIsDigit(ctx) ||
         character == '.' ||
         character == 'P' ||
         character == 'p' ||
         character == 'X' ||
         character == 'x' ||
         isxdigit(character); /* 0-9, a-f, A-F */
}

static void bfJsonSkipNumber(bfJsonParserContext* ctx)
{
  while (ctx->current_location < ctx->source_end && bfJsonIsNumber(ctx))
  {
    bfJsonIncrement(ctx);
  }
}

static void bfJsonSkipString(bfJsonParserContext* ctx)
{
  while (ctx->current_location < ctx->source_end && bfJsonCurrentChar(ctx) != BF_JSON_QUOTE)
  {
    const char prev = bfJsonCurrentChar(ctx);
    bfJsonIncrement(ctx);

    if (prev == '\\')
    {
      bfJsonIncrement(ctx);
    }
    else if (bfJsonCurrentChar(ctx) == BF_JSON_QUOTE)
    {
      break;
    }
  }
}

static char bfJsonUnescapeStringHelper(char character, const char** inc)
{
  /*
    @JsonSpecExtension
      Added support for some more escape characters.
  */

  const char* hex_start = *inc + 1;

  *inc += character == 'u' ? 4 : 1;

  switch (character)
  {
    case 'a': return '\a';  /* EXT  */
    case 'b': return '\b';  /* SPEC */
    case 'f': return '\f';  /* SPEC */
    case 'n': return '\n';  /* SPEC */
    case 'r': return '\r';  /* SPEC */
    case 't': return '\t';  /* SPEC */
    case 'v': return '\v';  /* EXT  */
    case '\\': return '\\'; /* SPEC */
    case '\'': return '\''; /* EXT  */
    case '\"': return '\"'; /* SPEC */
    case '/': return '/';   /* SPEC */
    case '?': return '\?';  /* EXT  */
    case 'u':               /* SPEC */
    {
      const char hex_buffer[] =
       {
        hex_start[0],
        hex_start[1],
        hex_start[2],
        hex_start[3],
        '\0',
       };

      return (char)strtoul(hex_buffer, NULL, 16);
    }
    default: return character;
  }
}

static size_t bfJsonUnescapeString(char* str)
{
  const char* old_str = str;
  char*       new_str = str;

  while (*old_str)
  {
    char c = old_str[0];
    ++old_str;

    if (c == '\\')
    {
      c = bfJsonUnescapeStringHelper(old_str[0], &old_str);

      if (c == '\0')
      {
        break;
      }
    }

    *new_str++ = c;
  }

  new_str[0] = '\0';

  return new_str - str;
}

static void bfJsonSetToken(bfJsonParserContext* ctx, bfJsonTokenType type, const char* bgn, const char* end)
{
  ctx->val_token.type       = type;
  ctx->val_token.source_bgn = bgn;
  ctx->val_token.source_end = end;
}

static void bfJsonNextToken(bfJsonParserContext* ctx)
{
  bfJsonSkipSpace(ctx);

  if (ctx->current_location < ctx->source_end)
  {
    if (bfJsonIsKeyword(ctx))
    {
      const char* token_bgn = ctx->current_location;
      bfJsonSkipKeyword(ctx);
      const char* token_end = ctx->current_location;

      /*
        @JsonSpecExtension
          Added support for "inf", "infinity", and "nan" (case insensitive).
      */
      if (token_end - token_bgn >= 3 && (bfJsonIsSameStringI(token_bgn, "INF", 3) || bfJsonIsSameStringI(token_bgn, "NAN", 3)))
      {
        bfJsonSetToken(ctx, BF_JSON_NUMBER, token_bgn, token_end);
      }
      else
      {
        bfJsonSetToken(ctx, token_bgn[0], token_bgn, token_end);
      }
    }
    else if (bfJsonIsDigit(ctx))
    {
      const char* token_bgn = ctx->current_location;
      bfJsonSkipNumber(ctx);
      const char* token_end = ctx->current_location;

      bfJsonSetToken(ctx, BF_JSON_NUMBER, token_bgn, token_end);
    }
    else if (bfJsonCurrentChar(ctx) == BF_JSON_QUOTE)
    {
      bfJsonIncrement(ctx); /* '"' */
      char* token_bgn = ctx->current_location;
      bfJsonSkipString(ctx);
      char* token_end = ctx->current_location;
      token_end[0]    = '\0';
      token_end       = token_bgn + bfJsonUnescapeString(token_bgn);
      bfJsonIncrement(ctx); /* '"' (really '\0') */

      bfJsonSetToken(ctx, BF_JSON_QUOTE, token_bgn, token_end);
    }
    else
    {
      bfJsonSetToken(ctx, ctx->current_location[0], NULL, NULL);
      bfJsonIncrement(ctx);
    }
  }
  else
  {
    bfJsonSetToken(ctx, BF_JSON_EOF, NULL, NULL);
  }
}

static bool bfJsonEat(bfJsonParserContext* ctx, bfJsonTokenType type, bool optional)
{
  if (bfJsonIs(ctx, type))
  {
    bfJsonNextToken(ctx);
    return true;
  }

  if (!optional)
  {
    ctx->error_message_len = snprintf(ctx->error_message,
                                      sizeof(ctx->error_message),
                                      "Line(%i): Expected a '%c' but got a '%c'.",
                                      (int)ctx->line_no,
                                      (char)type,
                                      (char)ctx->val_token.type);
    ctx->callback(ctx, BF_JSON_EVENT_PARSE_ERROR, ctx->user_data);
    longjmp(ctx->error_handling, 1);
    /* return bfFalse; */
  }

  return optional;
}

static void bfJsonInterpret(bfJsonParserContext* ctx)
{
  switch (ctx->val_token.type)
  {
    case BF_JSON_L_CURLY:
    {
      bfJsonEat(ctx, BF_JSON_L_CURLY, false);

      ctx->callback(ctx, BF_JSON_EVENT_BEGIN_OBJECT, ctx->user_data);

      while (!bfJsonIs(ctx, BF_JSON_R_CURLY))
      {
        ctx->key_token = ctx->val_token;
        bfJsonEat(ctx, BF_JSON_QUOTE, false);
        bfJsonEat(ctx, BF_JSON_COLON, false);
        bfJsonInterpret(ctx);

        /*
          @JsonSpecExtension
            Added support for trailing commas (allowed in ES5 though).
            Also commas are optional.
        */
        bfJsonEat(ctx, BF_JSON_COMMA, true);
      }

      ctx->callback(ctx, BF_JSON_EVENT_END_OBJECT, ctx->user_data);
      bfJsonEat(ctx, BF_JSON_R_CURLY, false);
      break;
    }
    case BF_JSON_L_SQR_BOI:
    {
      bfJsonEat(ctx, BF_JSON_L_SQR_BOI, false);
      ctx->callback(ctx, BF_JSON_EVENT_BEGIN_ARRAY, ctx->user_data);

      size_t num_elements = 0u;

      while (!bfJsonIs(ctx, BF_JSON_R_SQR_BOI))
      {
        const size_t number_buffer_length = (size_t)snprintf(ctx->array_idx_number_buffer, sizeof(ctx->array_idx_number_buffer), "%zu", num_elements);

        ctx->key_token.type       = BF_JSON_NUMBER;
        ctx->key_token.source_bgn = ctx->array_idx_number_buffer;
        ctx->key_token.source_end = ctx->key_token.source_bgn + number_buffer_length;

        bfJsonInterpret(ctx);
        ++num_elements;

        /*
          @JsonSpecExtension
            Added support for trailing commas (allowed in ES5 though).
            Also commas are optional.
        */
        bfJsonEat(ctx, BF_JSON_COMMA, true);
      }

      ctx->callback(ctx, BF_JSON_EVENT_END_ARRAY, ctx->user_data);
      bfJsonEat(ctx, BF_JSON_R_SQR_BOI, false);
      break;
    }
    case BF_JSON_QUOTE:
    case BF_JSON_TRUE:
    case BF_JSON_FALSE:
    case BF_JSON_NULL:
    case BF_JSON_NUMBER:
    {
      ctx->callback(ctx, BF_JSON_EVENT_KEY_VALUE, ctx->user_data);
      /* [[fallthough]] */
    }
    case BF_JSON_R_CURLY:
    case BF_JSON_R_SQR_BOI:
    case BF_JSON_COMMA:
    case BF_JSON_COLON:
    case BF_JSON_EOF:
    default:
    {
      bfJsonEat(ctx, ctx->val_token.type, false);
      break;
    }
  }
}

/******************************************************************************/
/*
  MIT License

  Copyright (c) 2020-2021 Shareef Abdoul-Raheem

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/
/******************************************************************************/
