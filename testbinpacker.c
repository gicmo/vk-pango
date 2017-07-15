/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8; indent-tabs-mode: nil; -*- */

#include <glib.h>
#include <locale.h>

#include "gbinpacker.h"

#include <cairo.h>
#include <pango/pangocairo.h>

typedef struct Fixture {
  int dummy;
} Fixture;

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

  a.height = a.width = b.height = b.width = 10;

  b.x = a.width + 10;
  b.y = a.height + 10;

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


static const char sample_text[] =
  "AbCdEfGhIjKlMnOpQrStUvWxYz"
  "aBcDeFgHiJkLmNoPqRsTuVwXyZ"
  "ÄäÖöÜüß"
  "θαζχσωεδξωφρτγβνηψυςμκιολπάέήίϊΐόύϋΰώ"
  "ŁĄŻĘĆŃŚŹ"
  "ЯБГДЖЙ"
  "ろぬふあうええおやゆよわほへたていすかんなにらぜ゜むちとしはきくまのりれけむつさそひこみもねるめ"
  "غ ظ ض ذ خ ث ت ش ر ق ص ف ع س ن م ل ك ي ط ح ز و ه د ج ب ا شين"
  "¥£€$¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹"
  "0123456789+*[]{}!\"#$%&'()~=";

typedef struct GlyphInfo {
  PangoGlyph      glyph;
  PangoFont      *font;
  PangoRectangle  ink;
} GlyphInfo;

typedef struct {
  cairo_t *cr;
  cairo_surface_t *surface;
  PangoLayout *layout;
  PangoContext *context;
  PangoFontMap *fontmap;
  GArray *bins;

  guint width;
  guint height;
} PackerFixture;

static void
fixture_set_up (PackerFixture *fixture,
		gconstpointer  user_data)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_format_t format_cairo;
  PangoLayout *layout;
  PangoFontDescription *desc;
  PangoRectangle pr = {0, };
  PangoContext *context;
  PangoFontMap *fontmap;
  GArray *bins;
  gint i;
  int scale = 2;

  fixture->height = 256;
  fixture->width = 256;

  format_cairo = CAIRO_FORMAT_ARGB32;
  surface = cairo_image_surface_create(format_cairo,
                                       fixture->width * scale,
                                       fixture->height * scale);

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
  pango_font_description_free(desc);

  pango_layout_set_text(layout,
                        sample_text,
                        -1);

  pango_layout_get_pixel_extents(layout, &pr, NULL);

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

        g_array_append_val(bins, gr);
      }

  } while (pango_layout_iter_next_run(li));

  fixture->cr = cr;
  fixture->surface = surface;
  fixture->layout = layout;
  fixture->context = context;
  fixture->fontmap = fontmap;
  fixture->bins = bins;
}

  static void
fixture_tear_down (PackerFixture *fixture,
		   gconstpointer  user_data)
{
  cairo_destroy(fixture->cr);
  cairo_surface_destroy(fixture->surface);
  g_object_unref(fixture->layout);
  g_object_unref(fixture->context);
  g_object_unref(fixture->fontmap);
}

static void
draw_packed_bins(cairo_t *cr,
                 GArray *packed)
{
  guint i;

  if (packed == NULL || packed->len < 1)
    return;

  for (i = 0; i < packed->len; i++)
    {
      GRect *gr = &g_array_index(packed, GRect, i);
      GlyphInfo *info = gr->id;
      PangoGlyph glyph = info->glyph;
      cairo_scaled_font_t *scaled_font;
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

      cairo_set_line_width(cr, 1);
      cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 0.1);
      cairo_rectangle(cr, gr->x, gr->y, gr->width, gr->height);
      cairo_stroke_preserve(cr);
      cairo_fill(cr);
    }
}

static void
draw_rects(cairo_t *cr,
           GArray  *rects,
           double   r,
           double   g,
           double   b)
{
  guint i;

  if (rects == NULL || rects->len < 1)
    return;


  for (i = 0; i < rects->len; i++)
    {
      GRect *gr = &g_array_index(rects, GRect, i);

      cairo_set_line_width(cr, .5);
      cairo_set_source_rgba(cr, r, g, b, 0.1);
      cairo_rectangle(cr, gr->x, gr->y, gr->width, gr->height);
      cairo_fill_preserve(cr);
      cairo_set_source_rgba(cr, r, g, b, 0.1);
      cairo_stroke(cr);
    }
}

