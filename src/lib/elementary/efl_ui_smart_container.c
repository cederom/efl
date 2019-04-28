#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#define EFL_PACK_LAYOUT_PROTECTED

#include <Elementary.h>
#include "elm_priv.h"
#include "efl_ui_widget_pager.h"
#include "efl_page_transition.h"

typedef struct _Efl_Ui_Smart_Container_Data
{
   Eina_List               *content_list;

   Efl_Ui_Box              *page_box;
   Eo                      *foreclip;
   Eo                      *backclip;
   Eo                      *event;
   Eo                      *page_root;

   Evas_Coord               x, y, w, h;

   struct {
      Eina_Size2D           sz;
   } page_spec;

   struct {
      int                   page;
      double                pos;
   } curr;

   Efl_Page_Transition     *transition;
   Efl_Page_Indicator      *indicator;

   Eina_Bool                fill_width: 1;
   Eina_Bool                fill_height: 1;

} Efl_Ui_Smart_Container_Data;

#define MY_CLASS EFL_UI_SMART_CONTAINER_CLASS

static void _unpack(Eo *obj, Efl_Ui_Smart_Container_Data *pd, Efl_Gfx_Entity *subobj, int index);

static int
clamp_index(Efl_Ui_Smart_Container_Data *pd, int index)
{
   if (index < ((int)eina_list_count(pd->content_list)) * -1)
     return -1;
   else if (index > (int)eina_list_count(pd->content_list) - 1)
     return 1;
   return 0;
}

static int
index_rollover(Efl_Ui_Smart_Container_Data *pd, int index)
{
   int c = eina_list_count(pd->content_list);
   if (index < c * -1)
     return 0;
   else if (index > c - 1)
     return c - 1;
   else if (index < 0)
     return index + c;
   return index;
}

static void
_resize_cb(void *data, const Efl_Event *ev)
{
   Efl_Ui_Smart_Container_Data *pd = data;
   Eina_Size2D sz;

   sz = efl_gfx_entity_size_get(ev->object);

   pd->w = sz.w;
   pd->h = sz.h;

   if (pd->fill_width) pd->page_spec.sz.w = pd->w;
   if (pd->fill_height) pd->page_spec.sz.h = pd->h;

   if (pd->transition)
     efl_ui_smart_container_transition_page_size_set(pd->transition, pd->page_spec.sz);
   else
     {
        efl_gfx_entity_size_set(pd->foreclip, sz);
        efl_gfx_entity_size_set(pd->page_box, pd->page_spec.sz);
        efl_gfx_entity_position_set(pd->page_box,
                                    EINA_POSITION2D(pd->x + (pd->w / 2) - (pd->page_spec.sz.w / 2),
                                                    pd->y + (pd->h / 2) - (pd->page_spec.sz.h / 2)));
     }
}

static void
_move_cb(void *data, const Efl_Event *ev)
{
   Efl_Ui_Smart_Container_Data *pd = data;
   Eina_Position2D pos;

   pos = efl_gfx_entity_position_get(ev->object);

   pd->x = pos.x;
   pd->y = pos.y;

   if (!pd->transition)
     {
        efl_gfx_entity_position_set(pd->foreclip, pos);
        efl_gfx_entity_position_set(pd->page_box,
                             EINA_POSITION2D(pd->x + (pd->w / 2) - (pd->page_spec.sz.w / 2),
                                             pd->y + (pd->h / 2) - (pd->page_spec.sz.h / 2)));
     }
}

