#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#include <Elementary.h>
#include "elm_priv.h"
#ifdef HAVE_MMAN_H
# include <sys/mman.h>
#endif

#include "efl_selection_manager_private.h"

#define MY_CLASS EFL_SELECTION_MANAGER_CLASS
//#define MY_CLASS_NAME "Efl.Selection_Manager"

#define DEBUGON 1
#ifdef DEBUGON
# define sel_debug(fmt, args...) fprintf(stderr, __FILE__":%s:%d : " fmt "\n", __FUNCTION__, __LINE__, ##args)
#else
# define sel_debug(x...) do { } while (0)
#endif


//FIXME: data in selection_set and converter: pd???


/* TODO: this should not be an actual tempfile, but rather encode the object
 * as http://dataurl.net/ if it's an image or similar. Evas should support
 * decoding it as memfile. */
static Tmp_Info *
_tempfile_new(int size)
{
#ifdef HAVE_MMAN_H
   Tmp_Info *info;
   const char *tmppath = NULL;
   mode_t cur_umask;
   int len;

   info = calloc(1, sizeof(Tmp_Info));
   if (!info) return NULL;
#if defined(HAVE_GETUID) && defined(HAVE_GETEUID)
   if (getuid() == geteuid())
#endif
     tmppath = getenv("TMP");
   if (!tmppath) tmppath = P_tmpdir;
   len = snprintf(NULL, 0, "%s/%sXXXXXX", tmppath, "elmcnpitem-");
   if (len < 0) goto on_error;
   len++;
   info->filename = malloc(len);
   if (!info->filename) goto on_error;
   snprintf(info->filename,len,"%s/%sXXXXXX", tmppath, "elmcnpitem-");
   cur_umask = umask(S_IRWXO | S_IRWXG);
   info->fd = mkstemp(info->filename);
   umask(cur_umask);
   if (info->fd < 0) goto on_error;
# ifdef __linux__
     {
        char *tmp;
        /* And before someone says anything see POSIX 1003.1-2008 page 400 */
        long pid;

        pid = (long)getpid();
        /* Use pid instead of /proc/self: That way if can be passed around */
        len = snprintf(NULL,0,"/proc/%li/fd/%i", pid, info->fd);
        len++;
        tmp = malloc(len);
        if (tmp)
          {
             snprintf(tmp,len, "/proc/%li/fd/%i", pid, info->fd);
             unlink(info->filename);
             free(info->filename);
             info->filename = tmp;
          }
     }
# endif
   sel_debug("filename is %s\n", info->filename);
   if (size < 1) goto on_error;
   /* Map it in */
   if (ftruncate(info->fd, size))
     {
        perror("ftruncate");
        goto on_error;
     }
   eina_mmap_safety_enabled_set(EINA_TRUE);
   info->map = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, info->fd, 0);
   if (info->map == MAP_FAILED)
     {
        perror("mmap");
        goto on_error;
     }
   return info;

 on_error:
   if (info->fd >= 0) close(info->fd);
   info->fd = -1;
   /* Set map to NULL and return */
   info->map = NULL;
   info->len = 0;
   free(info->filename);
   free(info);
   return NULL;
#else
   (void) size;
   return NULL;
#endif
}

static int
_tmpinfo_free(Tmp_Info *info)
{
   if (!info) return 0;
   free(info->filename);
   free(info);
   return 0;
}


static Ecore_X_Window
_x11_xwin_get(Evas_Object *obj)
{
   if (!obj) return 0;

   Ecore_X_Window xwin = 0;
   //get top
   Evas_Object *top = obj;
   Evas_Object *parent = obj;
   while(parent)
     {
        top = parent;
        parent = efl_parent_get(parent);
     }
   if (efl_isa(top, EFL_UI_WIN_CLASS))
     {
        xwin = elm_win_xwindow_get(top);
     }
   if (!xwin)
     {
        Ecore_Evas *ee;
        Evas *evas = evas_object_evas_get(obj);
        if (!evas) return 0;
        ee = ecore_evas_ecore_evas_get(evas);
        if (!ee) return 0;

        while(!xwin)
          {
             const char *engine_name = ecore_evas_engine_name_get(ee);
             if (!strcmp(engine_name, ELM_BUFFER))
               {
                  ee = ecore_evas_buffer_ecore_evas_parent_get(ee);
                  if (!ee) return 0;
                  xwin = _elm_ee_xwin_get(ee);
               }
             else
               {
                  xwin = _elm_ee_xwin_get(ee);
                  if (!xwin) return 0;
               }
          }
     }

   return xwin;
}

static Eina_Bool
_x11_is_uri_type_data(X11_Cnp_Selection *sel EINA_UNUSED, Ecore_X_Event_Selection_Notify *notify)
{
   Ecore_X_Selection_Data *data;
   char *p;

   data = notify->data;
   sel_debug("data->format is %d %p %p", data->format, notify, data);
   if (data->content == ECORE_X_SELECTION_CONTENT_FILES) return EINA_TRUE;
   p = (char *)data->data;
   if (!p) return EINA_TRUE;
   sel_debug("Got %s", p);
   if (strncmp(p, "file:/", 6))
     {
        if (*p != '/') return EINA_FALSE;
     }
   return EINA_TRUE;
}

static Eina_Bool
_x11_data_preparer_text(Efl_Selection_Manager_Data *pd, Ecore_X_Event_Selection_Notify *notify,
      Efl_Selection_Data *ddata, Tmp_Info **tmp_info EINA_UNUSED)
{
   sel_debug("text data preparer");
   Ecore_X_Selection_Data *data = notify->data;
   ddata->format = EFL_SELECTION_FORMAT_TEXT;
   ddata->data = eina_memdup(data->data, data->length, EINA_TRUE);
   ddata->len = data->length;
   return EINA_TRUE;
}

