/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8; indent-tabs-mode: nil; -*- */

#include <glib.h>
#include <string.h>

#include "gbinpacker.h"

/* ************************************************************************** */

GRect *
g_rect_copy (const GRect *rect)
{
  GRect *out = g_slice_new(GRect);
  memcpy(out, rect, sizeof(GRect));
  return out;
}

void
g_rect_free (GRect *rect)
{
  g_slice_free(GRect, rect);
}

gboolean
g_rect_size_equal (const GRect *a,
		   const GRect *b)
{
  return a->width == b->width && a->height == b->height;
}

static gboolean
g_rect_contains_point(const GRect *r,
                      guint x,
                      guint y)
{
  return FALSE;
}

gboolean
g_rect_intersect (const GRect *a,
                  const GRect *b,
                  GRect       *intersection)
{
  guint64 axe = a->x + a->width;
  guint64 aye = a->y + a->height;
  guint64 bxe = b->x + b->width;
  guint64 bye = b->y + b->height;

  return FALSE;
}

void
g_rect_guillotine (const GRect *origin,
		   const GRect *used,
		   GRect       *lt,
		   GRect       *rb,
		   GRectSplit   method)
{
  const guint w = origin->width  - used->width;
  const guint h = origin->height - used->height;
  gboolean horizontal;

  /*
     origin:
      ______________ _
     |      .       |
     |  lt  .   ?   |h
     |      .       |
     |------........|- <-- horizontal split
     | used |  rb   |
     |______________|
            |  w    |
            ^- vertical split

      The area "?" either belongs to "lt" or "rb"
      if we split horizontally or not.
   */

  switch (method)
    {
    case G_RECT_SPLIT_AREA_MAX:
      /* max(area(rb), area(lt)) */
      horizontal = used->width * h <= w * used->height;
      break;

    case G_RECT_SPLIT_AREA_MIN:
      horizontal = used->width * h > w * used->height;
      break;
    }

  lt->x      = origin->x;
  lt->y      = origin->y + used->height;
  lt->height = h; // origin - used
  //      width depends on split location

  rb->x     = origin->x + used->width;
  rb->y     = origin->y;
  rb->width = w; // origin - used
  //     height depends on split location

  if (horizontal)
    {
      lt->width  = origin->width;
      rb->height = used->height;
    }
  else
    {
      lt->width  = used->width;
      rb->height = origin->height;
    }
}

gboolean
g_rect_area_nonzero (const GRect *rect)
{
  return rect->height > 0 && rect->width > 0;
}

guint
g_rect_area (const GRect *rect)
{
  return rect->height * rect->width;
}

gboolean
g_rect_can_fit (const GRect *outter,
		const GRect *inner)
{
  return outter->height >= inner->height &&
         outter->width  >= inner->width;
}

gint
g_rect_fit (const GRect *o,
	    const GRect *i,
	    GRectFit     method)
{
  switch (method)
    {

    case G_RECT_FIT_AREA_BEST:
      return g_rect_area(o) - g_rect_area(i);

    case G_RECT_FIT_AREA_WORST:
      return -1 * (g_rect_area(o) - g_rect_area(i));
    }

  return G_MAXINT;
}


/* ************************************************************************** */

typedef struct _GBinPackerPrivate {
  guint width;
  guint height;

  GArray *rects;

} GBinPackerPrivate;

enum {
    PROP_BP_0,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_RECTS,
    PROP_BP_LAST
};

static GParamSpec *bp_props[PROP_BP_LAST] = { NULL, };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(GBinPacker,
                                    g_bin_packer,
                                    G_TYPE_OBJECT);

#define BP_GET_PRIV(obj) \
    ((GBinPackerPrivate *) g_bin_packer_get_instance_private(G_BIN_PACKER(obj)))

static void
g_bin_packer_finalize(GObject *object)
{
    GBinPacker *bp = G_BIN_PACKER(object);
    GBinPackerPrivate *priv = BP_GET_PRIV(bp);

    g_array_free(priv->rects, TRUE);
}