EOLIAN static Eo *
_efl_ui_smart_container_efl_object_constructor(Eo *obj,
                                     Efl_Ui_Smart_Container_Data *pd)
{
   ELM_WIDGET_DATA_GET_OR_RETURN(obj, wd, NULL);

   if (!elm_widget_theme_klass_get(obj))
     elm_widget_theme_klass_set(obj, "pager");

   obj = efl_constructor(efl_super(obj, MY_CLASS));

   if (elm_widget_theme_object_set(obj, wd->resize_obj,
                                       elm_widget_theme_klass_get(obj),
                                       elm_widget_theme_element_get(obj),
                                       elm_widget_theme_style_get(obj)) == EFL_UI_THEME_APPLY_ERROR_GENERIC)
     CRI("Failed to set layout!");

   pd->curr.page = -1;
   pd->curr.pos = 0.0;

   pd->transition = NULL;
   pd->indicator = NULL;

   pd->fill_width = EINA_TRUE;
   pd->fill_height = EINA_TRUE;

   pd->page_spec.sz.w = -1;
   pd->page_spec.sz.h = -1;

   elm_widget_can_focus_set(obj, EINA_TRUE);

   pd->page_root = efl_add(EFL_CANVAS_GROUP_CLASS, evas_object_evas_get(obj));
   efl_content_set(efl_part(obj, "efl.page_root"), pd->page_root);

   efl_event_callback_add(pd->page_root, EFL_GFX_ENTITY_EVENT_SIZE_CHANGED, _resize_cb, pd);
   efl_event_callback_add(pd->page_root, EFL_GFX_ENTITY_EVENT_POSITION_CHANGED, _move_cb, pd);

   pd->page_box = efl_add(EFL_UI_BOX_CLASS, obj);
   efl_ui_widget_internal_set(pd->page_box, EINA_TRUE);
   efl_canvas_group_member_add(pd->page_root, pd->page_box);

   pd->foreclip = efl_add(EFL_CANVAS_RECTANGLE_CLASS,
                          evas_object_evas_get(obj));
   efl_canvas_group_member_add(pd->page_root, pd->foreclip);
   evas_object_static_clip_set(pd->foreclip, EINA_TRUE);
   efl_canvas_object_clipper_set(pd->page_box, pd->foreclip);

   pd->backclip = efl_add(EFL_CANVAS_RECTANGLE_CLASS,
                          evas_object_evas_get(obj));
   efl_canvas_group_member_add(pd->page_root, pd->backclip);
   evas_object_static_clip_set(pd->backclip, EINA_TRUE);
   efl_gfx_entity_visible_set(pd->backclip, EINA_FALSE);

   pd->event = efl_add(EFL_CANVAS_RECTANGLE_CLASS,
                       evas_object_evas_get(obj));
   evas_object_color_set(pd->event, 0, 0, 0, 0);
   evas_object_repeat_events_set(pd->event, EINA_TRUE);
   efl_content_set(efl_part(obj, "efl.event"), pd->event);

   return obj;
}

EOLIAN static void
_efl_ui_smart_container_efl_object_invalidate(Eo *obj,
                                    Efl_Ui_Smart_Container_Data *pd)
{
   efl_invalidate(efl_super(obj, MY_CLASS));

   /* Since the parent of foreclip and backclip is evas, foreclip and backclip
    * are not deleted automatically when pager is deleted.
    * Therefore, foreclip and backclip are deleted manually here. */
   efl_del(pd->foreclip);
   efl_del(pd->backclip);
}

EOLIAN static int
_efl_ui_smart_container_efl_container_content_count(Eo *obj EINA_UNUSED,
                                          Efl_Ui_Smart_Container_Data *pd)
{
   return eina_list_count(pd->content_list);
}

static void
_child_inv(void *data, const Efl_Event *ev)
{
   Efl_Ui_Smart_Container_Data *pd = efl_data_scope_get(data, EFL_UI_PAGER_CLASS);
   int index = eina_list_data_idx(pd->content_list, ev->object);

   pd->content_list = eina_list_remove(pd->content_list, ev->object);

   if (((index == pd->curr.page) && ((index != 0) || (eina_list_count(pd->content_list) == 0))) ||
       (index < pd->curr.page))
     pd->curr.page--;
}

static Eina_Bool
_register_child(Eo *obj EINA_UNUSED, Efl_Ui_Smart_Container_Data *pd, Efl_Gfx_Entity *subobj)
{
   if (eina_list_data_find(pd->content_list, subobj))
     {
        ERR("Object %p is already part of this!", subobj);
        return EINA_FALSE;
     }
   if (!efl_ui_widget_sub_object_add(obj, subobj))
     return EINA_FALSE;

   if (!pd->transition)
     efl_canvas_object_clipper_set(subobj, pd->backclip);

   efl_event_callback_add(subobj, EFL_EVENT_INVALIDATE, _child_inv, obj);

   return EINA_TRUE;
}

static void
_update_internals(Eo *obj EINA_UNUSED, Efl_Ui_Smart_Container_Data *pd, Efl_Gfx_Entity *subobj EINA_UNUSED, int index)
{
   if (pd->curr.page >= index)
     pd->curr.page++;

   if (pd->transition)
     efl_ui_smart_container_transition_content_update(pd->transition, subobj, index, EINA_FALSE);
   if (pd->indicator)
     efl_ui_smart_container_indicator_content_update(pd->indicator, subobj, index, EINA_FALSE);

   if (eina_list_count(pd->content_list) == 1)
     efl_ui_smart_container_current_page_set(obj, 0);
}

