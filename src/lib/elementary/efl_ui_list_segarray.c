#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#include "efl_ui_list_private.h"
#include "efl_ui_list_segarray.h"

#include <Efl.h>

#include <assert.h>

#undef DBG
#define DBG(...) do { \
    fprintf(stderr, __FILE__ ":" "%d %s ", __LINE__, __PRETTY_FUNCTION__); \
    fprintf(stderr,  __VA_ARGS__);                                     \
    fprintf(stderr, "\n"); fflush(stderr);                              \
  } while(0)

static int _search_lookup_cb(Eina_Rbtree const* rbtree, const void* key, int length EINA_UNUSED, void* data EINA_UNUSED)
{
  Efl_Ui_List_SegArray_Node const* node = (void const*)rbtree;
  int index = *(int*)key;
  DBG("searching for %d", index);
  if(index < node->first)
    {
      DBG("index is less than index");
      return -1;
    }
  else if(index < node->first + node->length)
    {
      DBG("index is within bounds, found it");
      return 0;
    }
  else
    {
      DBG("we're after first %d max %d index %d", node->first, node->max, index);
      return 1;
    }
}

static int _insert_lookup_cb(Eina_Rbtree const* rbtree, const void* key, int length EINA_UNUSED, void* data EINA_UNUSED)
{
  Efl_Ui_List_SegArray_Node const* node = (void const*)rbtree;
  int index = *(int*)key;
  if(index < node->first)
    {
      return -1;
    }
  else if(index < node->first + node->max)
    {
      return 0;
    }
  else
    {
       return 1;
    }
}

static Eina_Rbtree_Direction _rbtree_compare(Eina_Rbtree const* left, Eina_Rbtree const* right, void* data EINA_UNUSED)
{
   Efl_Ui_List_SegArray_Node const *nl = (void const*)left, *nr = (void const*)right;
   return !nl ? EINA_RBTREE_LEFT :
     (
       !nr ? EINA_RBTREE_RIGHT :
       (
        nl->first < nr->first ? EINA_RBTREE_LEFT : EINA_RBTREE_RIGHT
       )
     );
}

static Efl_Ui_List_SegArray_Node*
_alloc_node(Efl_Ui_List_SegArray* segarray, int first, int max)
{
   DBG("alloc'ing and inserting node with first index: %d", first);
  
   Efl_Ui_List_SegArray_Node* node;
   node = calloc(1, sizeof(Efl_Ui_List_SegArray_Node) + max*sizeof(Efl_Ui_List_Item*));
   node->first = first;
   node->max = max;
   segarray->root = eina_rbtree_inline_insert(segarray->root, EINA_RBTREE_GET(node), &_rbtree_compare, NULL);
   segarray->node_count++;
   return node;
}

void efl_ui_list_segarray_setup(Efl_Ui_List_SegArray* segarray, //int member_size,
                                int initial_step_size)
{
   segarray->root = NULL;
   /* segarray->member_size = member_size; */
   segarray->array_initial_size = initial_step_size;
}

static Efl_Ui_List_Item* _create_item(Efl_Model* model, unsigned int index)
{
   Efl_Ui_List_Item* item = calloc(1, sizeof(Efl_Ui_List_Item));
   item->model = model;
   item->index = index;
   return item;
}