static void
g_bin_packer_get_property(GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
  GBinPacker *bp = G_BIN_PACKER(object);
  GBinPackerPrivate *priv = BP_GET_PRIV(bp);

  switch (prop_id) {

  case PROP_WIDTH:
    g_value_set_uint(value, priv->width);
    break;

  case PROP_HEIGHT:
    g_value_set_uint(value, priv->height);
    break;

  case PROP_RECTS:
    g_value_set_boxed(value, priv->rects);
    break;
  }

}

static void
g_bin_packer_set_property(GObject     *object,
			  guint        prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
    GBinPacker *bp = G_BIN_PACKER(object);
    GBinPackerPrivate *priv = BP_GET_PRIV(bp);

    switch (prop_id) {

    case PROP_WIDTH:
      priv->width = g_value_get_uint(value);
      break;

    case PROP_HEIGHT:
      priv->height = g_value_get_uint(value);
      break;
    }
}



static void
g_bin_packer_class_init(GBinPackerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize     = g_bin_packer_finalize;
    gobject_class->get_property = g_bin_packer_get_property;
    gobject_class->set_property = g_bin_packer_set_property;

    bp_props[PROP_WIDTH] =
      g_param_spec_uint("width",
			NULL, NULL,
			0, G_MAXUINT, 0,
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_READWRITE |
			G_PARAM_STATIC_NICK);

    bp_props[PROP_HEIGHT] =
      g_param_spec_uint("height",
			NULL, NULL,
			0, G_MAXUINT, 0,
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_READWRITE |
			G_PARAM_STATIC_NICK);

    bp_props[PROP_RECTS] =
      g_param_spec_boxed("rects",
			 NULL, NULL,
			 G_TYPE_ARRAY,
			 G_PARAM_READWRITE |
			 G_PARAM_STATIC_NICK);

    g_object_class_install_properties(gobject_class,
                                      PROP_BP_LAST,
                                      bp_props);
}

static void
g_bin_packer_init(GBinPacker *bp)
{
  GBinPackerPrivate *priv = BP_GET_PRIV(bp);

  priv->rects = g_array_new(FALSE, FALSE, sizeof(GRect));
}

gfloat g_bin_packer_occupancy(GBinPacker *packer)
{
  GBinPackerPrivate *priv = BP_GET_PRIV(packer);
  guint64 used = 0;
  gdouble total;
  gsize i;

  for (i = 0; i < priv->rects->len; i++)
    {
      const GRect *r = &g_array_index(priv->rects, GRect, i);
      used += g_rect_area(r);
    }

  total = priv->height * priv->width;
  return used / total;
}

/* ************************************************************************** */

struct _GGuillotinePacker {
  GBinPacker parent;

  GArray    *rects_free;

  GRectFit   fit_method;
  GRectSplit split_method;
};

enum {
  PROP_GP_0,
  PROP_GP_FREE_RECTS,
  PROP_GP_LAST
};
static GParamSpec *gp_props[PROP_GP_LAST] = { NULL, };

G_DEFINE_TYPE(GGuillotinePacker, g_guillotine_packer, G_TYPE_BIN_PACKER);

static void
g_guillotine_packer_finalize(GObject *obj)
{
  GGuillotinePacker *gp = G_GUILLOTINE_PACKER(obj);

  g_array_free(gp->rects_free, TRUE);

  G_OBJECT_CLASS(g_guillotine_packer_parent_class)->finalize(obj);
}

static void
g_guillotine_packer_get_property(GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GGuillotinePacker *gp = G_GUILLOTINE_PACKER(object);

  switch (prop_id) {
  case PROP_GP_FREE_RECTS:
    g_value_set_boxed(value, gp->rects_free);
    break;
  }
}