static Eina_Bool
_x11_data_preparer_markup(Efl_Selection_Manager_Data *pd, Ecore_X_Event_Selection_Notify *notify,
      Efl_Selection_Data *ddata, Tmp_Info **tmp_info EINA_UNUSED)
{
   sel_debug("markup data preparer");
   Ecore_X_Selection_Data *data = notify->data;
   ddata->format = EFL_SELECTION_FORMAT_MARKUP;
   ddata->data = eina_memdup(data->data, data->length, EINA_TRUE);
   ddata->len = data->length;
   return EINA_TRUE;
}

/**
 * So someone is pasting an image into my entry or widget...
 */
static Eina_Bool
_x11_data_preparer_uri(Efl_Selection_Manager_Data *pd, Ecore_X_Event_Selection_Notify *notify,
      Efl_Selection_Data *ddata, Tmp_Info **tmp_info EINA_UNUSED)
{
   sel_debug("uri data preparer");
   Ecore_X_Selection_Data *data;
   Ecore_X_Selection_Data_Files *files;
   char *p, *stripstr = NULL;

   data = notify->data;
   sel_debug("data->format is %d %p %p\n", data->format, notify, data);
   if (data->content == ECORE_X_SELECTION_CONTENT_FILES)
     {
        Efreet_Uri *uri;
        Eina_Strbuf *strbuf;
        int i;

        sel_debug("got a files list\n");
        files = notify->data;
        /*
        if (files->num_files > 1)
          {
             // Don't handle many items <- this makes mr bigglesworth sad :(
             sel_debug("more then one file: Bailing\n");
             return EINA_FALSE;
          }
        stripstr = p = strdup(files->files[0]);
         */

        strbuf = eina_strbuf_new();
        if (!strbuf)
          return EINA_FALSE;

        for (i = 0; i < files->num_files ; i++)
          {
             uri = efreet_uri_decode(files->files[i]);
             if (uri)
               {
                  eina_strbuf_append(strbuf, uri->path);
                  efreet_uri_free(uri);
               }
             else
               {
                  eina_strbuf_append(strbuf, files->files[i]);
               }
             if (i < (files->num_files - 1))
               eina_strbuf_append(strbuf, "\n");
          }
        stripstr = eina_strbuf_string_steal(strbuf);
        eina_strbuf_free(strbuf);
     }
   else
     {
        Efreet_Uri *uri;

        p = (char *)eina_memdup((unsigned char *)data->data, data->length, EINA_TRUE);
        if (!p) return EINA_FALSE;
        uri = efreet_uri_decode(p);
        if (!uri)
          {
             /* Is there any reason why we care of URI without scheme? */
             if (p[0] == '/') stripstr = p;
             else free(p);
          }
        else
          {
             free(p);
             stripstr = strdup(uri->path);
             efreet_uri_free(uri);
          }
     }

   if (!stripstr)
     {
        sel_debug("Couldn't find a file\n");
        return EINA_FALSE;
     }
   free(pd->savedtypes->imgfile);
   if (pd->savedtypes->textreq)
     {
        pd->savedtypes->textreq = 0;
        pd->savedtypes->imgfile = stripstr;
     }
   else
     {
        ddata->format = EFL_SELECTION_FORMAT_IMAGE;
        ddata->data = stripstr;
        ddata->len = strlen(stripstr);
        pd->savedtypes->imgfile = NULL;
     }
   return EINA_TRUE;
}

/**
 * Just received an vcard, either through cut and paste, or dnd.
 */
static Eina_Bool
_x11_data_preparer_vcard(Efl_Selection_Manager_Data *pd, Ecore_X_Event_Selection_Notify *notify,
      Efl_Selection_Data *ddata, Tmp_Info **tmp_info EINA_UNUSED)
{
   sel_debug("vcard receive\n");
   Ecore_X_Selection_Data *data = notify->data;
   ddata->format = EFL_SELECTION_FORMAT_VCARD;
   ddata->data = eina_memdup(data->data, data->length, EINA_TRUE);
   ddata->len = data->length;
   return EINA_TRUE;
}

static Eina_Bool
_x11_data_preparer_image(Efl_Selection_Manager_Data *pd, Ecore_X_Event_Selection_Notify *notify,
      Efl_Selection_Data *ddata, Tmp_Info **tmp_info)
{
   Ecore_X_Selection_Data *data = notify->data;
   sel_debug("got a image file!\n");
   sel_debug("Size if %d\n", data->length);

   ddata->format = EFL_SELECTION_FORMAT_IMAGE;
   data = notify->data;

   Tmp_Info *tmp = _tempfile_new(data->length);
   if (!tmp) return EINA_FALSE;
   memcpy(tmp->map, data->data, data->length);
   munmap(tmp->map, data->length);
   ddata->data = strdup(tmp->filename);
   ddata->len = strlen(tmp->filename);
   *tmp_info = tmp;
   return EINA_TRUE;
}

/*
 * Callback to handle a targets response on a selection request:
 * So pick the format we'd like; and then request it.
 */
static Eina_Bool
_x11_notify_handler_targets(Efl_Selection_Manager_Data *pd, X11_Cnp_Selection *sel, Ecore_X_Event_Selection_Notify *notify)
{
   sel_debug("notify handler targets");
   Ecore_X_Selection_Data_Targets *targets;
   Ecore_X_Atom *atomlist;
   int i, j;

   targets = notify->data;
   atomlist = (Ecore_X_Atom *)(targets->data.data);
   for (j = (SELECTION_ATOM_LISTING_ATOMS + 1); j < SELECTION_N_ATOMS; j++)
     {
        sel_debug("\t%s %d", pd->atomlist[j].name, pd->atomlist[j].x_atom);
        if (!(pd->atomlist[j].format & sel->request_format)) continue;
        for (i = 0; i < targets->data.length; i++)
          {
             if ((pd->atomlist[j].x_atom == atomlist[i]) && (pd->atomlist[j].x_data_preparer))
               {
                  if (j == SELECTION_ATOM_TEXT_URILIST)
                    {
                       if (!_x11_is_uri_type_data(sel, notify)) continue;
                    }
                  sel_debug("Atom %s matches", pd->atomlist[j].name);
                  goto done;
               }
          }
     }
   sel_debug("Couldn't find anything that matches");
   return ECORE_CALLBACK_PASS_ON;
done:
   sel_debug("Sending request for %s, xwin=%#llx",
             pd->atomlist[j].name, (unsigned long long)sel->xwin);
   sel->request(sel->xwin, pd->atomlist[j].name);
   return ECORE_CALLBACK_PASS_ON;
}


