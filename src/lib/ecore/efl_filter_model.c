#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <Efl_Core.h>

#include "efl_composite_model_private.h"

typedef struct _Efl_Filter_Model_Mapping Efl_Filter_Model_Mapping;
struct _Efl_Filter_Model_Mapping
{
   EINA_RBTREE;

   uint64_t original;
   uint64_t mapped;

   EINA_REFCOUNT;
};

typedef struct _Efl_Filter_Model_Data Efl_Filter_Model_Data;
struct _Efl_Filter_Model_Data
{
   Efl_Filter_Model_Mapping *self;

   Eina_Rbtree *mapping;

   struct {
      void *data;
      EflFilterModel cb;
      Eina_Free_Cb free_cb;
      uint64_t count;
   } filter;

   uint64_t counted;
   Eina_Bool counting_started : 1;
};

// FIXME: Idea map index to composite index
// First count request start with zero and background walk every file
// filter them and trigger and event.
// For each remote event update index
// For each add event, call the filter on it, reemit if make sense.
// Intercept property "child.index"

static Eina_Rbtree_Direction
_filter_mapping_cmp_cb(const Eina_Rbtree *left, const Eina_Rbtree *right, void *data EINA_UNUSED)
{
   const Efl_Filter_Model_Mapping *l, *r;

   l = (const Efl_Filter_Model_Mapping *) left;
   r = (const Efl_Filter_Model_Mapping *) right;

   if (l->mapped < r->mapped)
     return EINA_RBTREE_LEFT;
   return EINA_RBTREE_RIGHT;
}

static int
_filter_mapping_looking_cb(const Eina_Rbtree *node, const void *key,
                           int length EINA_UNUSED, void *data EINA_UNUSED)
{
   const Efl_Filter_Model_Mapping *n = (const Efl_Filter_Model_Mapping *) node;
   const uint64_t *k = key;

   return n->mapped - *k;
}

static void
_efl_filter_model_filter_set(Eo *obj EINA_UNUSED, Efl_Filter_Model_Data *pd,
                             void *filter_data, EflFilterModel filter, Eina_Free_Cb filter_free_cb)
{
   if (pd->filter.cb)
     pd->filter.free_cb(pd->filter.data);
   pd->filter.data = filter_data;
   pd->filter.cb = filter;
   pd->filter.free_cb = filter_free_cb;
}

static void
_rbtree_free_cb(Eina_Rbtree *node, void *data EINA_UNUSED)
{
   Efl_Filter_Model_Mapping *m = (Efl_Filter_Model_Mapping*) node;

   EINA_REFCOUNT_UNREF(m)
     free(m);
}

static void
_efl_filter_model_child_added(void *data, const Efl_Event *event)
{
   Efl_Filter_Model_Data *pd = data;
}

static void
_efl_filter_model_child_removed(void *data, const Efl_Event *event)
{
   Efl_Filter_Model_Mapping *mapping;
   Efl_Filter_Model_Data *pd = data;
   Efl_Model_Children_Event *ev = event->info;
   uint64_t removed = ev->index;

   mapping = (void *)eina_rbtree_inline_lookup(pd->mapping,
                                               &removed, sizeof (uint64_t),
                                               _filter_mapping_looking_cb, NULL);
   if (!mapping) return;

   pd->mapping = eina_rbtree_inline_remove(pd->mapping, EINA_RBTREE_GET(mapping),
                                           _filter_mapping_cmp_cb, NULL);

   EINA_REFCOUNT_UNREF(mapping)
     free(mapping);

   // Update the tree for the index to reflect the removed child
   for (removed++; removed < pd->filter.count; removed++)
     {
        mapping = (void *)eina_rbtree_inline_lookup(pd->mapping,
                                                    &removed, sizeof (uint64_t),
                                                    _filter_mapping_looking_cb, NULL);
        if (!mapping) continue;

        pd->mapping = eina_rbtree_inline_remove(pd->mapping, EINA_RBTREE_GET(mapping),
                                                _filter_mapping_cmp_cb, NULL);
        mapping->mapped++;
        pd->mapping = eina_rbtree_inline_insert(pd->mapping, EINA_RBTREE_GET(mapping),
                                                _filter_mapping_cmp_cb, NULL);
     }
}

EFL_CALLBACKS_ARRAY_DEFINE(filters_callbacks,
                           { EFL_MODEL_EVENT_CHILD_ADDED, _efl_filter_model_child_added },
                           { EFL_MODEL_EVENT_CHILD_REMOVED, _efl_filter_model_child_removed });

static Efl_Object *
_efl_filter_model_efl_object_constructor(Eo *obj, Efl_Filter_Model_Data *pd)
{
   efl_event_callback_array_priority_add(obj, filters_callbacks(), EFL_CALLBACK_PRIORITY_BEFORE, pd);

   return efl_constructor(efl_super(obj, EFL_FILTER_MODEL_CLASS));
}

static void
_efl_filter_model_efl_object_destructor(Eo *obj, Efl_Filter_Model_Data *pd)
{
   eina_rbtree_delete(pd->mapping, _rbtree_free_cb, NULL);
   EINA_REFCOUNT_UNREF(pd->self)
     free(pd->self);
   pd->self = NULL;

   efl_destructor(efl_super(obj, EFL_FILTER_MODEL_CLASS));
}

static Eina_Value
_filter_remove_array(Eo *o EINA_UNUSED, void *data, const Eina_Value v)
{
   Efl_Filter_Model_Mapping *mapping = data;
   unsigned int i, len;
   Eo *target = NULL;

   EINA_VALUE_ARRAY_FOREACH(&v, len, i, target)
     break;

   if (efl_isa(target, EFL_FILTER_MODEL_CLASS))
     {
        Efl_Filter_Model_Data *pd = efl_data_scope_get(target, EFL_FILTER_MODEL_CLASS);

        pd->self = mapping;
        EINA_REFCOUNT_REF(pd->self);
     }

   return eina_value_object_init(target);
}

