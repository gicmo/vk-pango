/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8; indent-tabs-mode: nil; -*- */

#include <glib.h>
#include <locale.h>

#include "gbinpacker.h"

#include <cairo.h>
#include <pango/pangocairo.h>

typedef struct {
  int dummy;
} Fixture;

static void
fixture_set_up (Fixture       *fixture,
		gconstpointer  user_data)
{

}

static void
fixture_tear_down (Fixture       *fixture,
		   gconstpointer  user_data)
{

}

static void
test_rect_basic (Fixture       *fixture,
		 gconstpointer  user_data)
{

  GRect a, b = {0, } ;
  GRect lt, rl, u = {0, };
  guint sum;

  a.x = a.y = 0;
  a.height = a.width = 100;

  g_assert_true(g_rect_area_nonzero(&a));
  g_assert_false(g_rect_area_nonzero(&b));

  g_assert_true(g_rect_can_fit(&a, &b));
  g_assert_cmpuint(g_rect_area(&a), ==, 100*100);

  b.height = a.height;
  b.width  = a.height;

  g_assert_true(g_rect_size_equal(&a, &b));
  g_assert_true(g_rect_can_fit(&a, &b));

  u.height = 20;
  u.width = 50;

  g_rect_guillotine(&a,
                    &u,
                    &lt,
                    &rl,
                    G_RECT_SPLIT_AREA_MAX);

  g_assert_true(g_rect_can_fit(&a, &lt));
  g_assert_true(g_rect_can_fit(&a, &rl));

  sum = g_rect_area(&u) +
        g_rect_area(&lt) +
        g_rect_area(&rl);

  g_assert_cmpuint(g_rect_area(&a), ==, sum);
}

static void
test_rect_merge (Fixture       *fixture,
		 gconstpointer  user_data)
{
  GRect a, b, u = {0, } ;

  a.x = a.y = 0;

  b.x = a.width + 10;
  b.y = a.height + 10;

  a.height = a.width = b.height = b.width = 10;

  /* same size, but not overlapping or sharing a border */
  g_assert_false(g_rect_merge(&a, &b, &u));

  /* move b on the x axis to the end of a,
     still not merge-able due to y offset */
  b.x = a.width;
  g_assert_false(g_rect_merge(&a, &b, &u));

  /* now they share a border and can be merged  */
  b.y = a.y;
  g_assert_true(g_rect_merge(&a, &b, &u));

  g_assert_cmpuint(u.x, ==, a.x);
  g_assert_cmpuint(u.y, ==, b.y);
  g_assert_cmpuint(u.height, ==, a.height);
  g_assert_cmpuint(u.width, ==, b.x + b.width - u.x);

  /* now lets make them overlap */
  b.x -= 3;
  g_assert_true(g_rect_merge(&a, &b, &u));

  g_assert_cmpuint(u.x, ==, a.x);
  g_assert_cmpuint(u.y, ==, b.y);
  g_assert_cmpuint(u.height, ==, a.height);
  g_assert_cmpuint(u.width, ==, b.x + b.width - u.x);

  a.x += 3;
  g_assert_true(g_rect_merge(&a, &b, &u));

  g_assert_cmpuint(u.x, ==, a.x);
  g_assert_cmpuint(u.y, ==, b.y);
  g_assert_cmpuint(u.height, ==, a.height);
  g_assert_cmpuint(u.width, ==, b.x + b.width - u.x);

  /* move a right of b */
  a.x = b.x + 5;
  g_assert_true(g_rect_merge(&a, &b, &u));

  g_assert_cmpuint(u.x, ==, b.x);  /* changed */
  g_assert_cmpuint(u.y, ==, b.y);
  g_assert_cmpuint(u.height, ==, a.height);  /* changed */
  g_assert_cmpuint(u.width, ==, a.x + a.width - u.x);  /* changed */

  /* the same spiel for the other axis */
  a.y = 10;
  a.x = 10;
  b.x = a.width + 10;
  b.y = a.height + 10;

  a.height = a.width = b.height = b.width = 15;
  g_assert_false(g_rect_merge(&a, &b, &u));

  b.y = a.height;
  g_assert_false(g_rect_merge(&a, &b, &u));

  b.x = a.x;
  g_assert_true(g_rect_merge(&a, &b, &u));

  g_assert_cmpuint(u.x, ==, a.x);
  g_assert_cmpuint(u.y, ==, a.y);
  g_assert_cmpuint(u.height, ==, b.y + b.height - u.y);
  g_assert_cmpuint(u.width, ==, b.width);

  /* now lets make them overlap */
  b.y -= 3;
  g_assert_true(g_rect_merge(&a, &b, &u));

  g_assert_cmpuint(u.x, ==, a.x);
  g_assert_cmpuint(u.y, ==, a.y);
  g_assert_cmpuint(u.height, ==, b.y + b.height - u.y);
  g_assert_cmpuint(u.width, ==, b.width);

  a.y -= 2;
  g_assert_true(g_rect_merge(&a, &b, &u));

  g_assert_cmpuint(u.x, ==, a.x);
  g_assert_cmpuint(u.y, ==, a.y);
  g_assert_cmpuint(u.height, ==, b.y + b.height - u.y);
  g_assert_cmpuint(u.width, ==, b.width);

  a.y = b.y + 5;
  g_assert_true(g_rect_merge(&a, &b, &u));

  g_assert_cmpuint(u.x, ==, a.x);
  g_assert_cmpuint(u.y, ==, b.y); /* changed */
  g_assert_cmpuint(u.height, ==, a.y + a.height - u.y); /* changed */
  g_assert_cmpuint(u.width, ==, b.width);

  /* let's do some known values,
     same y (50), needs same height (206),
     merge width of bordering rects
  */

  a.y = b.y = 50;
  a.height = b.height = 206;

  a.x = 135;
  a.width = 3;

  b.x = 138;
  b.width = 9;

  g_assert_true(g_rect_merge(&a, &b, &u));
  g_assert_cmpuint(u.x, ==, 135);
  g_assert_cmpuint(u.y, ==, 50);
  g_assert_cmpuint(u.height, ==, 206);
  g_assert_cmpuint(u.width, ==, 12); /* merged */

}