/*
 * Response to a selection notify:
 *  - So we have asked for the selection list.
 *  - If it's the targets list, parse it, and fire of what we want,
 *    else it's the data we want.
 */
static Eina_Bool
_efl_sel_manager_x11_selection_notify(void *udata, int type EINA_UNUSED, void *event)
{
   Efl_Selection_Manager_Data *pd = udata;
   Ecore_X_Event_Selection_Notify *ev = event;
   X11_Cnp_Selection *sel;
   int i;

   sel_debug("selection notify callback: %d",ev->selection);
   switch (ev->selection)
     {
      case ECORE_X_SELECTION_PRIMARY:
        sel = pd->sellist + EFL_SELECTION_TYPE_PRIMARY;
        break;
      case ECORE_X_SELECTION_SECONDARY:
        sel = pd->sellist + EFL_SELECTION_TYPE_SECONDARY;
        break;
      case ECORE_X_SELECTION_XDND:
        sel = pd->sellist + EFL_SELECTION_TYPE_DND;
        break;
      case ECORE_X_SELECTION_CLIPBOARD:
        sel = pd->sellist + EFL_SELECTION_TYPE_CLIPBOARD;
        break;
      default:
        return ECORE_CALLBACK_PASS_ON;
     }
   sel_debug("Target is %s", ev->target);

   if (ev->selection != ECORE_X_SELECTION_XDND &&
       (!strcmp(ev->target, "TARGETS") || !strcmp(ev->target, "ATOMS")))
     {
        _x11_notify_handler_targets(pd, sel, ev);
        return ECORE_CALLBACK_PASS_ON;
     }
   for (i = 0; i < SELECTION_N_ATOMS; i++)
     {
        if (!strcmp(ev->target, pd->atomlist[i].name))
          {
             if (pd->atomlist[i].x_data_preparer)
               {
                  Efl_Selection_Data ddata;
                  Tmp_Info *tmp_info = NULL;
                  Eina_Bool success;
                  ddata.data = NULL;
                  sel_debug("Found something: %s", pd->atomlist[i].name);
                  success = pd->atomlist[i].x_data_preparer(pd, ev, &ddata, &tmp_info);
                  if ((pd->atomlist[i].format == EFL_SELECTION_FORMAT_IMAGE) &&
                      (pd->savedtypes->imgfile))
                    break;
                  if (ev->selection == ECORE_X_SELECTION_XDND)
                    {
                       if (success)
                         {
#if 0
                            Dropable *dropable;
                            Eina_List *l;
                            sel_debug("drag & drop\n");
                            EINA_LIST_FOREACH(drops, l, dropable)
                              {
                                 if (dropable->obj == sel->request_widget) break;
                                 dropable = NULL;
                              }
                            if (dropable)
                              {
                                 Dropable_Cbs *cbs;
                                 Eina_Inlist *itr;
                                 ddata.x = savedtypes->x;
                                 ddata.y = savedtypes->y;
                                 EINA_INLIST_FOREACH_SAFE(dropable->cbs_list, itr, cbs)
                                    if ((cbs->types & dropable->last.format) && cbs->dropcb)
                                       cbs->dropcb(cbs->dropdata, dropable->obj, &ddata);
                              }
#endif
                         }
                       /* We have to finish DnD, no matter what */
                       ecore_x_dnd_send_finished();
                    }
                  else if (sel->data_func && success)
                    {
                       ddata.x = ddata.y = 0;
                       sel->data_func(sel->data_func_data, sel->request_obj, &ddata);
                    }
                  free(ddata.data);
                  if (tmp_info) _tmpinfo_free(tmp_info);
               }
             else sel_debug("Ignored: No handler!");
             break;
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static void
_selection_loss_data_clear_cb(void *data)
{
   Efl_Selection_Type *lt = data;
   free(lt);
}

static Eina_Bool
_x11_selection_clear(void *data, int type EINA_UNUSED, void *event)
{
   Efl_Selection_Manager_Data *pd = data;
   Ecore_X_Event_Selection_Clear *ev = event;
   X11_Cnp_Selection *sel;
   unsigned int i;
   ERR("In");
   /*if (pd->promise)
     {
        Efl_Selection_Type *lt = malloc(sizeof(Efl_Selection_Type));
        *lt = pd->loss_type;
        efl_promise_value_set(pd->promise, lt, _selection_loss_data_clear_cb);
        pd->promise = NULL;
     }*/
   for (i = ELM_SEL_TYPE_PRIMARY; i <= ELM_SEL_TYPE_CLIPBOARD; i++)
     {
        if (pd->sellist[i].ecore_sel == ev->selection) break;
     }
   sel_debug("selection %d clear", i);
   /* Not me... Don't care */
   if (i > ELM_SEL_TYPE_CLIPBOARD) return ECORE_CALLBACK_PASS_ON;

   sel = pd->sellist + i;

   efl_event_callback_call(sel->owner, EFL_SELECTION_EVENT_SELECTION_LOSS, NULL);
   sel->owner = NULL;

   return ECORE_CALLBACK_PASS_ON;
}

//note: sel_set: data is pd -> converter: data is pd
//->return data from converter: real buffer data ready to paste


static Efl_Selection_Format
_get_selection_type(void *data)
{
   Efl_Selection_Manager_Data *pd = *(Efl_Selection_Manager_Data **)data;
   if (pd->has_sel)
     {
        sel_debug("has sel");
        if (pd->active_type > EFL_SELECTION_TYPE_CLIPBOARD)
          return EFL_SELECTION_FORMAT_NONE;
        sel_debug("has active type: %d, pd active_format: %d", pd->active_type, pd->active_format);
        if ((pd->sellist[pd->active_type].format >= EFL_SELECTION_FORMAT_TARGETS) &&
            (pd->sellist[pd->active_type].format <= EFL_SELECTION_FORMAT_HTML))
          return pd->sellist[pd->active_type].format;
     }
   sel_debug("has no sel");
   return EFL_SELECTION_FORMAT_NONE;
}

static Eina_Bool
_x11_general_converter(char *target EINA_UNUSED, void *data, int size, void **data_ret, int *size_ret, Ecore_X_Atom *ttype EINA_UNUSED, int *typesize EINA_UNUSED)
{
   Efl_Selection_Manager_Data *pd = *(Efl_Selection_Manager_Data **)data;
   if (_get_selection_type(data) == EFL_SELECTION_FORMAT_NONE)
     {
        //FIXME: Check this case: remove or not
        if (data_ret)
          {
             *data_ret = malloc(size * sizeof(char) + 1);
             if (!*data_ret) return EINA_FALSE;
             memcpy(*data_ret, data, size);
             ((char**)(data_ret))[0][size] = 0;
          }
        if (size_ret) *size_ret = size;
     }
   else
     {
        X11_Cnp_Selection *sel = pd->sellist + pd->active_type;
        if (sel->selbuf)
          {
             if (data_ret) *data_ret = strdup(sel->selbuf);
             if (size_ret) *size_ret = strlen(sel->selbuf);
          }
        else
          {
             if (data_ret) *data_ret = NULL;
             if (size_ret) *size_ret = 0;
          }
     }
   return EINA_TRUE;
}

static Eina_Bool
_x11_targets_converter(char *target EINA_UNUSED, void *data, int size EINA_UNUSED, void **data_ret, int *size_ret, Ecore_X_Atom *ttype, int *typesize)
{
   int i, count;
   Ecore_X_Atom *aret;
   X11_Cnp_Selection *sel;
   Efl_Selection_Format seltype;

   if (!data_ret) return EINA_FALSE;
   Efl_Selection_Manager_Data *pd = *(Efl_Selection_Manager_Data **)data;
   seltype = pd->atomlist[pd->active_format].format;

   for (i = SELECTION_ATOM_LISTING_ATOMS + 1, count = 0; i < SELECTION_N_ATOMS ; i++)
     {
        if (seltype & pd->atomlist[i].format) count++;
     }
   aret = malloc(sizeof(Ecore_X_Atom) * count);
   if (!aret) return EINA_FALSE;
   for (i = SELECTION_ATOM_LISTING_ATOMS + 1, count = 0; i < SELECTION_N_ATOMS ; i++)
     {
        if (seltype & pd->atomlist[i].format)
          aret[count ++] = pd->atomlist[i].x_atom;
     }

   *data_ret = aret;
   if (typesize) *typesize = 32 /* urk */;
   if (ttype) *ttype = ECORE_X_ATOM_ATOM;
   if (size_ret) *size_ret = count;
   return EINA_TRUE;
}

static Eina_Bool
_x11_image_converter(char *target EINA_UNUSED, void *data EINA_UNUSED, int size EINA_UNUSED, void **data_ret EINA_UNUSED, int *size_ret EINA_UNUSED, Ecore_X_Atom *ttype EINA_UNUSED, int *typesize EINA_UNUSED)
{
   sel_debug("Image converter called");
   return EINA_TRUE;
}

static Eina_Bool
_x11_vcard_send(char *target EINA_UNUSED, void *data EINA_UNUSED, int size EINA_UNUSED, void **data_ret, int *size_ret, Ecore_X_Atom *ttype EINA_UNUSED, int *typesize EINA_UNUSED)
{
   X11_Cnp_Selection *sel;

   sel_debug("Vcard send called");
   Efl_Selection_Manager_Data *pd = *(Efl_Selection_Manager_Data **)data;
   //sel = _x11_selections + *((int *)data);
   sel = &pd->sellist[pd->active_type];
   if (data_ret) *data_ret = strdup(sel->selbuf);
   if (size_ret) *size_ret = strlen(sel->selbuf);
   return EINA_TRUE;
}

static Eina_Bool
_x11_text_converter(char *target, void *data, int size, void **data_ret, int *size_ret, Ecore_X_Atom *ttype, int *typesize)
{
   X11_Cnp_Selection *sel;
   Efl_Selection_Manager_Data *pd = *(Efl_Selection_Manager_Data **)data;

   sel_debug("text converter");
   if (_get_selection_type(data) == EFL_SELECTION_FORMAT_NONE)
     {
        sel_debug("none");
        if (data_ret)
          {
             *data_ret = malloc(size * sizeof(char) + 1);
             if (!*data_ret) return EINA_FALSE;
             memcpy(*data_ret, data, size);
             ((char**)(data_ret))[0][size] = 0;
          }
        if (size_ret) *size_ret = size;
        return EINA_TRUE;
     }
   //sel = _x11_selections + *((int *)data);

   sel = &pd->sellist[pd->active_type];

   if ((sel->format & EFL_SELECTION_FORMAT_MARKUP) ||
       (sel->format & EFL_SELECTION_FORMAT_HTML))
     {
        *data_ret = _elm_util_mkup_to_text(sel->selbuf);
        if (size_ret && *data_ret) *size_ret = strlen(*data_ret);
        sel_debug("markup or html");
     }
   else if (sel->format & EFL_SELECTION_FORMAT_TEXT)
     {
        ecore_x_selection_converter_text(target, sel->selbuf,
                                         strlen(sel->selbuf),
                                         data_ret, size_ret,
                                         ttype, typesize);
        sel_debug("text");
     }
   else if (sel->format & EFL_SELECTION_FORMAT_IMAGE)
     {
        //FIXME: remove photocam...
        sel_debug("Image %s", evas_object_type_get(sel->widget));
        sel_debug("Elm type: %s", elm_object_widget_type_get(sel->widget));
        evas_object_image_file_get(elm_photocam_internal_image_get(sel->widget),
                                   (const char **)data_ret, NULL);
        if (!*data_ret) *data_ret = strdup("No file");
        else *data_ret = strdup(*data_ret);

        if (!*data_ret)
          {
             ERR("Failed to allocate memory!");
             *size_ret = 0;
             return EINA_FALSE;
          }

        *size_ret = strlen(*data_ret);
     }
   return EINA_TRUE;
}

#ifdef HAVE_ELEMENTARY_X
static void
_x11_efl_sel_manager_selection_set(Eo *obj, Efl_Selection_Manager_Data *pd,
                                   Efl_Selection_Type type, Efl_Selection_Format format,
                                   const void *buf, int len, Efl_Input_Device *seat)
{
   Ecore_X_Window xwin = _x11_xwin_get(obj);

   pd->active_type = type;
   sel_debug("pd active_type: %d, active_format: %d", pd->active_type, pd->active_format);
   pd->active_format = format;
   if (pd->sellist[type].selbuf)
     {
        free(pd->sellist[type].selbuf);
     }
   pd->sellist[type].selbuf = malloc(len);
   if (!pd->sellist[type].selbuf)
     {
        ERR("failed to allocate buf");
        return;
     }
   pd->sellist[type].selbuf = memcpy(pd->sellist[type].selbuf, buf, len);
   pd->sellist[type].len = len;
   pd->sellist[type].format = format;

   //set selection
   pd->sellist[type].set(xwin, &pd, sizeof(&pd));
   sel_debug("data: %p (%ld)", &pd, sizeof(&pd));
}
#endif

EOLIAN static void
_efl_selection_manager_selection_set(Eo *obj, Efl_Selection_Manager_Data *pd,
                                     Efl_Object *owner, Efl_Selection_Type type,
                                     Efl_Selection_Format format,
                                     const void *buf, int len, Efl_Input_Device *seat)
{
   ERR("In");
   if (type > EFL_SELECTION_TYPE_CLIPBOARD)
     {
        ERR("Not supported format: %d", type);
        return;
     }
   Eina_Bool same_win = EINA_FALSE;

#ifdef HAVE_ELEMENTARY_X
   X11_Cnp_Selection *sel = pd->sellist + type;
   Ecore_X_Window xwin = _x11_xwin_get(owner);
   //support 1 app with multiple window, 1 selection manager
   if (sel->xwin == xwin)
     same_win = EINA_TRUE;
#endif
   //check if owner is changed
   if (sel->owner != NULL &&
       sel->owner != owner && same_win)
     {
        /*//call selection_loss callback: should we include prev owner??
        Efl_Selection_Type *lt = malloc(sizeof(Efl_Selection_Type));
        *lt = pd->loss_type;
        efl_promise_value_set(pd->promise, lt, _selection_loss_data_clear_cb);*/

        efl_event_callback_call(sel->owner, EFL_SELECTION_EVENT_SELECTION_LOSS, NULL);
     }

   sel->owner = owner;
   pd->has_sel = EINA_TRUE;
#ifdef HAVE_ELEMENTARY_X
   sel->xwin = xwin;

   return _x11_efl_sel_manager_selection_set(obj, pd, type, format, buf, len, seat);
#endif
#ifdef HAVE_ELEMENTARY_WL2
#endif
#ifdef HAVE_ELEMENTARY_COCOA
#endif
#ifdef HAVE_ELEMENTARY_WIN32
#endif
}

static void
_x11_efl_sel_manager_selection_get(Eo *obj, Efl_Selection_Manager_Data *pd,
                                   Efl_Selection_Type type, Efl_Selection_Format format, Efl_Input_Device *seat)
{
   Ecore_X_Window xwin = _x11_xwin_get(obj);
   ERR("xwin: %d", xwin);
   //FIXME: use each sel or just pd
   X11_Cnp_Selection *sel = pd->sellist + type;
   sel->request_format = format;
   sel->xwin = xwin;

   if (pd->has_sel)
     {
        if (sel->selbuf &&
            ((format == sel->format) || (xwin == 0)))
          {
             sel_debug("use local data");
             Efl_Selection_Data seldata;

             seldata.data = sel->selbuf;
             seldata.len = sel->len;
             seldata.x = seldata.y = 0;
             seldata.format = sel->format;
             sel->data_func(sel->data_func_data, sel->request_obj, &seldata);
             return;
          }
     }

   ecore_x_selection_primary_request(xwin, ECORE_X_SELECTION_TARGET_TARGETS);
}

EOLIAN static void
_efl_selection_manager_selection_get(Eo *obj, Efl_Selection_Manager_Data *pd,
                                     Efl_Object *owner, Efl_Selection_Type type,
                                     Efl_Selection_Format format,
                                     void *data_func_data, Efl_Selection_Data_Ready data_func, Eina_Free_Cb data_func_free_cb,
                                     Efl_Input_Device *seat)
{
   ERR("In");
   X11_Cnp_Selection *sel = pd->sellist + type;

   sel->request_obj = obj;
   sel->data_func_data = data_func_data;
   sel->data_func = data_func;
   sel->data_func_free_cb = data_func_free_cb;

   _x11_efl_sel_manager_selection_get(obj, pd, type, format, seat);
}


EOLIAN static void
_efl_selection_manager_selection_clear(Eo *obj, Efl_Selection_Manager_Data *pd,
                                       Efl_Object *owner, Efl_Selection_Type type, Efl_Input_Device *seat)
{
   ERR("In");
   Eina_Bool local = EINA_FALSE;

   if (type > EFL_SELECTION_TYPE_CLIPBOARD)
     {
        ERR("Not supported type: %d", type);
        return;
     }
   X11_Cnp_Selection *sel = pd->sellist + type;
   if (sel->owner != owner)
     {
        return;
     }
   pd->sellist[type].len = 0;
   if (pd->sellist[type].selbuf)
     {
        free(pd->sellist[type].selbuf);
        pd->sellist[type].selbuf = NULL;
     }
#ifdef HAVE_ELEMENTARY_X
   if (sel->xwin != 0)
     local = EINA_TRUE;
#endif
   if (!local)
     {
        pd->sellist[type].clear();
     }
   else
     {
        efl_event_callback_call(sel->owner, EFL_SELECTION_EVENT_SELECTION_LOSS, NULL);
        pd->sellist[type].owner = NULL;
     }
}


/*
EOLIAN static Efl_Future *
_efl_selection_manager_selection_loss_feedback(Eo *obj, Efl_Selection_Manager_Data *pd,
                                               Efl_Object *owner, Efl_Selection_Type type)
{
   ERR("In");
   Efl_Promise *p;
   Eo *loop = efl_loop_get(obj);

   pd->promise = NULL;
   p = efl_add(EFL_PROMISE_CLASS, loop);
   if (!p) return NULL;
   pd->promise = p;
   pd->loss_type = type;

   return efl_promise_future_get(p);
}*/



static Efl_Object *
_efl_selection_manager_efl_object_constructor(Eo *obj, Efl_Selection_Manager_Data *pd)
{
   obj = efl_constructor(efl_super(obj, MY_CLASS));

   pd->atomlist = calloc(1, SELECTION_N_ATOMS * sizeof(Efl_Sel_Manager_Atom));
   if (!pd->atomlist)
     {
        ERR("failed to allocate atomlist");
        return NULL;
     }
   pd->atomlist[SELECTION_ATOM_TARGETS].name = "TARGETS";
   pd->atomlist[SELECTION_ATOM_TARGETS].format = EFL_SELECTION_FORMAT_TARGETS;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_TARGETS].x_converter = _x11_targets_converter;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_TARGETS].wl_converter = _wl_targets_converter;
#endif
   pd->atomlist[SELECTION_ATOM_ATOM].name = "ATOM"; // for opera browser
   pd->atomlist[SELECTION_ATOM_ATOM].format = EFL_SELECTION_FORMAT_TARGETS;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_ATOM].x_converter = _x11_targets_converter;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_ATOM].wl_converter = _wl_targets_converter;
