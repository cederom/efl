#ifndef EFL_DND_PRIVATE_H
#define EFL_DND_PRIVATE_H

#include "efl_selection_private.h"

#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#define EFL_DND_PROTECTED

#include <Elementary.h>
#include "elm_priv.h"

//typedef struct _Efl_Ui_Dnd_Drag_Data Efl_Ui_Dnd_Drag_Data;
//typedef struct _Efl_Ui_Dnd_Drop_Data Efl_Ui_Dnd_Drop_Data;
typedef struct _Efl_Ui_Dnd_Data Efl_Ui_Dnd_Data;

struct _Efl_Ui_Dnd_Data
{
   int type;
   Ecore_Event_Handler *notify_handler;
   Efl_Promise *promise;

   //icon create
   Efl_Dnd_Drag_Icon_Create icon_create;
   void *icon_create_data;
   Eina_Free_Cb icon_create_free_cb;
   //
   Efl_Selection_Action action;
   Efl_Selection_Format format;
};

/*struct _Efl_Ui_Dnd_Drop_Data
{
   Efl_Promise *promise;
   Efl_Selection_Format format;
};*/

#endif