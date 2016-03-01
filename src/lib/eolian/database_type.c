#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <Eina.h>
#include "eo_lexer.h"

void
database_type_del(Eolian_Type *tp)
{
   if (!tp) return;
   const char *sp;
   Eolian_Type *stp;
   if (tp->base.file) eina_stringshare_del(tp->base.file);
   if (tp->subtypes) EINA_LIST_FREE(tp->subtypes, stp)
     database_type_del(stp);
   database_type_del(tp->base_type);
   if (tp->name) eina_stringshare_del(tp->name);
   if (tp->full_name) eina_stringshare_del(tp->full_name);
   if (tp->fields) eina_hash_free(tp->fields);
   if (tp->field_list) eina_list_free(tp->field_list);
   if (tp->namespaces) EINA_LIST_FREE(tp->namespaces, sp)
     eina_stringshare_del(sp);
   if (tp->legacy) eina_stringshare_del(tp->legacy);
   if (tp->freefunc) eina_stringshare_del(tp->freefunc);
   database_doc_del(tp->doc);
   database_typedecl_del(tp->decl);
   free(tp);
}

void
database_typedecl_del(Eolian_Typedecl *tp)
{
   /* TODO: own storage for typedecls for several fields */
   if (!tp) return;
   const char *sp;
   if (tp->base.file) eina_stringshare_del(tp->base.file);
   /*database_type_del(tp->base_type);*/
   if (tp->name) eina_stringshare_del(tp->name);
   if (tp->full_name) eina_stringshare_del(tp->full_name);
   /*if (tp->fields) eina_hash_free(tp->fields);
   if (tp->field_list) eina_list_free(tp->field_list);*/
   if (tp->namespaces) EINA_LIST_FREE(tp->namespaces, sp)
     eina_stringshare_del(sp);
   if (tp->legacy) eina_stringshare_del(tp->legacy);
   if (tp->freefunc) eina_stringshare_del(tp->freefunc);
   /*database_doc_del(tp->doc);*/
   free(tp);
}

void
database_typedef_del(Eolian_Type *tp)
{
   if (!tp) return;
   Eolian_Type *btp = tp->base_type;
   /* prevent deletion of named structs/enums as they're deleted later on */
   if (btp)
     {
        if (btp->type == EOLIAN_TYPE_ENUM)
          tp->base_type = NULL;
        else if ((btp->type == EOLIAN_TYPE_STRUCT
               || btp->type == EOLIAN_TYPE_STRUCT_OPAQUE) && btp->name)
          tp->base_type = NULL;
     }
   database_type_del(tp);
}

static Eolian_Typedecl *
_typedecl_add(Eolian_Type *type)
{
   const char *nm;
   Eina_List *l;

   Eolian_Typedecl *ret = calloc(1, sizeof(Eolian_Typedecl));
   ret->base.file = eina_stringshare_ref(type->base.file);
   ret->base.line = type->base.line;
   ret->base.column = type->base.column;
   ret->base_type = type->base_type;
   ret->name = eina_stringshare_ref(type->name);
   ret->full_name = eina_stringshare_ref(type->full_name);
   if (type->namespaces) EINA_LIST_FOREACH(type->namespaces, l, nm)
     ret->namespaces = eina_list_append(ret->namespaces, eina_stringshare_ref(nm));
   ret->fields = type->fields;
   ret->field_list = type->field_list;
   ret->doc = type->doc;
   ret->legacy = eina_stringshare_ref(type->legacy);
   ret->freefunc = eina_stringshare_ref(type->freefunc);
   ret->is_extern = type->is_extern;
   ret->parent = type;

   return ret;
}

void
database_type_add(Eolian_Type *def)
{
   def->decl = _typedecl_add(def);
   def->decl->type = EOLIAN_TYPEDECL_ALIAS;
   eina_hash_set(_aliases, def->full_name, def);
   eina_hash_set(_aliasesf, def->base.file, eina_list_append
                ((Eina_List*)eina_hash_find(_aliasesf, def->base.file), def->decl));
   database_decl_add(def->full_name, EOLIAN_DECL_ALIAS, def->base.file, def->decl);
}

void
database_struct_add(Eolian_Type *tp)
{
   tp->decl = _typedecl_add(tp);
   tp->decl->type = (tp->type == EOLIAN_TYPE_STRUCT_OPAQUE) ? EOLIAN_TYPEDECL_STRUCT_OPAQUE
                                                            : EOLIAN_TYPEDECL_STRUCT;
   eina_hash_set(_structs, tp->full_name, tp);
   eina_hash_set(_structsf, tp->base.file, eina_list_append
                ((Eina_List*)eina_hash_find(_structsf, tp->base.file), tp->decl));
   database_decl_add(tp->full_name, EOLIAN_DECL_STRUCT, tp->base.file, tp->decl);
}

void
database_enum_add(Eolian_Type *tp)
{
   tp->decl = _typedecl_add(tp);
   tp->decl->type = EOLIAN_TYPEDECL_ENUM;
   eina_hash_set(_enums, tp->full_name, tp);
   eina_hash_set(_enumsf, tp->base.file, eina_list_append
                ((Eina_List*)eina_hash_find(_enumsf, tp->base.file), tp->decl));
   database_decl_add(tp->full_name, EOLIAN_DECL_ENUM, tp->base.file, tp->decl);
}