EOLIAN static Eina_Bool
_efl_ui_smart_container_efl_pack_linear_pack_begin(Eo *obj EINA_UNUSED,
                                         Efl_Ui_Smart_Container_Data *pd,
                                         Efl_Gfx_Entity *subobj)
{
   if (!_register_child(obj, pd, subobj)) return EINA_FALSE;
   pd->content_list = eina_list_prepend(pd->content_list, subobj);
   _update_internals(obj, pd, subobj, 0);
   return EINA_TRUE;
}

EOLIAN static Eina_Bool
_efl_ui_smart_container_efl_pack_linear_pack_end(Eo *obj EINA_UNUSED,
                                       Efl_Ui_Smart_Container_Data *pd,
                                       Efl_Gfx_Entity *subobj)
{
   if (!_register_child(obj, pd, subobj)) return EINA_FALSE;
   pd->content_list = eina_list_append(pd->content_list, subobj);
   _update_internals(obj, pd, subobj, eina_list_count(pd->content_list) - 1);
   return EINA_TRUE;
}

EOLIAN static Eina_Bool
_efl_ui_smart_container_efl_pack_linear_pack_before(Eo *obj EINA_UNUSED,
                                          Efl_Ui_Smart_Container_Data *pd,
                                          Efl_Gfx_Entity *subobj,
                                          const Efl_Gfx_Entity *existing)
{
   if (!_register_child(obj, pd, subobj)) return EINA_FALSE;
   int index = eina_list_data_idx(pd->content_list, (void *)existing);
   if (index == -1) return EINA_FALSE;
   pd->content_list = eina_list_prepend_relative(pd->content_list, subobj, existing);
   _update_internals(obj, pd, subobj, index);
   return EINA_TRUE;
}

EOLIAN static Eina_Bool
_efl_ui_smart_container_efl_pack_linear_pack_after(Eo *obj EINA_UNUSED,
                                         Efl_Ui_Smart_Container_Data *pd,
                                         Efl_Gfx_Entity *subobj,
                                         const Efl_Gfx_Entity *existing)
{
   if (!_register_child(obj, pd, subobj)) return EINA_FALSE;
   int index = eina_list_data_idx(pd->content_list, (void *)existing);
   if (index == -1) return EINA_FALSE;
   pd->content_list = eina_list_append_relative(pd->content_list, subobj, existing);
   _update_internals(obj, pd, subobj, index + 1);
   return EINA_TRUE;
}

EOLIAN static Eina_Bool
_efl_ui_smart_container_efl_pack_linear_pack_at(Eo *obj,
                                      Efl_Ui_Smart_Container_Data *pd,
                                      Efl_Gfx_Entity *subobj,
                                      int index)
{
   Efl_Gfx_Entity *existing = NULL;

   if (!_register_child(obj, pd, subobj)) return EINA_FALSE;
   int clamp = clamp_index(pd, index);
   int pass_index = -1;
   if (clamp == 0)
     {
        existing = eina_list_nth(pd->content_list, index_rollover(pd, index));
        pd->content_list = eina_list_prepend_relative(
           pd->content_list, subobj, existing);
     }
   else if (clamp == 1)
     {
        pd->content_list = eina_list_append(pd->content_list, subobj);
        pass_index = eina_list_count(pd->content_list);
     }
   else
     {
        pd->content_list = eina_list_prepend(pd->content_list, subobj);
        pass_index = 0;
     }
   _update_internals(obj, pd, subobj, pass_index);

   return EINA_TRUE;
}

EOLIAN static Efl_Gfx_Entity *
_efl_ui_smart_container_efl_pack_linear_pack_content_get(Eo *obj EINA_UNUSED,
                                               Efl_Ui_Smart_Container_Data *pd,
                                               int index)
{
   return eina_list_nth(pd->content_list, index_rollover(pd, index));
}

EOLIAN static int
_efl_ui_smart_container_efl_pack_linear_pack_index_get(Eo *obj EINA_UNUSED,
                                             Efl_Ui_Smart_Container_Data *pd,
                                             const Efl_Gfx_Entity *subobj)
{
   return eina_list_data_idx(pd->content_list, (void *)subobj);
}

EOLIAN static void
_efl_ui_smart_container_current_page_set(Eo *obj EINA_UNUSED,
                               Efl_Ui_Smart_Container_Data *pd,
                               int index)
{

   if ((index < 0) || (index > ((int)eina_list_count(pd->content_list) - 1)))
     {
        ERR("index %d out of range", index);
        return;
     }

   if (!pd->transition)
     {
        Eo *curr;

        curr = eina_list_nth(pd->content_list, pd->curr.page);
        if (curr)
          efl_pack_unpack(pd->page_box, curr);
        efl_canvas_object_clipper_set(curr, pd->backclip);

        pd->curr.page = index;
        curr = eina_list_nth(pd->content_list, pd->curr.page);
        efl_pack(pd->page_box, curr);

        return;
     }
   if (pd->transition)
     efl_ui_smart_container_transition_request_switch(pd->transition, pd->curr.page, index);
   pd->curr.page = index;
}