static void
g_guillotine_packer_constructed(GObject *obj)
{
  GGuillotinePacker *gp = G_GUILLOTINE_PACKER(obj);
  GBinPackerPrivate *priv = BP_GET_PRIV(gp);
  GRect *r;

  G_OBJECT_CLASS(g_guillotine_packer_parent_class)->constructed(obj);

  gp->rects_free->len = 1;

  r = &g_array_index(gp->rects_free, GRect, 0);
  r->x = r->y = 0;

  r->width  = priv->width;
  r->height = priv->height;
}

static void
g_guillotine_packer_init(GGuillotinePacker *gp)
{
  GBinPackerPrivate *priv = BP_GET_PRIV(gp);


  gp->rects_free = g_array_sized_new(FALSE, FALSE, sizeof(GRect), 1);


  /* TODO: make this a property  */
  gp->split_method = G_RECT_SPLIT_AREA_MAX;
  gp->fit_method = G_RECT_FIT_AREA_BEST;
}

static void
g_guillotine_packer_class_init(GGuillotinePackerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

  gobject_class->finalize     = g_guillotine_packer_finalize;
  gobject_class->get_property = g_guillotine_packer_get_property;
  gobject_class->constructed  = g_guillotine_packer_constructed;

  gp_props[PROP_GP_FREE_RECTS] =
    g_param_spec_boxed("free-rects",
                       NULL, NULL,
                       G_TYPE_ARRAY,
                       G_PARAM_READABLE |
                       G_PARAM_STATIC_NICK);

  g_object_class_install_properties(gobject_class,
                                    PROP_GP_LAST,
                                    gp_props);
}

gboolean
g_guillotine_packer_pack(GGuillotinePacker *gp,
                         const GRect       *r)
{
  GArray *out;
  GArray *in;
  gboolean res;

  in = g_array_sized_new(FALSE, FALSE, sizeof(GRect), 1);
  g_array_append_val(in, r);

  out = g_guillotine_packer_insert(gp, in);
  res = out->len == in->len;
  g_array_free(out, TRUE);

  return res;
}

GArray *
g_guillotine_packer_insert(GGuillotinePacker *gp,
			   GArray            *bins)
{
  GBinPackerPrivate *base = BP_GET_PRIV(gp);
  GArray *out;
  guint i, k;

  out = g_array_sized_new(FALSE, FALSE, sizeof(GRect), bins->len);

  while (bins->len > 0)
    {
      GRect inserted;
      int   score = G_MAXINT;     /* smaller is better */
      gsize pos;                  /* position of the free rect to use (i) */
      gsize idx;                  /* index of the best fitting rect (k) */

      GRect *f;
      GRect *b;

      GRect lt, rl;

      for (i = 0; i < gp->rects_free->len; i++)
	{
	  f = &g_array_index(gp->rects_free, GRect, i);

	  for (k = 0; k < bins->len; k++)
	    {
	      b = &g_array_index(bins, GRect, k);

	      if (g_rect_size_equal(b, f))
		{
		  pos = i;
		  idx = k;
		  score = G_MININT;

		  i = gp->rects_free->len - 1;
		}
	      else if (g_rect_can_fit(f, b))
		{
		  int my_score = g_rect_fit(f, b, gp->fit_method);
		  if (my_score < score)
		    {
		      pos = i;
		      idx = k;
		      score = my_score;
		    }
		}
	    }
        }

      if (score == G_MAXINT)
        {
          return out;
        }

      f = &g_array_index(gp->rects_free, GRect, pos);
      b = &g_array_index(bins, GRect, idx);

      inserted.x = f->x;
      inserted.y = f->y;
      inserted.height = b->height;
      inserted.width  = b->width;
      inserted.id = b->id;

      g_rect_guillotine(f, b, &lt, &rl, gp->split_method);
      g_array_remove_index_fast(gp->rects_free, pos);

      if (g_rect_area_nonzero(&lt))
        g_array_append_val(gp->rects_free, lt);

      if (g_rect_area_nonzero(&rl))
        g_array_append_val(gp->rects_free, rl);

      g_array_append_val(base->rects, inserted);
      g_array_append_val(out, inserted);
      g_array_remove_index(bins, idx);

    }

  return out;
}