#endif

   pd->atomlist[SELECTION_ATOM_XELM].name = "application/x-elementary-markup";
   pd->atomlist[SELECTION_ATOM_XELM].format = EFL_SELECTION_FORMAT_MARKUP;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_XELM].x_converter = _x11_general_converter;
   pd->atomlist[SELECTION_ATOM_XELM].x_data_preparer = _x11_data_preparer_markup;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_XELM].wl_converter = _wl_general_converter;
   pd->atomlist[SELECTION_ATOM_XELM].wl_data_preparer = _wl_data_preparer_markup;
#endif

   pd->atomlist[SELECTION_ATOM_TEXT_URILIST].name = "text/uri-list";
   pd->atomlist[SELECTION_ATOM_TEXT_URILIST].format = EFL_SELECTION_FORMAT_IMAGE;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_TEXT_URILIST].x_converter = _x11_general_converter;
   pd->atomlist[SELECTION_ATOM_TEXT_URILIST].x_data_preparer = _x11_data_preparer_uri;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_TEXT_URILIST].wl_converter = _wl_general_converter;
   pd->atomlist[SELECTION_ATOM_TEXT_URILIST].wl_data_preparer = _wl_data_preparer_uri;
#endif

   pd->atomlist[SELECTION_ATOM_TEXT_X_VCARD].name = "text/x-vcard";
   pd->atomlist[SELECTION_ATOM_TEXT_X_VCARD].format = EFL_SELECTION_FORMAT_VCARD;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_TEXT_X_VCARD].x_converter = _x11_vcard_send;
   pd->atomlist[SELECTION_ATOM_TEXT_X_VCARD].x_data_preparer = _x11_data_preparer_vcard;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_TEXT_X_VCARD].wl_data_preparer = _wl_data_preparer_vcard;