EOLIAN static int
_efl_ui_smart_container_current_page_get(const Eo *obj EINA_UNUSED,
                               Efl_Ui_Smart_Container_Data *pd)
{
   return pd->curr.page;
}

EOLIAN Eina_Size2D
_efl_ui_smart_container_page_size_get(const Eo *obj EINA_UNUSED,
                            Efl_Ui_Smart_Container_Data *pd)
{
   return pd->page_spec.sz;
}

EOLIAN static void
_efl_ui_smart_container_page_size_set(Eo *obj EINA_UNUSED,
                            Efl_Ui_Smart_Container_Data *pd,
                            Eina_Size2D sz)
{
   if (sz.w < -1 || sz.h < -1) return;

   pd->page_spec.sz = sz;
   pd->fill_width = sz.w == -1 ? EINA_TRUE : EINA_FALSE;
   pd->fill_height = sz.h == -1 ? EINA_TRUE : EINA_FALSE;

   if (pd->transition)
     efl_page_transition_page_size_set(pd->transition, pd->page_spec.sz);
   else
     {
        efl_gfx_entity_size_set(pd->page_box, pd->page_spec.sz);
        efl_gfx_entity_position_set(pd->page_box,
                                    EINA_POSITION2D(pd->x + (pd->w / 2) - (pd->page_spec.sz.w / 2),
                                                    pd->y + (pd->h / 2) - (pd->page_spec.sz.h / 2)));
     }
}

static void
_unpack_all(Eo *obj EINA_UNUSED,
            Efl_Ui_Smart_Container_Data *pd,
            Eina_Bool clear)
{
   Eo *subobj;

   pd->curr.page = -1;
   pd->curr.pos = 0.0;

   if (!pd->transition)
     {
        subobj = eina_list_nth(pd->content_list, pd->curr.page);
        if (subobj)
          efl_pack_unpack(pd->page_box, subobj);
        pd->curr.page = -1;
     }

   if (clear)
     {
        EINA_LIST_FREE(pd->content_list, subobj)
          {
             if (pd->transition)
               efl_ui_smart_container_transition_content_update(pd->transition, subobj, eina_list_data_idx(pd->content_list, subobj), EINA_TRUE);
             if (pd->indicator)
               efl_ui_smart_container_transition_content_update(pd->indicator, subobj, eina_list_data_idx(pd->content_list, subobj), EINA_TRUE);
             efl_event_callback_del(subobj, EFL_EVENT_INVALIDATE, _child_inv, obj);
             evas_object_del(subobj);
          }
     }
   else
     {
        EINA_LIST_FREE(pd->content_list, subobj)
          {
             if (pd->transition)
               efl_ui_smart_container_transition_content_update(pd->transition, subobj, eina_list_data_idx(pd->content_list, subobj), EINA_TRUE);
             if (pd->indicator)
               efl_ui_smart_container_transition_content_update(pd->indicator, subobj, eina_list_data_idx(pd->content_list, subobj), EINA_TRUE);
             efl_event_callback_del(subobj, EFL_EVENT_INVALIDATE, _child_inv, obj);
             efl_canvas_object_clipper_set(subobj, NULL);
          }
     }
}

EOLIAN static Eina_Bool
_efl_ui_smart_container_efl_pack_pack_clear(Eo *obj EINA_UNUSED,
                                  Efl_Ui_Smart_Container_Data *pd)
{
   _unpack_all(obj, pd, EINA_TRUE);

   return EINA_TRUE;
}

EOLIAN static Eina_Bool
_efl_ui_smart_container_efl_pack_unpack_all(Eo *obj EINA_UNUSED,
                                  Efl_Ui_Smart_Container_Data *pd)
{
   _unpack_all(obj, pd, EINA_FALSE);

   return EINA_TRUE;
}

