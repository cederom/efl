#include "efl_animation_alpha_private.h"

#define MY_CLASS EFL_ANIMATION_ALPHA_CLASS

EOLIAN static void
_efl_animation_alpha_alpha_set(Eo *eo_obj EINA_UNUSED,
                               Efl_Animation_Alpha_Data *pd,
                               double from_alpha,
                               double to_alpha)
{
   pd->from.alpha = from_alpha;
   pd->to.alpha = to_alpha;
}

EOLIAN static void
_efl_animation_alpha_alpha_get(Eo *eo_obj EINA_UNUSED,
                               Efl_Animation_Alpha_Data *pd,
                               double *from_alpha,
                               double *to_alpha)
{
   if (from_alpha)
     *from_alpha = pd->from.alpha;
   if (to_alpha)
     *to_alpha = pd->to.alpha;
}


EOLIAN static void
_efl_animation_alpha_efl_playable_progress_set(Eo *eo_obj,
                            Efl_Animation_Alpha_Data *pd EINA_UNUSED,
                            double progress)
{
   double from_alpha, to_alpha;
   int cur_alpha;
   int i;

   efl_playable_progress_set(efl_super(eo_obj, MY_CLASS), progress);
   progress = efl_playable_progress_get(eo_obj);
   Efl_Canvas_Object *target = efl_animation_target_get(eo_obj);
   if (!target) return;

   efl_animation_alpha_get(eo_obj, &from_alpha, &to_alpha);
   cur_alpha = (int)(GET_STATUS(from_alpha, to_alpha, progress) * 255);

   for (i = 0; i < 4; i++)
     {
        efl_gfx_map_color_set(target, i, cur_alpha, cur_alpha, cur_alpha, cur_alpha);
     }
}

EOLIAN static Efl_Object *
_efl_animation_alpha_efl_object_constructor(Eo *eo_obj,
                                            Efl_Animation_Alpha_Data *pd)
{
   eo_obj = efl_constructor(efl_super(eo_obj, MY_CLASS));

   pd->from.alpha = 1.0;
   pd->to.alpha = 1.0;

   return eo_obj;
}

#include "efl_animation_alpha.eo.c"