void
database_type_to_str(const Eolian_Type *tp, Eina_Strbuf *buf, const char *name)
{
   if ((tp->type == EOLIAN_TYPE_REGULAR
     || tp->type == EOLIAN_TYPE_COMPLEX
     || tp->type == EOLIAN_TYPE_VOID
     || tp->type == EOLIAN_TYPE_CLASS)
     && tp->is_const)
     {
        eina_strbuf_append(buf, "const ");
     }
   if (tp->type == EOLIAN_TYPE_REGULAR
    || tp->type == EOLIAN_TYPE_COMPLEX
    || tp->type == EOLIAN_TYPE_CLASS)
     {
        Eina_List *l;
        const char *sp;
        EINA_LIST_FOREACH(tp->namespaces, l, sp)
          {
             eina_strbuf_append(buf, sp);
             eina_strbuf_append_char(buf, '_');
          }
        int kw = eo_lexer_keyword_str_to_id(tp->name);
        if (kw && eo_lexer_is_type_keyword(kw))
          eina_strbuf_append(buf, eo_lexer_get_c_type(kw));
        else
          eina_strbuf_append(buf, tp->name);
     }
   else if (tp->type == EOLIAN_TYPE_VOID)
     eina_strbuf_append(buf, "void");
   else
     {
        Eolian_Type *btp = tp->base_type;
        database_type_to_str(tp->base_type, buf, NULL);
        if (btp->type != EOLIAN_TYPE_POINTER || btp->is_const)
           eina_strbuf_append_char(buf, ' ');
        eina_strbuf_append_char(buf, '*');
        if (tp->is_const) eina_strbuf_append(buf, " const");
     }
   if (name)
     {
        if (tp->type != EOLIAN_TYPE_POINTER)
          eina_strbuf_append_char(buf, ' ');
        eina_strbuf_append(buf, name);
     }
}

static void
_stype_to_str(const Eolian_Typedecl *tp, Eina_Strbuf *buf, const char *name)
{
   Eolian_Struct_Type_Field *sf;
   Eina_List *l;
   eina_strbuf_append(buf, "struct ");
   if (tp->name)
     {
        Eina_List *m;
        const char *sp;
        EINA_LIST_FOREACH(tp->namespaces, m, sp)
          {
             eina_strbuf_append(buf, sp);
             eina_strbuf_append_char(buf, '_');
          }
        eina_strbuf_append(buf, tp->name);
        eina_strbuf_append_char(buf, ' ');
     }
   if (tp->type == EOLIAN_TYPEDECL_STRUCT_OPAQUE)
     goto append_name;
   eina_strbuf_append(buf, "{ ");
   EINA_LIST_FOREACH(tp->field_list, l, sf)
     {
        database_type_to_str(sf->type, buf, sf->name);
        eina_strbuf_append(buf, "; ");
     }
   eina_strbuf_append(buf, "}");
append_name:
   if (name)
     {
        eina_strbuf_append_char(buf, ' ');
        eina_strbuf_append(buf, name);
     }
}

static void
_etype_to_str(const Eolian_Typedecl *tp, Eina_Strbuf *buf, const char *name)
{
   Eolian_Enum_Type_Field *ef;
   Eina_List *l;
   eina_strbuf_append(buf, "enum ");
   if (tp->name)
     {
        Eina_List *m;
        const char *sp;
        EINA_LIST_FOREACH(tp->namespaces, m, sp)
          {
             eina_strbuf_append(buf, sp);
             eina_strbuf_append_char(buf, '_');
          }
        eina_strbuf_append(buf, tp->name);
        eina_strbuf_append_char(buf, ' ');
     }
   eina_strbuf_append(buf, "{ ");
   EINA_LIST_FOREACH(tp->field_list, l, ef)
     {
        eina_strbuf_append(buf, ef->name);
        if (ef->value)
          {
             Eolian_Value val = eolian_expression_eval(ef->value,
                 EOLIAN_MASK_INT);
             const char *ret;
             eina_strbuf_append(buf, " = ");
             ret = eolian_expression_value_to_literal(&val);
             eina_strbuf_append(buf, ret);
             eina_stringshare_del(ret);
          }
        if (l != eina_list_last(tp->field_list))
          eina_strbuf_append(buf, ", ");
     }
   eina_strbuf_append(buf, " }");
   if (name)
     {
        eina_strbuf_append_char(buf, ' ');
        eina_strbuf_append(buf, name);
     }
}

static void
_atype_to_str(const Eolian_Typedecl *tp, Eina_Strbuf *buf)
{
   Eina_Strbuf *fulln = eina_strbuf_new();
   Eina_List *l;
   const char *sp;

   eina_strbuf_append(buf, "typedef ");

   EINA_LIST_FOREACH(tp->namespaces, l, sp)
     {
        eina_strbuf_append(fulln, sp);
        eina_strbuf_append_char(fulln, '_');
     }
   eina_strbuf_append(fulln, tp->name);

   database_type_to_str(tp->base_type, buf, eina_strbuf_string_get(fulln));
   eina_strbuf_free(fulln);
}

void
database_typedecl_to_str(const Eolian_Typedecl *tp, Eina_Strbuf *buf, const char *name)
{
   if (tp->type == EOLIAN_TYPEDECL_ALIAS)
     {
        _atype_to_str(tp, buf);
        return;
     }
   else if (tp->type == EOLIAN_TYPEDECL_STRUCT
         || tp->type == EOLIAN_TYPEDECL_STRUCT_OPAQUE)
     {
        _stype_to_str(tp, buf, name);
        return;
     }
   else if (tp->type == EOLIAN_TYPEDECL_ENUM)
     {
        _etype_to_str(tp, buf, name);
        return;
     }
   else
     return;
   if (name)
     {
        eina_strbuf_append_char(buf, ' ');
        eina_strbuf_append(buf, name);
     }
}