void efl_ui_list_segarray_insert_accessor(Efl_Ui_List_SegArray* segarray, int first, Eina_Accessor* accessor)
{
   int i;
   Efl_Model* children;
   Efl_Ui_List_SegArray_Node *first_node = NULL/*, *last_node = NULL*/;
   int array_first = 0;

   if(segarray->root)
     {
        {
           Eina_Iterator* pre_iterator = eina_rbtree_iterator_prefix(segarray->root);
           if(!eina_iterator_next(pre_iterator, (void**)&first_node))
             first_node = NULL;
           else
             array_first = first_node->first;
           eina_iterator_free(pre_iterator);
        }
        /* { */
        /*    Eina_Iterator* post_iterator = eina_rbtree_iterator_postfix(segarray->root); */
        /*    if(!eina_iterator_next(post_iterator, (void**)&last_node)) */
        /*      last_node = NULL; */
        /*    eina_iterator_free(post_iterator); */
        /* } */
     }

   EINA_ACCESSOR_FOREACH(accessor, i, children)
     {
        // if prefix'ing
        if((first + i < array_first) || !efl_ui_list_segarray_count(segarray))
          {
             // count is zero
             DBG("prefixing count: %d", efl_ui_list_segarray_count(segarray));
             // if no first_node
             if(!first_node)
               {
                  first_node = _alloc_node(segarray, i + first, segarray->array_initial_size);
                  first_node->pointers[0] = _create_item(children, first + i);
                  first_node->length++;
                  segarray->count++;
               }
             else
               {
                  DBG("there is a first node");
               }
             /* else if() */
             /*   { */
                  
             /*   } */
          }
        else if(first + i <= array_first + efl_ui_list_segarray_count(segarray))
          {
            Efl_Ui_List_SegArray_Node *node;
            int idx = first + i;

            DBG("insert is in the middle");

            node = (void*)eina_rbtree_inline_lookup(segarray->root, &idx, sizeof(idx), &_insert_lookup_cb, NULL);
            if(node)
              {
                 assert(node->length < node->max);
                 node->pointers[node->length] = _create_item(children, first + i);
                 node->length++;
                 segarray->count++;
              }
            else
              {
                 DBG("no node to add item!");
              }
          }
        /* else // suffix'ing */
        /*   { */
        /*      DBG("suffixing"); */
        /*      assert(!!last_node == !!first_node); */
        /*      if(last_node->max < last_node->length) */
        /*        { */
        /*           last_node->pointers[last_node->length++] = _create_item(children, first + i); */
        /*           ++last_node->length; */
        /*           segarray->count++; */
        /*        } */
        /*   } */
     }
}

int efl_ui_list_segarray_count(Efl_Ui_List_SegArray const* segarray)
{
   return segarray->count;
}

typedef struct _Efl_Ui_List_Segarray_Eina_Accessor
{
   Eina_Accessor vtable;
   Efl_Ui_List_SegArray* segarray;
} Efl_Ui_List_Segarray_Eina_Accessor;

static Eina_Bool
_efl_ui_list_segarray_accessor_get_at(Efl_Ui_List_Segarray_Eina_Accessor* acc,
                                      int idx, void** data)
{
   Efl_Ui_List_SegArray_Node* node;
   node = (void*)eina_rbtree_inline_lookup(acc->segarray->root, &idx, sizeof(idx), &_search_lookup_cb, NULL);
   if(node)
     {
        if(node->first <= idx && node->first + node->length > idx)
          {
             int i = idx - node->first;
             Efl_Ui_List_Item* item = node->pointers[i];
             *data = item;
             return EINA_TRUE;
          }
        else
          {
            DBG("node found is not within bounds first %d length %d idx %d", node->first, node->length, idx);
          }
     }
   else
     DBG("no node found with index %d", idx);
   return EINA_FALSE;
}

static void*
_efl_ui_list_segarray_accessor_get_container(Efl_Ui_List_Segarray_Eina_Accessor* acc EINA_UNUSED)
{
  return NULL;
}

static void
_efl_ui_list_segarray_accessor_free(Efl_Ui_List_Segarray_Eina_Accessor* acc EINA_UNUSED)
{
   free(acc);
}

static void
_efl_ui_list_segarray_accessor_lock(Efl_Ui_List_Segarray_Eina_Accessor* acc EINA_UNUSED)
{
}

static void
_efl_ui_list_segarray_accessor_unlock(Efl_Ui_List_Segarray_Eina_Accessor* acc EINA_UNUSED)
{
}

