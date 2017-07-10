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

gboolean
g_rect_equal (const GRect *a,
              const GRect *b)
{
  return a->x == b->x && a->y && b-> y &&
         g_rect_size_equal(a, b);
}


static gboolean
g_rect_contains_point(const GRect *r,
                      guint x,
                      guint y)
{
  return FALSE;
}


/* check if the point p is contained in the
   segment that starts at s with length l */
#define SEGMENT_CONTAINS(s, l, p)          \
  (p >= s && p <= s + l)

gboolean
g_rect_intersect (const GRect *a,
                  const GRect *b,
                  GRect       *i)
{

  int right, left;
  int top, bottom;
  gboolean intersect;

  right  = MIN(a->x + a->width, b->x + b->width);
  left   = MAX(a->x, b->x);

  top    = MIN(a->y + a->height, b->y + b->height);
  bottom = MAX(a->y, b->y);

  intersect = top > bottom && right > left;

  if (!i)
    return intersect;

  if (intersect)
    {
      i->x = left;
      i->y = bottom;
      i->width = right - left;
      i->height = top - bottom;
    }
  else
    {
      memset(i, 0, sizeof(GRect));
    }

  return intersect;
}

gboolean
g_rect_merge (const GRect *a,
              const GRect *b,
              GRect       *u)
{
  if (a->height == b->height && a->y == b->y &&
      (SEGMENT_CONTAINS(a->x, a->width, b->x) ||
       SEGMENT_CONTAINS(b->x, b->width, a->x)))
    {

      u->y      = a->y;
      u->height = a->height;
      u->x      = MIN(a->x, b->x);
      u->width  = MAX(a->x + a->width, b->x + b->width) - u->x;

      return TRUE;
    }
  else if ((a->width == b->width && a->x == b->x) &&
           (SEGMENT_CONTAINS(a->y, a->height, b->y) ||
            SEGMENT_CONTAINS(b->y, b->height, a->y)))
    {

      u->x      = a->x;
      u->width  = a->width;
      u->y      = MIN(a->y, b->y);
      u->height = MAX(a->y + a->height, b->y + b->height) - u->y;

      return TRUE;
    }

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

#define G_RECT_FIT_IS_BEST(m) ((m & 1) == 0)
gint
g_rect_fit (const GRect *o,
	    const GRect *i,
	    GRectFit     method)
{
  gint score;
  switch (method)
    {

    case G_RECT_FIT_AREA_BEST:
      /* intentional fall-through */
    case G_RECT_FIT_AREA_WORST:
      /* assumes area(o) > area(i) */
      score = g_rect_area(o) - g_rect_area(i);
      break;

    case G_RECT_FIT_SHORT_SIDE_BEST:
      /* intentional fall-through */
    case G_RECT_FIT_LONG_SIDE_BEST:
      /* intentional fall-through */
    case G_RECT_FIT_LONG_SIDE_WORST:
      /* intentional fall-through */
    case G_RECT_FIT_SHORT_SIDE_WORST:
      {
        /* TODO: test me */
        const int h = ABS((int) o->width  - i->width);
	const int v = ABS((int) o->height - i->height);

        if (method == G_RECT_FIT_SHORT_SIDE_BEST ||
            method == G_RECT_FIT_SHORT_SIDE_WORST)
          score = MIN(h, v);
        else
          score = MAX(h, v);
      }

    }

  if (!G_RECT_FIT_IS_BEST(method))
    score *= -1;

  return score;
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
  gboolean   merge_free;

};

enum {
  PROP_GP_0,
  PROP_GP_FREE_RECTS,
  PROP_GP_MERGE_FREE,
  PROP_GP_FIT_METHOD,
  PROP_GP_SPLIT_METHOD,
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

  case PROP_GP_MERGE_FREE:
    g_value_set_boolean(value, gp->merge_free);
    break;

  case PROP_GP_FIT_METHOD:
    g_value_set_uint(value, gp->fit_method);
    break;

  case PROP_GP_SPLIT_METHOD:
    g_value_set_uint(value, gp->split_method);
    break;
  }
}