static void
test_guillotine_packer (PackerFixture *fixture,
                        gconstpointer  user_data)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  GGuillotinePacker *packer;
  cairo_status_t status;
  GArray *packed, *rfree, *bad;

  surface = fixture->surface;
  cr = fixture->cr;

  packer = g_object_new(G_TYPE_GUILLOTINE_PACKER,
                        "width", fixture->width,
                        "height", fixture->height,
                        "merge-free", TRUE,
                        NULL);

  packed = g_guillotine_packer_insert(packer, fixture->bins);

  g_object_get(packer,
               "free-rects", &rfree,
               NULL);

  g_debug("Packing done [%u bins packed, %u free]\n",
          packed->len, rfree->len);

  draw_rects(cr, rfree, 0.0, 0.0, 1.0);
  draw_packed_bins(cr, packed);

  /* consistency check */
  bad = g_guillotine_packer_check(packer);
  draw_rects(cr, bad, 1.0, 0.0, 0.0);

  cairo_surface_flush(surface);
  status = cairo_surface_write_to_png(surface, "guillotine.png");

  if (bad)
    g_warning("consistency error: %d intersections\n", bad->len);

  g_object_unref(packer);
  g_assert_true(status == CAIRO_STATUS_SUCCESS);
}

static void
draw_skyline(cairo_t *cr,
             GArray  *skyline,
             double   r,
             double   g,
             double   b)
{

  guint i;
  GRect *n;

  if (skyline == NULL || skyline->len < 1)
    return;

  n = &g_array_index(skyline, GRect, 0);

  cairo_move_to(cr, 0, n->y);
  cairo_set_line_width(cr, 1.0);
  cairo_set_source_rgba(cr, r, g, b, 1.);

  for (i = 0; i < skyline->len; i++)
    {
      GRect *n = &g_array_index(skyline, GRect, i);

      cairo_line_to(cr, n->x,            n->y);
      cairo_line_to(cr, n->x + n->width, n->y);
    }

  cairo_stroke(cr);
}

static void
test_skyline_packer (PackerFixture *fixture,
                     gconstpointer  user_data)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  GSkylinePacker *packer;
  cairo_status_t status;
  GArray *packed, *skyline;

  surface = fixture->surface;
  cr = fixture->cr;

  packer = g_object_new(G_TYPE_SKYLINE_PACKER,
                        "width", fixture->width,
                        "height", fixture->height,
                        NULL);

  packed = g_skyline_packer_insert(packer, fixture->bins);
  g_debug("Packing done [%u bins packed]\n",
          packed->len);

  draw_packed_bins(cr, packed);

  g_object_get(packer, "skyline", &skyline, NULL);
  draw_skyline(cr, skyline, 0.0, 0.0, 1.0);

  cairo_surface_flush(surface);
  status = cairo_surface_write_to_png(surface, "skyline.png");

  g_object_unref(packer);
  g_assert_true(status == CAIRO_STATUS_SUCCESS);
}


int
main (int argc, char **argv)
{
  setlocale(LC_ALL, "");

  g_test_init(&argc, &argv, NULL);

  g_test_add("/bin-packer/rect",
	     Fixture, NULL,
	     NULL,
	     test_rect_basic,
	     NULL);

  g_test_add("/bin-packer/rect/merge",
	     Fixture, NULL,
	     NULL,
	     test_rect_merge,
	     NULL);

  /* the packers */
  g_test_add("/bin-packer/packer/guillotine",
             PackerFixture, NULL,
             fixture_set_up,
             test_guillotine_packer,
             fixture_tear_down);

  g_test_add("/bin-packer/packer/skyline",
             PackerFixture, NULL,
             fixture_set_up,
             test_skyline_packer,
             fixture_tear_down);

  return g_test_run();
}
