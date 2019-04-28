#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#define EFL_PACK_LAYOUT_PROTECTED

#include <Elementary.h>
#include "elm_priv.h"
#include "efl_ui_widget_pager.h"
#include "efl_page_transition.h"

typedef struct {
   Efl_Ui_Smart_Container * container;
   Efl_Gfx_Entity *group, *foreclip, *backclip;
   Eina_Size2D page_size;
   struct {
      Eina_Bool active;
      int to;
      double from, progress;
      double start_time;
      double max_time;
   } transition;
   struct {
      Eina_Bool active;
      int from;
      Eina_Position2D mouse_start;
   } mouse_move;
} Efl_Ui_Smart_Container_Transition_Scroll_Data;

#define MY_CLASS EFL_UI_SMART_CONTAINER_TRANSITION_SCROLL_CLASS

static void _page_set_animation(void *data, const Efl_Event *event);

static void
_propagate_progress(Eo *obj, double pos)
{
   efl_event_callback_call(obj, EFL_UI_SMART_CONTAINER_TRANSITION_EVENT_POS_UPDATE, &pos);
}

static void
_apply_box_properties(Eo *obj, Efl_Ui_Smart_Container_Transition_Scroll_Data *pd)
{
   Eina_Rect geometry = EINA_RECT_EMPTY();
   Eina_Rect group_pos = efl_gfx_entity_geometry_get(pd->group);
   double current_pos = pd->transition.from + ((double)pd->transition.to - pd->transition.from)*pd->transition.progress;

   efl_gfx_entity_geometry_set(pd->foreclip, group_pos);
   //first calculate the size
   geometry.h = pd->page_size.h;
   geometry.w = (pd->page_size.w);

   //calculate the position
   geometry.y = (group_pos.y + group_pos.h/2)-pd->page_size.h/2;
        _propagate_progress(obj, current_pos);

   _propagate_progress(obj, current_pos);

   for (int i = 0; i < efl_content_count(pd->container) ; ++i)
     {
        double diff = i - current_pos;
        Efl_Gfx_Entity *elem = efl_pack_content_get(pd->container, i);

        geometry.x = (group_pos.x + group_pos.w/2)-(pd->page_size.w/2 - diff*pd->page_size.w);
        if (!eina_rectangles_intersect(&geometry.rect, &group_pos.rect))
          {
             efl_canvas_object_clipper_set(elem, pd->backclip);
          }
        else
          {
             efl_gfx_entity_geometry_set(elem, geometry);
             efl_canvas_object_clipper_set(elem, pd->foreclip);
          }
     }
}

static void
_resize_cb(void *data, const Efl_Event *ev EINA_UNUSED)
{
   _apply_box_properties(data, efl_data_scope_get(data, EFL_UI_SMART_CONTAINER_TRANSITION_SCROLL_CLASS));
}

static void
_move_cb(void *data, const Efl_Event *ev EINA_UNUSED)
{
   _apply_box_properties(data, efl_data_scope_get(data, EFL_UI_SMART_CONTAINER_TRANSITION_SCROLL_CLASS));
}

static void
_mouse_down_cb(void *data,
               const Efl_Event *event)
{
   Efl_Input_Pointer *ev = event->info;
   Eo *obj = data;
   Efl_Ui_Smart_Container_Transition_Scroll_Data *pd = efl_data_scope_get(obj, MY_CLASS);

   if (efl_input_pointer_button_get(ev) != 1) return;
   if (efl_input_event_flags_get(ev) & EFL_INPUT_FLAGS_PROCESSED) return;

   if (efl_content_count(pd->container) == 0) return;

   efl_event_callback_del(pd->container, EFL_CANVAS_OBJECT_EVENT_ANIMATOR_TICK, _page_set_animation, obj);

   pd->mouse_move.active = EINA_TRUE;
   pd->mouse_move.from = efl_ui_smart_container_current_page_get(pd->container);
   pd->mouse_move.mouse_start = efl_input_pointer_position_get(ev);
}

static void
_mouse_move_cb(void *data,
               const Efl_Event *event)
{
   Efl_Input_Pointer *ev = event->info;
   Eo *obj = data;
   Efl_Ui_Smart_Container_Transition_Scroll_Data *pd = efl_data_scope_get(obj, MY_CLASS);
   Eina_Position2D pos;
   int pos_y_diff;

   if (efl_input_event_flags_get(ev) & EFL_INPUT_FLAGS_PROCESSED) return;
   if (!pd->mouse_move.active) return;

   pos = efl_input_pointer_position_get(ev);
   pos_y_diff = pd->mouse_move.mouse_start.x - pos.x;

   pd->transition.from = pd->mouse_move.from;
   pd->transition.progress = (double)pos_y_diff / (double)pd->page_size.w;
   pd->transition.to = pd->transition.from + 1;

   _propagate_progress(data, pd->transition.from + pd->transition.progress);

   _apply_box_properties(obj, pd);
}

static void
_mouse_up_cb(void *data,
             const Efl_Event *event)
{
   Efl_Input_Pointer *ev = event->info;
   Eo *obj = data;
   Efl_Ui_Smart_Container_Transition_Scroll_Data *pd = efl_data_scope_get(obj, MY_CLASS);

   if (efl_input_event_flags_get(ev) & EFL_INPUT_FLAGS_PROCESSED) return;
   if (!pd->mouse_move.active) return;

   double absolut_current_position = (double)pd->transition.from + pd->transition.progress;
   int result = round(absolut_current_position);

   efl_ui_smart_container_current_page_set(pd->container, MIN(MAX(result, 0), efl_content_count(pd->container) - 1));
}

