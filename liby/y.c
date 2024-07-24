#include <liby/y.h>
#include <sdk/fs.h>

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

typedef enum token
{
  TOKEN_NONE,

  TOKEN_TEXT,
  TOKEN_NUMBER,
  TOKEN_STRING,

  TOKEN_DOT,
  TOKEN_NOTE,
  TOKEN_OPEN,
  TOKEN_CLOSE,
} token;

typedef struct source_t
{
  uint line;
  char *start, *begin, *end;
} source_t;

typedef struct token_t
{
  token kind;
  source_t source;
  y_value_t value;
} token_t;

typedef struct unit_t
{
  char *data;
  char *start;
  int length;
  int cursor;
  int line;

  token_t previous, current;
} unit_t;

#define C_FATAL "\033[1;31m"
#define C_RESET "\033[0m"

static void msg_line(char *ch, const char *col, char *begin, char *end)
{
  if (!ch)
  {
    printf("Invalid line.\n");
    return;
  }

  while (*ch)
  {
    if (*ch == '\n')
      break;

    if (ch == begin)
    {
      fprintf(stderr, "%s", col);
    }

    fputc(*ch, stderr);
    ch++;

    if (ch == end)
    {
      fprintf(stderr, C_RESET);
    }
  }

  fputc('\n', stderr);
  fprintf(stderr, C_RESET);
}

static void msg_underline(int length)
{
  while (length--)
    fputc('^', stderr);

  fputc(' ', stderr);
}

static void msg(source_t source, const char *col, const char *format, va_list args)
{
  uint padding = 4;
  uint base = (ulong)(source.begin - source.start);

  fprintf(stderr, "%*d | ", padding, source.line);
  msg_line(source.start, col, source.begin, source.end);
  fprintf(stderr, "%*s | %*s", padding, "", base, "");
  msg_underline(source.end - source.begin);
  vfprintf(stderr, format, args);
  fputc('\n', stderr);
}

void fatal_at(source_t source, const char *format, ...)
{
  va_list va;

  fprintf(stderr, "liby " C_FATAL "fatal" C_RESET " %d:%d:\n", source.line, (int)(source.begin - source.start));
  va_start(va, format);
  msg(source, C_FATAL, format, va);
  va_end(va);

  exit(1);
}

static char *raw(unit_t *unit)
{
  char *ch = (unit->data + unit->cursor++);

  if (*ch == '/' && *(ch + 1) == '/')
  {
    *ch += 2;

    while (*ch && *ch != '\n')
      ch = (unit->data + unit->cursor++);
  }

  return ch;
}

static char *next(unit_t *unit)
{
  char *ch = raw(unit);

  switch (*ch)
  {
  case '\n':
    unit->line++;
    unit->start = ch + 1;
    // fallthrough
  case '\t':
  case  ' ':
    return next(unit);
  }

  return ch;
}

static source_t source(unit_t *unit, char *begin, char *end)
{
  return (source_t)
  {
    .line   = unit->line,
    .start  = unit->start,
    .begin  = begin,
    .end    = end
  };
}

static token_t token_new(unit_t *unit, token kind, char *begin, char *end)
{
  token_t token;

  token.kind = kind;
  token.source = source(unit, begin, end);

  if (kind == TOKEN_TEXT || kind == TOKEN_STRING)
  {
    token.value.kind = Y_STRING;
    token.value.string = (string_t) { .string = begin, .length = end - begin };
  }

  return token;
}

static token_t token_string(unit_t *unit, char *begin)
{
  char *ch;

  while (*(ch = raw(unit)) != '\"')
  {
    if (*ch == '\n')
      fatal_at(source(unit, ch, ch + 1), "Strings can not contain a new line.");
  }

  return token_new(unit, TOKEN_STRING, begin + 1, ch);
}

static token_t token_text(unit_t *unit, char *begin)
{
  char *ch = begin;

  while (isalpha(*ch) || isdigit(*ch) || *ch == '_')
    ch = raw(unit);

  return token_new(unit, TOKEN_TEXT, begin, (unit->data + --unit->cursor));
}

static token_t token_number(unit_t *unit, char *begin)
{
  token_t token;
  y_kind  number = Y_INTEGER;
  char *ch = begin;

  while (isdigit(*ch) || *ch == '_' || *ch == '.')
  {
    ch = raw(unit);

    if (*ch == '.')
    {
      if (number == Y_DECIMAL)
        fatal_at(source(unit, ch, ch + 1), "Duplicate floating-point decimal in number.");

      number = Y_DECIMAL;
    }
  }

  token = token_new(unit, TOKEN_NUMBER, begin, (unit->data + --unit->cursor));
  token.value.kind = number;

  if (number == Y_INTEGER)
    token.value.integer = strtoull(token.source.begin, &token.source.end, 10);
  else if (number == Y_DECIMAL)
    token.value.decimal = strtod(token.source.begin, &token.source.end);

  return token;
}