#endif

   pd->atomlist[SELECTION_ATOM_IMAGE_PNG].name = "image/png";
   pd->atomlist[SELECTION_ATOM_IMAGE_PNG].format = EFL_SELECTION_FORMAT_IMAGE;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_IMAGE_PNG].x_converter = _x11_image_converter;
   pd->atomlist[SELECTION_ATOM_IMAGE_PNG].x_data_preparer = _x11_data_preparer_image;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_IMAGE_PNG].wl_data_preparer = _wl_data_preparer_image;
#endif

   pd->atomlist[SELECTION_ATOM_IMAGE_JPEG].name = "image/jpeg";
   pd->atomlist[SELECTION_ATOM_IMAGE_JPEG].format = EFL_SELECTION_FORMAT_IMAGE;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_IMAGE_JPEG].x_converter = _x11_image_converter;
   pd->atomlist[SELECTION_ATOM_IMAGE_JPEG].x_data_preparer = _x11_data_preparer_image;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_IMAGE_JPEG].wl_data_preparer = _wl_data_preparer_image;
#endif

   pd->atomlist[SELECTION_ATOM_IMAGE_BMP].name = "image/x-ms-bmp";
   pd->atomlist[SELECTION_ATOM_IMAGE_BMP].format = EFL_SELECTION_FORMAT_IMAGE;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_IMAGE_BMP].x_converter = _x11_image_converter;
   pd->atomlist[SELECTION_ATOM_IMAGE_BMP].x_data_preparer = _x11_data_preparer_image;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_IMAGE_BMP].wl_data_preparer = _wl_data_preparer_image;