static Eina_Accessor*
_efl_ui_list_segarray_accessor_clone(Efl_Ui_List_Segarray_Eina_Accessor* acc EINA_UNUSED)
{
   return &acc->vtable;
}

static void
_efl_ui_list_segarray_accessor_setup(Efl_Ui_List_Segarray_Eina_Accessor* acc, Efl_Ui_List_SegArray* segarray)
{
   EINA_MAGIC_SET(&acc->vtable, EINA_MAGIC_ACCESSOR);
   acc->vtable.version = EINA_ACCESSOR_VERSION;
   acc->vtable.get_at = FUNC_ACCESSOR_GET_AT(_efl_ui_list_segarray_accessor_get_at);
   acc->vtable.get_container = FUNC_ACCESSOR_GET_CONTAINER(_efl_ui_list_segarray_accessor_get_container);
   acc->vtable.free = FUNC_ACCESSOR_FREE(_efl_ui_list_segarray_accessor_free);
   acc->vtable.lock = FUNC_ACCESSOR_LOCK(_efl_ui_list_segarray_accessor_lock);
   acc->vtable.unlock = FUNC_ACCESSOR_LOCK(_efl_ui_list_segarray_accessor_unlock);
   acc->vtable.clone = FUNC_ACCESSOR_CLONE(_efl_ui_list_segarray_accessor_clone);
   acc->segarray = segarray;
}

Eina_Accessor* efl_ui_list_segarray_accessor_get(Efl_Ui_List_SegArray* segarray)
{
   Efl_Ui_List_Segarray_Eina_Accessor* acc = calloc(1, sizeof(Efl_Ui_List_Segarray_Eina_Accessor));
   _efl_ui_list_segarray_accessor_setup(acc, segarray);
   return &acc->vtable;
}

/* static void */
/* _insert(int pos, Efl_Model* model) */
/* { */
/*    Efl_Ui_List_Item* item = malloc(sizeof(Efl_Ui_List_Item)); */
/*    item->model = model; */

   
/* } */

/* inline static Efl_Ui_List_Item** */
/* _back_empty_get_or_null(Efl_Ui_List_SegArray* array) */
/* { */
/*    /\* void* inlist_last = eina_rbtree_last(array->list); *\/ */
/*    /\* Efl_Ui_List_SegArray_Node* node = inlist_last; *\/ */
/*    /\* if(node && node->max == node->length) *\/ */
/*    /\*   return &node->pointers[node->length++]; *\/ */
/*    /\* else *\/ */
/*      return NULL; */
/* } */

/* inline static Efl_Ui_List_Item** */
/* _alloc_back_and_return_last(Efl_Ui_List_SegArray* array) */
/* { */
/*    Efl_Ui_List_SegArray_Node* new_node = calloc(1, sizeof(Efl_Ui_List_SegArray_Node) + array->array_initial_size); */
/*    new_node->length = 0; */
/*    new_node->max = array->array_initial_size; */

/*    array->list = eina_inlist_append(array->list, EINA_INLIST_GET(new_node)); */
/*    return &new_node->pointers[0]; */
/* } */


/* /\* void efl_ui_list_segarray_insert_at_index(Efl_Ui_List_SegArray* array EINA_UNUSED, int index EINA_UNUSED, *\/ */
/* /\*                                           Efl_Ui_List_Item* item EINA_UNUSED) *\/ */
/* /\* { *\/ */

/* /\* } *\/ */

/* void efl_ui_list_segarray_append(Efl_Ui_List_SegArray* array, Efl_Ui_List_Item* item) */
/* { */
/*    Efl_Ui_List_Item** new_item = _back_empty_get_or_null(array); */
/*    if(!new_item) */
/*      new_item = _alloc_back_and_return_last(array); */

/*    *new_item = item; */
/* } */

/* /\* void efl_ui_list_segarray_insert_at(Efl_Ui_List_SegArray* array, int position, Efl_Ui_List_Item* item) *\/ */
/* /\* { *\/ */
   
/* /\* } *\/ */