static token_t lex_token(unit_t *unit)
{
  if (unit->cursor > unit->length)
    return token_new(unit, TOKEN_NONE, 0, 0);

  char *ch = next(unit);

  switch (*ch)
  {
  case 0: // EOF
    return token_new(unit, TOKEN_NONE, ch, ch);
  case '{': case '}': case '@':
    return token_new(unit, *ch, ch, ch + 1);
  case '\"':
    return token_string(unit, ch);
  default:
  {
    if (isalpha(*ch) || *ch == '_')
      return token_text(unit, ch);

    if (isdigit(*ch))
      return token_number(unit, ch);
  }
  }

  fatal_at(source(unit, ch, ch + 1), "Unknown character: %c (%d)", *ch, *ch);
  return (token_t) { 0 };
}

static token_t lex_next(unit_t *unit)
{
  unit->previous = unit->current;

  return (unit->current = lex_token(unit));
}

token_t *token_eat(unit_t *unit)
{
  lex_next(unit);

  return &unit->previous;
}

token_t *token_expect(unit_t *unit, token kind)
{
  if (unit->current.kind == kind)
    return token_eat(unit);

  fatal_at(unit->current.source, "Expected token: %d, Recieved token: %d", kind, unit->current.kind);
  return NULL;
}

token_t *token_consume(unit_t *unit, token kind)
{
  if (unit->current.kind == kind)
    return token_eat(unit);

  return NULL;
}

static y_note_t *parse_note(unit_t *unit)
{
  y_note_t head = { 0 }, *it = &head;

  while (token_consume(unit, '@'))
  {
    y_note_t *note = it = it->next = allocate(NULL, sizeof(y_note_t));
    note->name = token_expect(unit, TOKEN_TEXT)->value.string;
  }

  return head.next;
}

static y_node_t *parse_node(unit_t *unit)
{
  y_node_t *node = allocate(NULL, sizeof(y_node_t));
  token_t *temp, *name = token_expect(unit, TOKEN_TEXT);

  node->name = name->value.string;

  if (token_consume(unit, '{'))
  {
    y_node_t head = { 0 }, *it = &head;

    while (!token_consume(unit, '}'))
    {
      if (temp = token_consume(unit, TOKEN_NONE))
        fatal_at(temp->source, "Expecting ending to node list.");
      
      it = it->next = parse_node(unit);
      it->parent = node;
    }

    node->value.kind = Y_NODE;
    node->value.node = head.next;
  }
  else if (temp = token_consume(unit, TOKEN_NUMBER)) 
  {
    node->value = temp->value;
  }
  else if (temp = token_consume(unit, TOKEN_STRING))
  {
    node->value = temp->value;
  }

  node->note = parse_note(unit);

  return node;
}

static y_node_t *parse_unit(unit_t *unit)
{
  y_node_t *node = parse_node(unit);
  token_expect(unit, TOKEN_NONE);

  return node;
}

yctx_t y_create(void)
{
  yctx_t ctx = { 0 };

  ctx.heads = &ctx.root;
  ctx.units = buf_create(0, sizeof(unit_t), NULL);
  
  return ctx;
}

void y_delete(yctx_t *y)
{
  buf_delete(y->units);
}

y_node_t *y_load(yctx_t *y, cstr path)
{
  unit_t unit = { 0 };
  fs_item_t *file = fs_open(path);

  assert(file->kind == FS_FILE);

  unit.length = fs_read(file, &unit.data, 0);
  unit.start = unit.data;
  fs_close(file);

  lex_next(&unit);
  y_node_t *head = parse_unit(&unit);
  y->heads = y->heads->next = head;

  buf_push(y->units, &unit);

  return head;
}

y_node_t *y_find(yctx_t *y, cstr path)
{
  unit_t unit = { .data = (char *) path, .length = strlen(path) };
  y_node_t *current = y->heads;
  token_t *temp;
  bool found = false;

  lex_next(&unit);
  while ((temp = token_consume(&unit, TOKEN_TEXT)))
  {
    // Looking for temp->value.string.string
    for (y_node_t *it = current; it; it = it->next)
    {
      // Testing temp->value.string.string vs it->name.string
      if (temp->value.string.length == it->name.length && !strncmp(temp->value.string.string, it->name.string, it->name.length))
      {
        if (unit.current.kind == TOKEN_NONE)
          return it;

        current = it->value.node;
        found = true;
        break;
      }
    }

    if (!found)
    {
      // Did not find a valid temp->value.string.string
      return NULL;
    }

    found = false;
  }
}

y_node_t *y_iter(y_node_t *begin, y_node_t **iter)
{
  if (*iter == NULL)
    *iter = begin;

  if ((*iter)->next == NULL)
    return NULL;
  
  return (*iter = (*iter)->next);
}

y_note_t *y_has(y_node_t *node, string_t note)
{
  for (y_note_t *it = node->note; it; it = it->next)
  {
    if (it->name.length == note.length && !strncmp(it->name.string, note.string, note.length))
      return it;
  }

  return NULL;
}