#endif

   pd->atomlist[SELECTION_ATOM_IMAGE_GIF].name = "image/gif";
   pd->atomlist[SELECTION_ATOM_IMAGE_GIF].format = EFL_SELECTION_FORMAT_IMAGE;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_IMAGE_GIF].x_converter = _x11_image_converter;
   pd->atomlist[SELECTION_ATOM_IMAGE_GIF].x_data_preparer = _x11_data_preparer_image;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_IMAGE_GIF].wl_data_preparer = _wl_data_preparer_image;
#endif

   pd->atomlist[SELECTION_ATOM_IMAGE_TIFF].name = "image/tiff";
   pd->atomlist[SELECTION_ATOM_IMAGE_TIFF].format = EFL_SELECTION_FORMAT_IMAGE;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_IMAGE_TIFF].x_converter = _x11_image_converter;
   pd->atomlist[SELECTION_ATOM_IMAGE_TIFF].x_data_preparer = _x11_data_preparer_image;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_IMAGE_TIFF].wl_data_preparer = _wl_data_preparer_image;
#endif

   pd->atomlist[SELECTION_ATOM_IMAGE_SVG].name = "image/svg+xml";
   pd->atomlist[SELECTION_ATOM_IMAGE_SVG].format = EFL_SELECTION_FORMAT_IMAGE;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_IMAGE_SVG].x_converter = _x11_image_converter;
   pd->atomlist[SELECTION_ATOM_IMAGE_SVG].x_data_preparer = _x11_data_preparer_image;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_IMAGE_SVG].wl_data_preparer = _wl_data_preparer_image;
