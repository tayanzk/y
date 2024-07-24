#ifndef _LIB_Y_h
#define _LIB_Y_h 1

#include <sdk/buf.h>
#include <sdk/map.h>
#include <sdk/alloc.h>
#include <sdk/view.h>

#define Y_VERSION "0.1"

typedef struct y_value_t y_value_t;
typedef struct y_note_t  y_note_t;
typedef struct y_node_t  y_node_t;
typedef struct yctx_t    yctx_t;

typedef enum y_kind
{
  Y_NONE = 0,

  Y_NODE,
  Y_STRING,
  Y_INTEGER,
  Y_DECIMAL
} y_kind;

struct y_value_t
{
  y_kind kind;

  union
  {
    y_node_t *node;
    y_value_t *list;
    string_t string;
    u64 integer;
    f64 decimal;
  };
};

struct y_note_t
{
  string_t name;
  y_note_t *next;
};

struct y_node_t
{
  string_t name;
  y_note_t *note;
  y_value_t value;
  y_node_t *parent;
  y_node_t *next;
};

struct yctx_t
{
  y_node_t root, *heads;
  buf_t   *units;
};

yctx_t y_create(void);
void   y_delete(yctx_t *y);

y_node_t *y_load(yctx_t *y, cstr path); // y_load "settings.y"
y_node_t *y_find(yctx_t *y, cstr path); // y_find "settings graphics vsync"
y_node_t *y_iter(y_node_t *begin, y_node_t **iterator);

y_note_t *y_has(y_node_t *node, string_t note);

#endif