static void
_unpack(Eo *obj,
        Efl_Ui_Smart_Container_Data *pd,
        Efl_Gfx_Entity *subobj,
        int index)
{
   int self_index = eina_list_data_idx(pd->content_list, subobj);
   int self_curr_page = pd->curr.page;
   pd->content_list = eina_list_remove(pd->content_list, subobj);
   _elm_widget_sub_object_redirect_to_top(obj, subobj);

   if (((index == pd->curr.page) && ((index != 0) || (!eina_list_count(pd->content_list)))) ||
       (index < pd->curr.page))
     pd->curr.page--;

   if (pd->transition)
     {
        efl_ui_smart_container_transition_content_update(pd->transition, subobj, index, EINA_TRUE);
     }
   else
     {
        if (self_curr_page == self_index)
          {
             efl_pack_unpack(pd->page_box, subobj);
             self_curr_page = pd->curr.page;
             pd->curr.page = -1;
             if (eina_list_count(pd->content_list) > 0 && self_curr_page > -1)
               {
                  efl_ui_smart_container_current_page_set(obj, MIN(self_curr_page, (int)eina_list_count(pd->content_list) - 1));
               }

          }
     }

   if (pd->indicator)
     efl_ui_smart_container_transition_content_update(pd->transition, subobj, index, EINA_TRUE);

   efl_event_callback_del(subobj, EFL_EVENT_INVALIDATE, _child_inv, obj);
}

EOLIAN static Eina_Bool
_efl_ui_smart_container_efl_pack_unpack(Eo *obj,
                              Efl_Ui_Smart_Container_Data *pd,
                              Efl_Gfx_Entity *subobj)
{
   if (!subobj) return EINA_FALSE;

   int index = eina_list_data_idx(pd->content_list, subobj);
   if (index == -1)
     {
        ERR("Item %p is not part of this container", subobj);
        return EINA_FALSE;
     }

   _unpack(obj, pd, subobj, index);

   return EINA_TRUE;
}

EOLIAN static Efl_Gfx_Entity *
_efl_ui_smart_container_efl_pack_linear_pack_unpack_at(Eo *obj,
                                             Efl_Ui_Smart_Container_Data *pd,
                                             int index)
{
   Efl_Gfx_Entity *subobj = eina_list_nth(pd->content_list, index_rollover(pd, index_rollover(pd, index)));

   _unpack(obj, pd, subobj, index);

   return subobj;
}

EOLIAN static Eina_Bool
_efl_ui_smart_container_efl_pack_pack(Eo *obj, Efl_Ui_Smart_Container_Data *pd EINA_UNUSED, Efl_Gfx_Entity *subobj)
{
   return efl_pack_begin(obj, subobj);
}

EOLIAN static Eina_Iterator*
_efl_ui_smart_container_efl_container_content_iterate(Eo *obj EINA_UNUSED, Efl_Ui_Smart_Container_Data *pd)
{
  return eina_list_iterator_new(pd->content_list);
}

static void
_pos_updated(void *data, const Efl_Event *event)
{
   Efl_Ui_Smart_Container_Data *pd = efl_data_scope_get(data, MY_CLASS);
   double progress = *((double*)event->info);
   if (pd->indicator)
     {
        if (progress < -1.0) progress = -1.0;
        if (progress > eina_list_count(pd->content_list)) progress = eina_list_count(pd->content_list);
        efl_ui_smart_container_indicator_position_update(pd->indicator, progress);
     }
}

EOLIAN static void
_efl_ui_smart_container_transition_set(Eo *obj, Efl_Ui_Smart_Container_Data *pd, Efl_Ui_Smart_Container_Transition *transition)
{
   if (pd->transition)
     {
        efl_ui_smart_container_transition_smart_container_set(pd->transition, NULL, NULL);
        efl_del(pd->transition);
     }
   pd->transition = transition;
   if (pd->transition)
     {
        efl_ui_smart_container_transition_smart_container_set(pd->transition, obj,
          pd->page_root);
        efl_event_callback_add(pd->transition, EFL_UI_SMART_CONTAINER_TRANSITION_EVENT_POS_UPDATE, _pos_updated, obj);
     }

}

EOLIAN static Efl_Ui_Smart_Container_Transition*
_efl_ui_smart_container_transition_get(const Eo *obj EINA_UNUSED, Efl_Ui_Smart_Container_Data *pd)
{
   return pd->transition;
}


EOLIAN static void
_efl_ui_smart_container_indicator_set(Eo *obj, Efl_Ui_Smart_Container_Data *pd, Efl_Ui_Smart_Container_Indicator *indicator)
{
   if (pd->indicator)
     {
        efl_ui_smart_container_indicator_smart_container_set(pd->indicator, obj);
        efl_del(pd->indicator);
     }
   pd->indicator = indicator;
   if (pd->indicator)
     {
        efl_ui_smart_container_indicator_smart_container_set(pd->indicator, obj);
     }
}

EOLIAN static Efl_Ui_Smart_Container_Indicator*
_efl_ui_smart_container_indicator_get(const Eo *obj EINA_UNUSED, Efl_Ui_Smart_Container_Data *pd)
{
   return pd->indicator;
}


#include "efl_ui_smart_container.eo.c"