#endif

   pd->atomlist[SELECTION_ATOM_IMAGE_XPM].name = "image/x-xpixmap";
   pd->atomlist[SELECTION_ATOM_IMAGE_XPM].format = EFL_SELECTION_FORMAT_IMAGE;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_IMAGE_XPM].x_converter = _x11_image_converter;
   pd->atomlist[SELECTION_ATOM_IMAGE_XPM].x_data_preparer = _x11_data_preparer_image;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_IMAGE_XPM].wl_data_preparer = _wl_data_preparer_image;
#endif

   pd->atomlist[SELECTION_ATOM_IMAGE_TGA].name = "image/x-tga";
   pd->atomlist[SELECTION_ATOM_IMAGE_TGA].format = EFL_SELECTION_FORMAT_IMAGE;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_IMAGE_TGA].x_converter = _x11_image_converter;
   pd->atomlist[SELECTION_ATOM_IMAGE_TGA].x_data_preparer = _x11_data_preparer_image;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_IMAGE_TGA].wl_data_preparer = _wl_data_preparer_image;
#endif

   pd->atomlist[SELECTION_ATOM_IMAGE_PPM].name = "image/x-portable-pixmap";
   pd->atomlist[SELECTION_ATOM_IMAGE_PPM].format = EFL_SELECTION_FORMAT_IMAGE;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_IMAGE_PPM].x_converter = _x11_image_converter;
   pd->atomlist[SELECTION_ATOM_IMAGE_PPM].x_data_preparer = _x11_data_preparer_image;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_IMAGE_PPM].wl_data_preparer = _wl_data_preparer_image;
#endif

   pd->atomlist[SELECTION_ATOM_UTF8STRING].name = "UTF8_STRING";
   pd->atomlist[SELECTION_ATOM_UTF8STRING].format = EFL_SELECTION_FORMAT_TEXT | EFL_SELECTION_FORMAT_MARKUP | EFL_SELECTION_FORMAT_HTML;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_UTF8STRING].x_converter = _x11_text_converter;
   pd->atomlist[SELECTION_ATOM_UTF8STRING].x_data_preparer = _x11_data_preparer_text;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_UTF8STRING].wl_converter = _wl_text_converter;
   pd->atomlist[SELECTION_ATOM_UTF8STRING].wl_data_preparer = _wl_data_preparer_text,
#endif

   pd->atomlist[SELECTION_ATOM_STRING].name = "STRING";
   pd->atomlist[SELECTION_ATOM_STRING].format = EFL_SELECTION_FORMAT_TEXT | EFL_SELECTION_FORMAT_MARKUP | EFL_SELECTION_FORMAT_HTML;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_STRING].x_converter = _x11_text_converter;
   pd->atomlist[SELECTION_ATOM_STRING].x_data_preparer = _x11_data_preparer_text;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_STRING].wl_converter = _wl_text_converter;
   pd->atomlist[SELECTION_ATOM_STRING].wl_data_preparer = _wl_data_preparer_text;
#endif

   pd->atomlist[SELECTION_ATOM_COMPOUND_TEXT].name = "COMPOUND_TEXT";
   pd->atomlist[SELECTION_ATOM_COMPOUND_TEXT].format = EFL_SELECTION_FORMAT_TEXT | EFL_SELECTION_FORMAT_MARKUP | EFL_SELECTION_FORMAT_HTML;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_COMPOUND_TEXT].x_converter = _x11_text_converter;
   pd->atomlist[SELECTION_ATOM_COMPOUND_TEXT].x_data_preparer = _x11_data_preparer_text;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_COMPOUND_TEXT].wl_converter = _wl_text_converter;
   pd->atomlist[SELECTION_ATOM_COMPOUND_TEXT].wl_data_preparer = _wl_data_preparer_text;