EFL_CALLBACKS_ARRAY_DEFINE(mouse_listeners,
  {EFL_EVENT_POINTER_DOWN, _mouse_down_cb},
  {EFL_EVENT_POINTER_UP, _mouse_up_cb},
  {EFL_EVENT_POINTER_MOVE, _mouse_move_cb},
);

EOLIAN static void
_efl_ui_smart_container_transition_scroll_efl_ui_smart_container_transition_smart_container_set(Eo *obj, Efl_Ui_Smart_Container_Transition_Scroll_Data *pd, Efl_Ui_Smart_Container *smart_container, Efl_Canvas_Group *group)
{
   if (smart_container && group)
     {
        pd->container = smart_container;
        pd->group = group;
        efl_event_callback_add(pd->group, EFL_GFX_ENTITY_EVENT_SIZE_CHANGED, _resize_cb, obj);
        efl_event_callback_add(pd->group, EFL_GFX_ENTITY_EVENT_POSITION_CHANGED, _move_cb, obj);

        pd->foreclip = efl_add(EFL_CANVAS_RECTANGLE_CLASS,
                               evas_object_evas_get(group));
        evas_object_static_clip_set(pd->foreclip, EINA_TRUE);

        pd->backclip = efl_add(EFL_CANVAS_RECTANGLE_CLASS,
                               evas_object_evas_get(group));
        evas_object_static_clip_set(pd->backclip, EINA_TRUE);
        efl_gfx_entity_visible_set(pd->backclip, EINA_FALSE);

        for (int i = 0; i < efl_content_count(smart_container) ; ++i) {
           Efl_Gfx_Entity *elem = efl_pack_content_get(smart_container, i);
           efl_canvas_object_clipper_set(elem, pd->backclip);
           efl_canvas_group_member_add(pd->group, elem);
        }
        _apply_box_properties(obj, pd);

        Eo *event = efl_content_get(efl_part(pd->container, "efl.event"));
        efl_event_callback_array_add(event, mouse_listeners(), obj);
     }
}

EOLIAN static void
_efl_ui_smart_container_transition_scroll_efl_ui_smart_container_transition_content_update(Eo *obj EINA_UNUSED, Efl_Ui_Smart_Container_Transition_Scroll_Data *pd, Efl_Gfx_Entity *subobj, int index EINA_UNUSED, Eina_Bool deletion EINA_UNUSED)
{
   if (deletion)
     {
        efl_canvas_object_clipper_set(subobj, NULL);
        efl_canvas_group_member_remove(pd->group, subobj);
     }
   else
     {
        efl_canvas_object_clipper_set(subobj, pd->backclip);
        efl_canvas_group_member_add(pd->group, subobj);
     }
}

static void
_page_set_animation(void *data, const Efl_Event *event EINA_UNUSED)
{
   Efl_Ui_Smart_Container_Transition_Scroll_Data *pd = efl_data_scope_get(data, MY_CLASS);
   double p = (ecore_loop_time_get() - pd->transition.start_time) / pd->transition.max_time;

   if (p >= 1.0) p = 1.0;
   p = ecore_animator_pos_map(p, ECORE_POS_MAP_ACCELERATE, 0.0, 0.0);

   pd->transition.progress = p;
   _apply_box_properties(data, pd);

   if (EINA_DBL_EQ(p, 1.0))
     {
        efl_event_callback_del(pd->container, EFL_CANVAS_OBJECT_EVENT_ANIMATOR_TICK,
                               _page_set_animation, data);
        pd->transition.active = EINA_FALSE;
     }
}

EOLIAN static void
_efl_ui_smart_container_transition_scroll_efl_ui_smart_container_transition_request_switch(Eo *obj EINA_UNUSED, Efl_Ui_Smart_Container_Transition_Scroll_Data *pd, int from, int to)
{
   efl_event_callback_del(pd->container, EFL_CANVAS_OBJECT_EVENT_ANIMATOR_TICK, _page_set_animation, obj);
   //if there is a ongoing transition, try to guess a better time, and try copy over the position where we are right now
   if (pd->transition.active || pd->mouse_move.active)
     {
        pd->transition.from = MIN(pd->transition.from, pd->transition.to) + pd->transition.progress;
        pd->transition.max_time = MIN(fabs(pd->transition.progress), 1.0f);
        pd->mouse_move.active = EINA_FALSE;
     }
   else
     {
        pd->transition.from = from;
        pd->transition.max_time = 1.0;
     }
   pd->transition.start_time = ecore_loop_time_get();
   pd->transition.active = EINA_TRUE;
   pd->transition.to = to;
   efl_event_callback_add(pd->container, EFL_CANVAS_OBJECT_EVENT_ANIMATOR_TICK, _page_set_animation, obj);
}

EOLIAN static void
_efl_ui_smart_container_transition_scroll_efl_ui_smart_container_transition_page_size_set(Eo *obj EINA_UNUSED, Efl_Ui_Smart_Container_Transition_Scroll_Data *pd, Eina_Size2D size)
{
   pd->page_size = size;
   if (!pd->transition.active)
     _apply_box_properties(obj, pd);
}


#include "efl_ui_smart_container_transition_scroll.eo.c"
