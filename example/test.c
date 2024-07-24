#include <liby/y.h>
#include <stdio.h>

void print_node(y_node_t *node)
{
  if (!node)
  {
    printf("null\n");
    return;
  }

  printf("%.*s: ", node->name.length, node->name.string);

  switch (node->value.kind)
  {
  case Y_NONE: printf("none"); break;
  case Y_NODE: printf("{..}"); break;
  case Y_STRING: printf("%.*s", node->value.string.length, node->value.string.string); break;
  case Y_INTEGER: printf("%d", node->value.integer); break;
  case Y_DECIMAL: printf("%.1f", node->value.decimal); break;
  }

  printf("\n");
}

int main(void)
{
  yctx_t ctx = y_create();

  y_node_t *settings = y_load(&ctx, "example/test.y");

  if (y_has(settings, string_view("mutable")))
    printf("settings are mutable\n");
  else 
    printf("settings are immutable\n");

  print_node(y_find(&ctx, "settings graphics"));
  print_node(y_find(&ctx, "settings graphics refresh"));
  print_node(y_find(&ctx, "settings graphics vsync"));
  print_node(y_find(&ctx, "settings difficulty"));

  y_delete(&ctx);
  return 0;
}