static Eina_Future *
_efl_filter_model_efl_model_children_slice_get(Eo *obj, Efl_Filter_Model_Data *pd,
                                               unsigned int start, unsigned int count)
{
   Efl_Filter_Model_Mapping **mapping = NULL;
   Eina_Future **r = NULL;
   Eina_Future *f;
   unsigned int i;
   Eina_Error err = ENOMEM;

   if ((uint64_t) start + (uint64_t) count > pd->filter.count)
     return efl_loop_future_rejected(obj, EFL_MODEL_ERROR_INCORRECT_VALUE);
   if (count == 0)
     return efl_loop_future_rejected(obj, EFL_MODEL_ERROR_INCORRECT_VALUE);

   r = calloc(count + 1, sizeof (Eina_Future *));
   if (!r) return efl_loop_future_rejected(obj, ENOMEM);

   mapping = calloc(count, sizeof (Efl_Filter_Model_Mapping *));
   if (!mapping) goto on_error;

   for (i = 0; i < count; i++)
     {
        uint64_t lookup = start + i;

        mapping[i] = (void *)eina_rbtree_inline_lookup(pd->mapping,
                                                       &lookup, sizeof (uint64_t),
                                                       _filter_mapping_looking_cb, NULL);
        if (!mapping[i]) goto on_error;
     }

   for (i = 0; i < count; i++)
     {
        r[i] = efl_model_children_slice_get(efl_super(obj, EFL_FILTER_MODEL_CLASS),
                                            mapping[i]->original, 1);
        r[i] = efl_future_then(obj, r[i], .success_type = EINA_VALUE_TYPE_ARRAY,
                               .success = _filter_remove_array,
                               .data = mapping[i]);
        if (!r) goto on_error;
     }
   r[i] = EINA_FUTURE_SENTINEL;

   f = efl_future_then(obj, eina_future_all_array(r));
   free(r);
   free(mapping);

   return f;

 on_error:
   free(mapping);

   if (r)
     for (i = 0; i < count; i ++)
       if (r[i]) eina_future_cancel(r[i]);
   free(r);

   return efl_loop_future_rejected(obj, err);
}

// This future receive an array of boolean that indicate if a fetched object is to be kept
static Eina_Value
_efl_filter_model_array_result_request(Eo *o EINA_UNUSED, void *data, const Eina_Value v)
{
   Efl_Filter_Model_Data *pd = data;
   unsigned int i, len;
   Eina_Bool b;

   EINA_VALUE_ARRAY_FOREACH(&v, len, i, b)
     {
        Efl_Filter_Model_Mapping *mapping;

        if (!b) continue;

        mapping = calloc(1, sizeof (Efl_Filter_Model_Mapping));
        if (!mapping) continue;

        EINA_REFCOUNT_INIT(mapping);
        mapping->original = i;
        mapping->mapped = pd->filter.count++;

        // Insert in tree here
        pd->mapping = eina_rbtree_inline_insert(pd->mapping, EINA_RBTREE_GET(mapping),
                                                _filter_mapping_cmp_cb, NULL);
     }

   return v;
}

// This future receive an array of children object
static Eina_Value
_efl_filter_model_array_fetch(Eo *o, void *data, const Eina_Value v)
{
   Efl_Filter_Model_Data *pd = data;
   unsigned int i, len;
   Eo *target = NULL;
   Eina_Future **array = NULL;
   Eina_Future *r;

   if (!eina_value_array_count(&v)) return v;

   array = malloc(eina_value_array_count(&v) * sizeof (Eina_Future*));
   if (!array) return eina_value_error_init(ENOMEM);

   EINA_VALUE_ARRAY_FOREACH(&v, len, i, target)
     {
        array[i] = pd->filter.cb(pd->filter.data, o, target);
     }

   array[len] = EINA_FUTURE_SENTINEL;

   r = eina_future_all_array(array);
   efl_future_then(o, r, .success_type = EINA_VALUE_TYPE_ARRAY,
                   .success = _efl_filter_model_array_result_request,
                   .data = pd);

   free(array);

   return v;
}

static unsigned int
_efl_filter_model_efl_model_children_count_get(const Eo *obj, Efl_Filter_Model_Data *pd)
{
   if (!pd->counting_started && pd->filter.cb)
     {
        pd->counted = efl_model_children_count_get(efl_super(obj, EFL_FILTER_MODEL_CLASS));
        if (pd->counted > 0)
          {
             Eina_Future *f;

             f = efl_model_children_slice_get(efl_super(obj, EFL_FILTER_MODEL_CLASS),
                                              0, pd->counted);
             efl_future_then(obj, f, .success_type = EINA_VALUE_TYPE_ARRAY,
                             .success = _efl_filter_model_array_fetch,
                             .data = pd);
          }
     }

   return pd->filter.count;
}

static Eina_Value *
_efl_filter_model_efl_model_property_get(const Eo *obj, Efl_Filter_Model_Data *pd,
                                         const char *property)
{
   if (pd->self && !strcmp(property, _CHILD_INDEX))
     {
        return eina_value_uint64_new(pd->self->mapped);
     }

   return efl_model_property_get(efl_super(obj, EFL_FILTER_MODEL_CLASS), property);
}

static unsigned int
_efl_filter_model_efl_composite_model_index_get(const Eo *obj, Efl_Filter_Model_Data *pd)
{
   if (pd->self) return pd->self->mapped;
   return efl_composite_model_index_get(efl_super(obj, EFL_FILTER_MODEL_CLASS));
}

#include "efl_filter_model.eo.c"
