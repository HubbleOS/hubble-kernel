#include "ui_editor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <core/compositor/compositor.h>
#include <utils/color/color.h>

/*  overlay colours  */
#define COL_BORDER rgb(255, 220, 0)    /* всі елементи          */
#define COL_SELECTED rgb(50, 200, 255) /* вибраний елемент      */
#define COL_HANDLE rgb(255, 255, 255)  /* ручки resize          */
#define COL_HANDLE_BG rgb(50, 200, 255)

#define HANDLE_SZ 6

/*  draw helpers  */

static void draw_rect_outline(canvas_t *cnv, int x, int y, int w, int h,
                              uint32_t col) {
  canvas_draw_line(cnv, x, y, x + w - 1, y, col);
  canvas_draw_line(cnv, x, y + h - 1, x + w - 1, y + h - 1, col);
  canvas_draw_line(cnv, x, y, x, y + h - 1, col);
  canvas_draw_line(cnv, x + w - 1, y, x + w - 1, y + h - 1, col);
}

static void draw_handle(canvas_t *cnv, int cx, int cy) {
  int x = cx - HANDLE_SZ / 2;
  int y = cy - HANDLE_SZ / 2;
  for (int dy = 0; dy < HANDLE_SZ; dy++)
    canvas_draw_line(cnv, x, y + dy, x + HANDLE_SZ - 1, y + dy, COL_HANDLE_BG);
  draw_rect_outline(cnv, x, y, HANDLE_SZ, HANDLE_SZ, COL_HANDLE);
}

static const char *type_name(element_type_t t) {
  switch (t) {
  case UI_BUTTON:
    return "button";
  case UI_LABEL:
    return "label";
  case UI_TEXTBOX:
    return "textbox";
  case UI_RECT:
    return "rect";
  default:
    return "unknown";
  }
}

/*  init / toggle  */

void ui_editor_init(ui_editor_t *ed, window_t *win) {
  memset(ed, 0, sizeof(*ed));
  ed->win = win;
  // ed->active = true;
}

void ui_editor_toggle(ui_editor_t *ed) {
  ed->active = !ed->active;
  ed->selected = NULL;
  printf("[editor] %s\n", ed->active ? "ON" : "OFF");
}

/*  update (викликати з main loop)  */

void ui_editor_update(ui_editor_t *ed, local_mouse_t *mouse) {
  /* selection: новий клік лівої кнопки */
  if (mouse->left && !ed->prev_left) {
    if (mouse->hover_el) {
      ed->selected = mouse->hover_el;
      printf("[editor] selected: %s \"%s\" x=%d y=%d w=%d h=%d\n",
             type_name(ed->selected->type),
             ed->selected->text ? ed->selected->text : "", ed->selected->x,
             ed->selected->y, ed->selected->width, ed->selected->height);
    } else {
      ed->selected = NULL;
    }
  }

  /* drag елемента всередині object */
  if (mouse->drag_obj && mouse->drag_el) {
    int drag_x = mouse->x - mouse->drag_offset_x;
    int drag_y = mouse->y - mouse->drag_offset_y;
    object_move_element(mouse->drag_obj, mouse->drag_el,
                        drag_x - mouse->drag_obj->x,
                        drag_y - mouse->drag_obj->y);
  }
  /* drag самого вікна — лишаємо як завжди */
  else if (mouse->drag_obj && !mouse->drag_el) {
    int drag_x = mouse->x - mouse->drag_offset_x;
    int drag_y = mouse->y - mouse->drag_offset_y;
    compositor_move_object(mouse->drag_obj, drag_x, drag_y);
    compositor_bring_to_front(mouse->drag_obj, LAYER_WINDOWS);
  }

  /* друкуємо фінальну позицію після відпускання */
  if (!mouse->left && ed->prev_left && ed->selected) {
    printf("[editor] placed: %s x=%d y=%d w=%d h=%d\n",
           type_name(ed->selected->type), ed->selected->x, ed->selected->y,
           ed->selected->width, ed->selected->height);
  }

  ed->prev_left = mouse->left;
}

/*  overlay render  */

void ui_editor_render(ui_editor_t *ed, canvas_t *cnv) {
  if (!ed->active)
    return;

  object_t *obj = ed->win->surface;

  /*
   * Координати елементів відносні до object.
   * canvas_draw_line теж відносний до object (canvas живе всередині).
   * Тому просто використовуємо el->x, el->y напряму.
   */
  for (int i = 0; i < obj->element_count; i++) {
    element_t *el = obj->elements[i];
    bool sel = (el == ed->selected);

    draw_rect_outline(cnv, el->x, el->y, el->width, el->height,
                      sel ? COL_SELECTED : COL_BORDER);
  }

  /* ручки і хрест тільки для вибраного */
  if (ed->selected) {
    element_t *el = ed->selected;
    int r = el->x + el->width;
    int b = el->y + el->height;

    draw_handle(cnv, el->x, el->y); /* top-left     */
    draw_handle(cnv, r, el->y);     /* top-right    */
    draw_handle(cnv, el->x, b);     /* bottom-left  */
    draw_handle(cnv, r, b);         /* bottom-right */

    /* центральний хрест */
    int cx = el->x + el->width / 2;
    int cy = el->y + el->height / 2;
    canvas_draw_line(cnv, cx - 5, cy, cx + 5, cy, COL_SELECTED);
    canvas_draw_line(cnv, cx, cy - 5, cx, cy + 5, COL_SELECTED);
  }

  element_redraw(cnv);
}

