/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8; indent-tabs-mode: nil; -*- */

#ifndef __G_BIN_PACKER_H__
#define __G_BIN_PACKER_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* ************************************************************************** */

typedef struct _GRect {
  guint x;
  guint y;

  guint height;
  guint width;

  gpointer id;
} GRect;

#define G_TYPE_RECT (g_rect_get_type())

GType            g_rect_get_type     (void);
GRect *          g_rect_copy         (const GRect *rect);
void             g_rect_free         (GRect       *rect);

gboolean         g_rect_size_equal   (const GRect *a,
				      const GRect *b);
gboolean         g_rect_equal        (const GRect *a,
				      const GRect *b);
gboolean         g_rect_area_nonzero (const GRect *rect);
guint            g_rect_area         (const GRect *rect);

gboolean         g_rect_intersect    (const GRect *a,
				      const GRect *b,
				      GRect       *intersection);

gboolean         g_rect_merge        (const GRect *a,
				      const GRect *b,
				      GRect       *intersection);

typedef enum _GRectSplit {

  G_RECT_SPLIT_AREA_MAX,
  G_RECT_SPLIT_AREA_MIN,

} GRectSplit;

void             g_rect_guillotine   (const GRect *origin,
				      const GRect *used,
				      GRect       *lt,
				      GRect       *rl,
				      GRectSplit   method);

typedef enum _GRectFit {
  G_RECT_FIT_AREA_BEST  = 0,
  G_RECT_FIT_AREA_WORST = 1,

  G_RECT_FIT_SHORT_SIDE_BEST  = 2,
  G_RECT_FIT_SHORT_SIDE_WORST = 3,

  G_RECT_FIT_LONG_SIDE_BEST  = 4,
  G_RECT_FIT_LONG_SIDE_WORST = 5

} GRectFit;


gboolean         g_rect_can_fit      (const GRect *outter,
				      const GRect *inner);

gint             g_rect_fit (const GRect *outter,
			     const GRect *inner,
			     GRectFit     method);

/* ************************************************************************** */
typedef struct _GBinPacker GBinPacker;

struct _GBinPackerClass
{
  GObjectClass parent_class;

  gpointer padding[13];
};

#define G_TYPE_BIN_PACKER g_bin_packer_get_type()
G_DECLARE_DERIVABLE_TYPE(GBinPacker, g_bin_packer, G, BIN_PACKER, GObject);


gfloat g_bin_packer_occupancy(GBinPacker *packer);


/* ************************************************************************** */

#define G_TYPE_GUILLOTINE_PACKER g_guillotine_packer_get_type()
G_DECLARE_FINAL_TYPE(GGuillotinePacker, g_guillotine_packer, G, GUILLOTINE_PACKER, GBinPacker);

GArray *  g_guillotine_packer_insert   (GGuillotinePacker *gp,
					GArray            *bins);
gboolean  g_guillotine_packer_pack     (GGuillotinePacker *gp,
					const GRect       *r);
GArray *  g_guillotine_packer_check    (GGuillotinePacker *gp);
/* ************************************************************************** */

G_END_DECLS

#endif /* __G_BIN_PACKER_H__ */