typedef struct GlyphInfo {
  PangoGlyph      glyph;
  PangoFont      *font;
  PangoRectangle  ink;
} GlyphInfo;

static void
test_font_basic (Fixture       *fixture,
		 gconstpointer  user_data)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_scaled_font_t *scaled_font;
  cairo_format_t format_cairo;
  PangoLayout *layout;
  PangoFontDescription *desc;
  cairo_status_t status;
  PangoRectangle pr = {0, };
  PangoContext *context;
  PangoFontMap *fontmap;
  GGuillotinePacker *packer;
  GArray *bins, *packed, *rfree;
  gint i;
  int scale = 2;

  format_cairo = CAIRO_FORMAT_ARGB32;
  surface = cairo_image_surface_create(format_cairo,
                                       256*scale,
                                       256*scale);

  packer = g_object_new(G_TYPE_GUILLOTINE_PACKER,
                        "width", 256,
                        "height", 256,
                        "merge-free", TRUE,
                        NULL);

  cr = cairo_create(surface);

  cairo_scale(cr, scale, scale);
  cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
  cairo_paint(cr);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

  desc = pango_font_description_from_string("Sans 12");
  context = pango_cairo_create_context(cr);
  fontmap = pango_context_get_font_map(context);

  //  font = pango_font_map_load_font(fontmap, context, desc);

  layout = pango_layout_new(context);

  pango_layout_set_font_description(layout, desc);
  pango_layout_set_text(layout,
                        "AbCdEfGhIjKlMnOpQrStUvWxYz"
                        "aBcDeFgHiJkLmNoPqRsTuVwXyZ"
                        "ÄäÖöÜüß"
                        "ŁĄŻĘĆŃŚŹ"
                        "ЯБГДЖЙ"
                        "ろぬふあうええおやゆよわほへたていすかんなにらぜ゜むちとしはきくまのりれけむつさそひこみもねるめ"
                        "¥£€$¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹"
                        "0123456789+*[]{}!\"#$%&'()~="
                        ,
                        -1);

  pango_layout_get_pixel_extents(layout, &pr, NULL);

  g_print("layout\n x: %d, y: %d, w: %d, h: %d\n",
          pr.x, pr.y, pr.width, pr.height);

  //pango_cairo_show_layout (cr, layout);

  bins = g_array_sized_new(FALSE, FALSE, sizeof(GRect), 10);

  PangoLayoutIter *li = pango_layout_get_iter(layout);

  do {
    const PangoLayoutRun *run = pango_layout_iter_get_run_readonly(li);
    const PangoGlyphItem *gi = (PangoGlyphItem *) run;

    if (!run)
      continue;

    for (i = 0; i < gi->glyphs->num_glyphs; i++)
      {
        PangoGlyphInfo *glyph_info = gi->glyphs->glyphs + i;
        PangoGlyph glyph = glyph_info->glyph;
        PangoFont *font = gi->item->analysis.font;
        GlyphInfo *info;
        GRect gr;


        info = g_slice_new(GlyphInfo);
        info->glyph = glyph;
        info->font = font;

        pango_font_get_glyph_extents(font, glyph, &info->ink, NULL);
        pango_extents_to_pixels(&info->ink, NULL);

        gr.height = info->ink.height + 1;
        gr.width = info->ink.width + 1;
        gr.id = info;

        g_print("  %u: %d, %d | %d, %d, %d, %d\n",
                glyph,
                gr.height, gr.width,
                info->ink.x, info->ink.y,
                info->ink.height, info->ink.width);

        g_array_append_val(bins, gr);
      }
  } while (pango_layout_iter_next_run(li));

  packed = g_guillotine_packer_insert(packer, bins);
  g_object_get(packer,
               "free-rects", &rfree,
               NULL);

  g_print("Packing done [%u bins packed, %u free rects]\n",
          packed->len, rfree->len);


  for (i = 0; i < rfree->len; i++)
    {
      GRect *gr = &g_array_index(rfree, GRect, i);

      g_print("  %u: %u, %u, %u, %d\n",
              i,
              gr->x, gr->y,
              gr->width, gr->height);

      cairo_set_line_width(cr, .5);
      cairo_set_source_rgba(cr, 0.0, 0.0, 1.0, 0.1);
      cairo_rectangle(cr, gr->x, gr->y, gr->width, gr->height);
      cairo_fill_preserve(cr);
      cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.1);
      cairo_stroke(cr);
    }


  g_print("glyphs (packed) [%u]: \n", packed->len);

  for (i = 0; i < packed->len; i++)
    {
      GRect *gr = &g_array_index(packed, GRect, i);
      GlyphInfo *info = gr->id;
      PangoGlyph glyph = info->glyph;
      cairo_glyph_t cairo_glyph;

      cairo_glyph.index = glyph;
      cairo_glyph.x = (int) gr->x - info->ink.x;
      cairo_glyph.y = (int) gr->y - info->ink.y;

      cairo_save(cr);
      cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
      scaled_font = pango_cairo_font_get_scaled_font(PANGO_CAIRO_FONT(info->font));
      cairo_set_scaled_font(cr, scaled_font);
      cairo_show_glyphs(cr, &cairo_glyph, 1);
      cairo_restore (cr);

      g_print("  %u: %d, %d, %d, %d @ %f, %f [%d, %d]\n",
              glyph,
              gr->x,
              gr->y,
              gr->width,
              gr->height,
              cairo_glyph.x,
              cairo_glyph.y,
              info->ink.x,
              info->ink.y);

      cairo_set_line_width(cr, 1);
      cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 0.1);
      cairo_rectangle(cr, gr->x, gr->y, gr->width, gr->height);
      cairo_stroke_preserve(cr);
      cairo_fill(cr);
    }

  GArray *bad = g_guillotine_packer_check(packer);

  if (bad)
    {
      g_print("consistency error: %d intersections\n", bad->len);
    }

  for (i = 0; bad && i < bad->len; i++)
    {
      GRect *r = &g_array_index(bad, GRect, i);

      cairo_set_line_width(cr, .5);
      cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.5);
      cairo_rectangle(cr, r->x, r->y, r->width, r->height);
      cairo_fill_preserve(cr);
      cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.1);
      cairo_stroke(cr);

    }

  cairo_destroy(cr);
  cairo_surface_flush(surface);
  status = cairo_surface_write_to_png(surface, "fb-1.png");

  cairo_surface_destroy(surface);
  pango_font_description_free(desc);

  g_assert_true(status == CAIRO_STATUS_SUCCESS);
}


int
main (int argc, char **argv)
{
  setlocale(LC_ALL, "");

  g_test_init(&argc, &argv, NULL);

  g_test_add("/bin-packer/rect",
	     Fixture, NULL,
	     fixture_set_up,
	     test_rect_basic,
	     fixture_tear_down);

  g_test_add("/bin-packer/rect/merge",
	     Fixture, NULL,
	     fixture_set_up,
	     test_rect_merge,
	     fixture_tear_down);


  g_test_add("/bin-packer/font-basic",
             Fixture, NULL,
             fixture_set_up,
             test_font_basic,
             fixture_tear_down);

  return g_test_run();
}