/* ══════════════════════════════════════════════════════════════
 *  JSON  (без залежностей)
 * ══════════════════════════════════════════════════════════════ */

/*  save  */

void ui_editor_save(ui_editor_t *ed, const char *path) {
  FILE *f = fopen(path, "w");
  if (!f) {
    perror("ui_editor_save");
    return;
  }

  object_t *obj = ed->win->surface;

  fprintf(f, "{\n");
  fprintf(f, "  \"window\": { \"x\":%d, \"y\":%d, \"w\":%d, \"h\":%d },\n",
          obj->x, obj->y, obj->width, obj->height);
  fprintf(f, "  \"elements\": [\n");

  for (int i = 0; i < obj->element_count; i++) {
    element_t *el = obj->elements[i];
    bool last = (i == obj->element_count - 1);

    /* екранування тексту */
    char safe[256] = "";
    if (el->text) {
      int si = 0;
      for (int ti = 0; el->text[ti] && si < 253; ti++) {
        char c = el->text[ti];
        if (c == '"' || c == '\\')
          safe[si++] = '\\';
        safe[si++] = c;
      }
    }

    fprintf(f,
            "    { \"type\":\"%s\", \"x\":%d, \"y\":%d,"
            " \"w\":%d, \"h\":%d, \"text\":\"%s\" }%s\n",
            type_name(el->type), el->x, el->y, el->width, el->height, safe,
            last ? "" : ",");
  }

  fprintf(f, "  ]\n}\n");
  fclose(f);
  printf("[editor] saved %d elements -> %s\n", obj->element_count, path);
}

/*  мінімальний JSON parser  */

static const char *js_skip(const char *p) {
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
    p++;
  return p;
}

static const char *js_expect(const char *p, char c) {
  p = js_skip(p);
  return (*p == c) ? p + 1 : NULL;
}

static const char *js_string(const char *p, char *buf, int len) {
  p = js_skip(p);
  if (*p != '"')
    return NULL;
  p++;
  int i = 0;
  while (*p && *p != '"' && i < len - 1) {
    if (*p == '\\')
      p++;
    buf[i++] = *p++;
  }
  buf[i] = '\0';
  return (*p == '"') ? p + 1 : p;
}

static const char *js_int(const char *p, int *out) {
  p = js_skip(p);
  int sign = 1;
  if (*p == '-') {
    sign = -1;
    p++;
  }
  int v = 0;
  while (*p >= '0' && *p <= '9')
    v = v * 10 + (*p++ - '0');
  *out = sign * v;
  return p;
}

static element_type_t parse_type(const char *s) {
  if (strcmp(s, "button") == 0)
    return UI_BUTTON;
  if (strcmp(s, "label") == 0)
    return UI_LABEL;
  if (strcmp(s, "textbox") == 0)
    return UI_TEXTBOX;
  if (strcmp(s, "rect") == 0)
    return UI_RECT;
  return UI_LABEL;
}

/*  load  */

void ui_editor_load(ui_editor_t *ed, const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) {
    perror("ui_editor_load");
    return;
  }

  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  rewind(f);
  char *buf = malloc(sz + 1);
  if (!buf) {
    fclose(f);
    return;
  }
  fread(buf, 1, sz, f);
  buf[sz] = '\0';
  fclose(f);

  const char *p = strstr(buf, "\"elements\"");
  if (!p) {
    free(buf);
    return;
  }
  p = strchr(p, '[');
  if (!p) {
    free(buf);
    return;
  }
  p++;

  object_t *obj = ed->win->surface;
  int loaded = 0;

  while (1) {
    p = js_skip(p);
    if (*p == ']' || *p == '\0')
      break;

    p = js_expect(p, '{');
    if (!p)
      break;

    char type_str[32] = "";
    char text[256] = "";
    int x = 0, y = 0, w = 0, h = 0;

    for (int kv = 0; kv < 8; kv++) {
      p = js_skip(p);
      if (*p == '}')
        break;
      if (*p == ',') {
        p++;
        continue;
      }

      char key[32] = "";
      const char *after = js_string(p, key, sizeof(key));
      if (!after)
        break;
      after = js_expect(after, ':');
      if (!after)
        break;

      if (strcmp(key, "type") == 0)
        p = js_string(after, type_str, sizeof(type_str));
      else if (strcmp(key, "text") == 0)
        p = js_string(after, text, sizeof(text));
      else if (strcmp(key, "x") == 0)
        p = js_int(after, &x);
      else if (strcmp(key, "y") == 0)
        p = js_int(after, &y);
      else if (strcmp(key, "w") == 0)
        p = js_int(after, &w);
      else if (strcmp(key, "h") == 0)
        p = js_int(after, &h);
      else {
        /* пропустити невідоме значення */
        after = js_skip(after);
        if (*after == '"') {
          char tmp[256];
          p = js_string(after, tmp, sizeof(tmp));
        } else {
          int tmp;
          p = js_int(after, &tmp);
        }
      }
      if (!p)
        break;
    }

    p = js_skip(p);
    if (*p == '}')
      p++;
    p = js_skip(p);
    if (*p == ',')
      p++;

    /* знаходимо елемент у вікні за type + позицією */
    element_type_t et = parse_type(type_str);
    for (int i = 0; i < obj->element_count; i++) {
      element_t *el = obj->elements[i];
      if (el->type == et && el->x == x && el->y == y) {
        el->width = w;
        el->height = h;
        element_mark_dirty(el);
        loaded++;
        break;
      }
    }
  }

  free(buf);
  printf("[editor] loaded %d elements from %s\n", loaded, path);
}
