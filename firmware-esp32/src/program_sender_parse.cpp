#include "program_sender.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

bool program_sender_prepare_line(const char *raw, uint32_t line_no, char *out, size_t out_sz) {
  if (!raw || !out || out_sz < 4) return false;

  // Skip leading whitespace
  while (*raw == ' ' || *raw == '\t' || *raw == '\r') raw++;

  // Strip trailing whitespace / CR
  const char *end = raw + strlen(raw);
  while (end > raw && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
    end--;

  // Copy while removing `;` comments (not inside parentheses)
  char body[160];
  size_t n = 0;
  int paren = 0;
  for (const char *p = raw; p < end && n + 1 < sizeof(body); p++) {
    char c = *p;
    if (c == '(') paren++;
    if (c == ')' && paren > 0) paren--;
    if (c == ';' && paren == 0) break;
    body[n++] = c;
  }
  // Trim trailing space on body
  while (n > 0 && (body[n - 1] == ' ' || body[n - 1] == '\t')) n--;
  body[n] = 0;

  if (n == 0) return false;

  // $ / [ / % lines go untouched (no N prefix)
  bool bare = (body[0] == '$' || body[0] == '[' || body[0] == '%');

  if (line_no > 0 && !bare) {
    int wrote = snprintf(out, out_sz, "N%lu%s", (unsigned long)line_no, body);
    if (wrote < 0 || (size_t)wrote >= out_sz) return false;
  } else {
    if (n + 1 > out_sz) return false;
    memcpy(out, body, n + 1);
  }
  return true;
}
