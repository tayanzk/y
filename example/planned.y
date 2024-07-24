root
{
  numbers [ 1, 2, 3, 4 ] @listbox
  size    0.0            @slider(min -1.0 max 1.0)
  size    "Hello, World"
  // Above line errors because `size` is declared twice
}