#endif

   pd->atomlist[SELECTION_ATOM_TEXT].name = "TEXT";
   pd->atomlist[SELECTION_ATOM_TEXT].format = EFL_SELECTION_FORMAT_TEXT | EFL_SELECTION_FORMAT_MARKUP | EFL_SELECTION_FORMAT_HTML;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_TEXT].x_converter = _x11_text_converter;
   pd->atomlist[SELECTION_ATOM_TEXT].x_data_preparer = _x11_data_preparer_text;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_TEXT].wl_converter = _wl_text_converter;
   pd->atomlist[SELECTION_ATOM_TEXT].wl_data_preparer = _wl_data_preparer_text;
#endif

   pd->atomlist[SELECTION_ATOM_TEXT_PLAIN_UTF8].name = "text/plain;charset=utf-8";
   pd->atomlist[SELECTION_ATOM_TEXT_PLAIN_UTF8].format = EFL_SELECTION_FORMAT_TEXT | EFL_SELECTION_FORMAT_MARKUP | EFL_SELECTION_FORMAT_HTML;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_TEXT_PLAIN_UTF8].x_converter = _x11_text_converter;
   pd->atomlist[SELECTION_ATOM_TEXT_PLAIN_UTF8].x_data_preparer = _x11_data_preparer_text;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_TEXT_PLAIN_UTF8].wl_converter = _wl_text_converter;
   pd->atomlist[SELECTION_ATOM_TEXT_PLAIN_UTF8].wl_data_preparer = _wl_data_preparer_text;
#endif

   pd->atomlist[SELECTION_ATOM_TEXT_PLAIN].name = "text/plain";
   pd->atomlist[SELECTION_ATOM_TEXT_PLAIN].format = EFL_SELECTION_FORMAT_TEXT | EFL_SELECTION_FORMAT_MARKUP | EFL_SELECTION_FORMAT_HTML;
#ifdef HAVE_ELEMENTARY_X
   pd->atomlist[SELECTION_ATOM_TEXT_PLAIN].x_converter = _x11_text_converter;
   pd->atomlist[SELECTION_ATOM_TEXT_PLAIN].x_data_preparer = _x11_data_preparer_text;
#endif
#ifdef HAVE_ELEMENTARY_WL2
   pd->atomlist[SELECTION_ATOM_TEXT_PLAIN].wl_converter = _wl_text_converter;
   pd->atomlist[SELECTION_ATOM_TEXT_PLAIN].wl_data_preparer = _wl_data_preparer_text;
#endif


#ifdef HAVE_ELEMENTARY_X
   int i;
   for (i = 0; i < SELECTION_N_ATOMS; i++)
      {
         pd->atomlist[i].x_atom = ecore_x_atom_get(pd->atomlist[i].name);
         ecore_x_selection_converter_atom_add
            (pd->atomlist[i].x_atom, pd->atomlist[i].x_converter);
      }
#endif

   pd->sellist = calloc(1, (EFL_SELECTION_TYPE_CLIPBOARD + 1) * sizeof(X11_Cnp_Selection));
   if (!pd->sellist)
     {
        ERR("cannot allocate sellist");
        return NULL;
     }
   pd->sellist[EFL_SELECTION_TYPE_PRIMARY].debug = "Primary";
   pd->sellist[EFL_SELECTION_TYPE_PRIMARY].ecore_sel = ECORE_X_SELECTION_PRIMARY;
   pd->sellist[EFL_SELECTION_TYPE_PRIMARY].set = ecore_x_selection_primary_set;
   pd->sellist[EFL_SELECTION_TYPE_PRIMARY].clear = ecore_x_selection_primary_clear;
   pd->sellist[EFL_SELECTION_TYPE_PRIMARY].request = ecore_x_selection_primary_request;

   pd->sellist[EFL_SELECTION_TYPE_SECONDARY].debug = "Secondary";
   pd->sellist[EFL_SELECTION_TYPE_SECONDARY].ecore_sel = ECORE_X_SELECTION_SECONDARY;
   pd->sellist[EFL_SELECTION_TYPE_SECONDARY].set = ecore_x_selection_secondary_set;
   pd->sellist[EFL_SELECTION_TYPE_SECONDARY].clear = ecore_x_selection_secondary_clear;
   pd->sellist[EFL_SELECTION_TYPE_SECONDARY].request = ecore_x_selection_secondary_request;

   pd->sellist[EFL_SELECTION_TYPE_DND].debug = "DnD";
   pd->sellist[EFL_SELECTION_TYPE_DND].ecore_sel = ECORE_X_SELECTION_PRIMARY;
   pd->sellist[EFL_SELECTION_TYPE_DND].request = ecore_x_selection_xdnd_request;

   pd->sellist[EFL_SELECTION_TYPE_CLIPBOARD].debug = "Clipboard";
   pd->sellist[EFL_SELECTION_TYPE_CLIPBOARD].ecore_sel = ECORE_X_SELECTION_CLIPBOARD;
   pd->sellist[EFL_SELECTION_TYPE_CLIPBOARD].set = ecore_x_selection_clipboard_set;
   pd->sellist[EFL_SELECTION_TYPE_CLIPBOARD].clear = ecore_x_selection_clipboard_clear;
   pd->sellist[EFL_SELECTION_TYPE_CLIPBOARD].request = ecore_x_selection_clipboard_request;

   pd->savedtypes = calloc(1, sizeof(Saved_Type));

   pd->notify_handler = ecore_event_handler_add(ECORE_X_EVENT_SELECTION_NOTIFY,
                                                _efl_sel_manager_x11_selection_notify, pd);
   pd->clear_handler = ecore_event_handler_add(ECORE_X_EVENT_SELECTION_CLEAR,
                                            _x11_selection_clear, pd);
   return obj;
}


static void
_efl_selection_manager_efl_object_destructor(Eo *obj, Efl_Selection_Manager_Data *pd)
{
   ecore_event_handler_del(pd->notify_handler);
   ecore_event_handler_del(pd->clear_handler);
   free(pd->atomlist);
   free(pd->savedtypes);

   efl_destructor(efl_super(obj, MY_CLASS));
}



#include "efl_selection_manager.eo.c"