static void
g_guillotine_packer_set_property(GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GGuillotinePacker *gp = G_GUILLOTINE_PACKER(object);

  switch (prop_id) {
  case PROP_GP_MERGE_FREE:
     gp->merge_free = g_value_get_boolean(value);
    break;

  case PROP_GP_FIT_METHOD:
    gp->fit_method = g_value_get_uint(value);
    break;

  case PROP_GP_SPLIT_METHOD:
    gp->split_method = g_value_get_uint(value);
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
  gp->rects_free = g_array_sized_new(FALSE, FALSE, sizeof(GRect), 1);
}

static void
g_guillotine_packer_class_init(GGuillotinePackerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

  gobject_class->finalize     = g_guillotine_packer_finalize;
  gobject_class->get_property = g_guillotine_packer_get_property;
  gobject_class->set_property = g_guillotine_packer_set_property;
  gobject_class->constructed  = g_guillotine_packer_constructed;

  gp_props[PROP_GP_FREE_RECTS] =
    g_param_spec_boxed("free-rects",
                       NULL, NULL,
                       G_TYPE_ARRAY,
                       G_PARAM_READABLE |
                       G_PARAM_STATIC_NICK);

  gp_props[PROP_GP_MERGE_FREE] =
    g_param_spec_boolean("merge-free", NULL, NULL, TRUE,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_NICK);

  gp_props[PROP_GP_FIT_METHOD] =
    g_param_spec_uint("fit-method",
                      NULL, NULL,
                      G_RECT_FIT_AREA_BEST,
                      G_RECT_FIT_LONG_SIDE_WORST,
                      G_RECT_FIT_SHORT_SIDE_BEST,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_NICK);

    gp_props[PROP_GP_SPLIT_METHOD] =
    g_param_spec_uint("split-method",
                      NULL, NULL,
                      G_RECT_SPLIT_AREA_MAX,
                      G_RECT_SPLIT_AREA_MIN,
                      G_RECT_SPLIT_AREA_MAX,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_NICK);

  g_object_class_install_properties(gobject_class,
                                    PROP_GP_LAST,
                                    gp_props);
}

static guint
gp_merge_free_rects_pass(GGuillotinePacker *gp)
{
  guint i, k;
  guint merged = 0;

  for (i = 0; i < gp->rects_free->len; i++)
    {
      GRect *f = &g_array_index(gp->rects_free, GRect, i);

      for (k = i + 1; k < gp->rects_free->len; k++)
        {
          GRect *b = &g_array_index(gp->rects_free, GRect, k);
          GRect u;

          if (!g_rect_merge(f, b, &u))
            continue;

          *f = u;
          g_array_remove_index_fast(gp->rects_free, k);
          merged += 1;
          k--; /* we removed k, and replaced it, so check again */
        }
    }

  return merged;
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

	      if (g_rect_size_equal(f, b))
		{
		  pos = i;
		  idx = k;
		  score = G_MININT;

                  /* it cant get better, drop out of both loops */
                  i = gp->rects_free->len;
                  break;
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

      if (gp->merge_free)
        {
          guint merged = gp_merge_free_rects_pass(gp);
          g_debug("GP: merged %u free rects", merged);
        }

      g_array_append_val(base->rects, inserted);
      g_array_append_val(out, inserted);
      g_array_remove_index(bins, idx);

    }

  return out;
}

GArray *
g_guillotine_packer_check(GGuillotinePacker *gp)
{
  GBinPackerPrivate *base = BP_GET_PRIV(gp);
  const guint n_free = gp->rects_free->len;
  const guint n_used = base->rects->len;
  const guint n = n_free + n_used;
  GArray *bad = NULL;
  GArray *all;
  guint  i;

  all = g_array_sized_new(FALSE, FALSE, sizeof(GRect), n);

  g_array_append_vals(all, gp->rects_free->data, n_free);
  g_array_append_vals(all, base->rects->data, n_used);

  for (i = 0; i < n; i++)
    {
      const GRect *a = &g_array_index(all, GRect, i);
      guint k;

      for (k = i + 1; k < n; k++)
        {
          const GRect *b = &g_array_index(all, GRect, k);
          GRect o = {0, };

          if (!g_rect_intersect(a, b, &o))
            continue;

          if (bad == NULL)
            bad = g_array_sized_new(FALSE, FALSE, sizeof(GRect), 1);

          g_array_append_val(bad, o);
          /* we keep going to find all errors */
        }
    }

  return bad;
}

/* ************************************************************************** */

struct _GSkylinePacker {
  GBinPacker         parent;

  GArray            *skyline;

  gboolean           use_wm;
  GGuillotinePacker *wastemap;
};

enum {
  PROP_SP_0,
  PROP_SP_SKYLINE,
  PROP_SP_USE_WASTEMAP,
  PROP_SP_LAST
};
static GParamSpec *sp_props[PROP_SP_LAST] = { NULL, };

G_DEFINE_TYPE(GSkylinePacker, g_skyline_packer, G_TYPE_BIN_PACKER);

static void
g_skyline_packer_finalize(GObject *obj)
{
  GSkylinePacker *sp = G_SKYLINE_PACKER(obj);

  g_array_free(sp->skyline, TRUE);
  g_clear_pointer(&sp->wastemap, g_object_unref);

  G_OBJECT_CLASS(g_skyline_packer_parent_class)->finalize(obj);
}

static void
g_skyline_packer_get_property(GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GSkylinePacker *sp = G_SKYLINE_PACKER(object);

  switch (prop_id) {
  case PROP_SP_SKYLINE:
    g_value_set_boxed(value, sp->skyline);
    break;

  case PROP_SP_USE_WASTEMAP:
    g_value_set_boolean(value, sp->wastemap != NULL);
    break;
  }
}


static void
g_skyline_packer_set_property(GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GSkylinePacker *sp = G_SKYLINE_PACKER(object);

  switch (prop_id) {
  case PROP_SP_USE_WASTEMAP:
     sp->use_wm = g_value_get_boolean(value);
    break;
  }

}


static void
g_skyline_packer_constructed(GObject *obj)
{
  GSkylinePacker *sp = G_SKYLINE_PACKER(obj);
  GBinPackerPrivate *priv = BP_GET_PRIV(sp);
  GRect *r;

  G_OBJECT_CLASS(g_skyline_packer_parent_class)->constructed(obj);

  sp->skyline->len = 1;

  r = &g_array_index(sp->skyline, GRect, 0);
  r->x = r->y = 0;

  r->width  = priv->width;
  r->height = 0; //not actually needed
}

static void
g_skyline_packer_init(GSkylinePacker *sp)
{
  sp->skyline = g_array_sized_new(FALSE, FALSE, sizeof(GRect), 1);
}

static void
g_skyline_packer_class_init(GSkylinePackerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

  gobject_class->finalize     = g_skyline_packer_finalize;
  gobject_class->get_property = g_skyline_packer_get_property;
  gobject_class->set_property = g_skyline_packer_set_property;
  gobject_class->constructed  = g_skyline_packer_constructed;

  sp_props[PROP_SP_SKYLINE] =
    g_param_spec_boxed("skyline",
                       NULL, NULL,
                       G_TYPE_ARRAY,
                       G_PARAM_READABLE |
                       G_PARAM_STATIC_NICK);

  sp_props[PROP_SP_USE_WASTEMAP] =
    g_param_spec_boolean("use-wastemap",
                         NULL, NULL, TRUE,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_NICK);

  g_object_class_install_properties(gobject_class,
                                    PROP_SP_LAST,
                                    sp_props);
}

typedef enum GSkylinePackerLevel {
  GSP_LEVEL_BOTTOM_LEFT,
  GSP_LEVEL_MIN_WASTE,
} GSkylinePackerLevel;

static gboolean
skyline_fit_rect_at(GSkylinePacker *sp,
                    const GRect     *r,
                    guint           index,
                    guint           *y_out)
{
  GBinPackerPrivate *base = BP_GET_PRIV(sp);
  GRect *n = &g_array_index(sp->skyline, GRect, index);
  int space;
  guint y;

  if (n->x + r->width > base->width)
    return FALSE;

  space = r->width;
  y = n->y;

  while (space > 0)
    {
      y = MAX(y, n->y);

      if (y + r->height > base->height)
        return FALSE;

      space -= n->width;
      index++;

      g_assert(index < sp->skyline->len || space <= 0);
      n = &g_array_index(sp->skyline, GRect, index);
    }

  *y_out = y;
  return TRUE;
}

#if 0
static int
skyline_compute_waste(GSkylinePacker *sp,
                      guint   index,
                      GRect  *r)
{
  int waste = 0;
  GRect *n = &g_array_index(sp->skyline, GRect, index);
  const int left = n->x;
  const int right = left + r->width;

  while (index < sp->skyline->len && n->x < right)
    {
      const int ls = n->x;
      const int rs = MIN(right, ls + n->width);

      if (ls >= right || n->x + n->width <= left)
        break;


      waste += (rs - ls) * (r->y - n->y);

      index++;
      n = &g_array_index(sp->skyline, GRect, index);
    }

  return waste;
}


static gboolean
skyline_fit_rect_at_waste(GSkylinePacker *sp,
                          guint   index,
                          GRect  *r,
                          int    *waste)
{
  if (! skyline_fit_rectangle(sp, index, r))
    return FALSE;

  *waste = skyline_compute_waste(sp, index, r);
  return TRUE;
}

#endif

typedef struct Score {

  guint first;
  guint second;

} Score;

static gboolean
score_check_and_update(Score *score,
                       guint  first,
                       guint  second)
{
  if (!(first < score->first ||
        (first == score->first && second < score->second)))
    return FALSE;

  score->first = first;
  score->second = second;

  return TRUE;
}

static gboolean
position_node_bl(GSkylinePacker *sp,
                 GRect          *r,
                 guint          *index,
                 Score          *score)
{
  guint i;
  gboolean have_fit = FALSE;

  score->first = G_MAXUINT;
  score->second = G_MAXUINT;

  for (i = 0; i < sp->skyline->len; i++)
    {
      GRect *n = &g_array_index(sp->skyline, GRect, i);
      guint top;
      guint y;

      if (! skyline_fit_rect_at(sp, r, i, &y))
        continue;

      top = y + r->height;

      if (!score_check_and_update(score, top, n->width))
        continue;

      r->x = n->x;
      r->y = y;

      *index = i;
      have_fit = TRUE;
    }

  return have_fit;
}

static void
skyline_add_level(GSkylinePacker *sp,
                  const GRect    *r,
                  guint           pos)
{
  GBinPackerPrivate *base = BP_GET_PRIV(sp);
  GRect a;
  guint i;

  a.x = r->x;
  a.y = r->y + r->height;
  a.width = r->width;

  g_array_insert_val(sp->skyline, pos, a);

  g_assert(a.x + a.width <= base->width);
  g_assert(a.y <= base->height);

  for (i = pos + 1; i < sp->skyline->len; i++)
    {
      GRect *n = &g_array_index(sp->skyline, GRect, i);
      GRect *b = &g_array_index(sp->skyline, GRect, i - 1);
      int width = n->width;
      int shrink;

      g_assert(b->x <= n->x);

      if (!(n->x < b->x + b->width))
        break;

      shrink = ((int) b->x + b->width) - n->x;

      n->x += shrink;
      width -= shrink;

      if (width > 0)
        {
          n->width = width;
          break;
        }

      g_array_remove_index(sp->skyline, i);
      i--;
    }

  /*  */
  if (sp->skyline->len == 0)
    return; /* avoid overflow in case of len == 0 below */

  for (i = 0; i < sp->skyline->len - 1; i++)
    {
      GRect *n = &g_array_index(sp->skyline, GRect, i);
      GRect *b = &g_array_index(sp->skyline, GRect, i + 1);

      if (n->y != b->y)
        continue;

      n->width += b->width;
      g_array_remove_index(sp->skyline, i + 1);
      i--;
    }
}

GArray *
g_skyline_packer_insert(GSkylinePacker *sp,
                        GArray         *bins)
{
  GBinPackerPrivate *base = BP_GET_PRIV(sp);
  GArray *out = g_array_sized_new(FALSE, FALSE, sizeof(GRect), bins->len);

  while (bins->len > 0)
    {
      gboolean have_fit = FALSE;
      Score score = {G_MAXUINT, G_MAXUINT};
      GRect best;
      guint best_skyline;
      guint best_bin;
      guint i;

      for (i = 0; i < bins->len; i++)
        {
          GRect t = g_array_index(bins, GRect, i);
          Score s;
          guint idx;

          if (!position_node_bl(sp, &t, &idx, &s) ||
              !score_check_and_update(&score, s.first, s.second))
              continue;

          best = t;
          best_skyline = idx;
          best_bin = i;
          have_fit = TRUE;
        }

      if (!have_fit)
        break;

      skyline_add_level(sp, &best, best_skyline);

      g_array_append_val(base->rects, best);
      g_array_append_val(out, best);
      g_array_remove_index_fast(bins, best_bin);
    }

  return out;
}
