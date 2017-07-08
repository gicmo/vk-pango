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
  PangoLayoutLine *line;
  PangoGlyphItem *gi;
  PangoFont *font;
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
                        NULL);

  cr = cairo_create(surface);

  cairo_scale(cr, scale, scale);
  cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
  cairo_paint(cr);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

  desc = pango_font_description_from_string("Sans 12");
  context = pango_cairo_create_context(cr);
  fontmap = pango_context_get_font_map(context);

  font = pango_font_map_load_font(fontmap, context, desc);

  layout = pango_layout_new(context);

  pango_layout_set_font_description(layout, desc);
  pango_layout_set_text(layout,
                        "AbCdEfGhIjKlMnOpQrStUvWxYz"
                        "aBcDeFgHiJkLmNoPqRsTuVwXyZ"
                        "ÄäÖöÜüß"
                        "ŁĄŻĘĆŃŚŹ"
                        //                        "ЯБГДЖЙ"
                        "¥£€$¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹"
                        "0123456789+*[]{}!\"#$%&'()~=",
                        -1);

  pango_layout_get_pixel_extents(layout, &pr, NULL);

  g_print("layout\n x: %d, y: %d, w: %d, h: %d\n",
          pr.x, pr.y, pr.width, pr.height);

  //pango_cairo_show_layout (cr, layout);

  line = pango_layout_get_line_readonly(layout, 0);
  g_assert_nonnull(line);
  g_assert_nonnull(line->runs);

  gi = (PangoGlyphItem *) line->runs->data;
  g_print(" glyphs: %d\n", gi->glyphs->num_glyphs);

  bins = g_array_sized_new(FALSE, FALSE, sizeof(GRect), gi->glyphs->num_glyphs);
  for (i = 0; i < gi->glyphs->num_glyphs; i++)
    {
      PangoGlyphInfo *glyph_info = gi->glyphs->glyphs + i;
      PangoGlyph glyph = glyph_info->glyph;
      PangoRectangle ink_rect;
      GRect gr = {0, };

      pango_font_get_glyph_extents(font, glyph, &ink_rect, NULL);
      pango_extents_to_pixels(&ink_rect, NULL);

      g_print("  %u: %d, %d, %d, %d\n",
              glyph,
              ink_rect.x, ink_rect.y,
              ink_rect.width, ink_rect.height);

      gr.height = ink_rect.height + 1;
      gr.width = ink_rect.width + 1;
      gr.id = GUINT_TO_POINTER(glyph);

      g_array_append_val(bins, gr);
    }

  packed = g_guillotine_packer_insert(packer, bins);
  g_print("Packing done [%d]\n", packed->len);

  g_object_get(packer,
               "free-rects", &rfree,
               NULL);


  scaled_font = pango_cairo_font_get_scaled_font(PANGO_CAIRO_FONT (font));
  cairo_set_scaled_font(cr, scaled_font);

  for (i = 0; i < rfree->len; i++)
    {
      GRect *gr = &g_array_index(rfree, GRect, i);

      g_print("  %u: %d, %d, %d, %d\n",
              i,
              gr->x, gr->y,
              gr->width, gr->height);

      cairo_set_line_width(cr, 1);
      cairo_set_source_rgba(cr, 0.0, 0.0, 1.0, 0.1);
      cairo_rectangle(cr, gr->x, gr->y, gr->width, gr->height);
      cairo_stroke_preserve(cr);
      cairo_fill(cr);
    }

  g_print("glyphs (packed): \n");

  for (i = 0; i < packed->len; i++)
    {
      GRect *gr = &g_array_index(packed, GRect, i);
      PangoGlyph glyph = GPOINTER_TO_UINT(gr->id);
      cairo_glyph_t cairo_glyph;
      PangoRectangle ink_rect;

      pango_font_get_glyph_extents(font, glyph, &ink_rect, NULL);
      pango_extents_to_pixels(&ink_rect, NULL);

      cairo_glyph.index = glyph;
      cairo_glyph.x = (int) gr->x - ink_rect.x;
      cairo_glyph.y = (int) gr->y - ink_rect.y;

      cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
      cairo_show_glyphs(cr, &cairo_glyph, 1);

      g_print("  %u: %d, %d, %d, %d @ %f, %f [%d, %d]\n",
              glyph,
              gr->x,
              gr->y,
              gr->width,
              gr->height,
              cairo_glyph.x,
              cairo_glyph.y,
              ink_rect.x,
              ink_rect.y);

      cairo_set_line_width(cr, 1);
      cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.1);
      cairo_rectangle(cr, gr->x, gr->y, gr->width, gr->height);
      cairo_stroke_preserve(cr);
      cairo_fill(cr);

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

  g_test_add("/bin-packer/font-basic",
	     Fixture, NULL,
	     fixture_set_up,
	     test_font_basic,
	     fixture_tear_down);

  return g_test_run();
}
