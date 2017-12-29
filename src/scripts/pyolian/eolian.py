#!/usr/bin/env python3
# encoding: utf-8
"""
Pyolian: ctypes bindings for the eolian library.

This file (along with eolian_lib.py) implement full pythonic eolian bindings.

Those bindings are designed to work at runtime without any prepare/compilation
steps required. The only requirement is that we can find the eolian.so/dll
somwhere built in the source tree, look at eolian_lib.py to see the searchs
performed.

You can directly use the python API provided here if you need direct access
to eolian, or we suggest to look at the template-based generator.py if you just
need to generate some sort of text files out of the eolian database.

To use this library from outside this directory, you need to hack sys.path in
a way that this folder will be available on PYTHON_PATH, fe:

  pyolian_path = os.path.join(EFL_ROOT_PATH, 'src', 'scripts')
  sys.path.insert(0, pyolian_path)
  from pyolian import eolian
  from pyolian.generator import Template

"""
from enum import IntEnum
from ctypes import cast, byref, c_char_p, c_void_p

try:
    from .eolian_lib import lib
except ImportError:
    from eolian_lib import lib
    


###  Eolian Enums  ############################################################

class Eolian_Function_Type(IntEnum):
    UNRESOLVED = 0
    PROPERTY = 1
    PROP_SET = 2
    PROP_GET = 3
    METHOD = 4
    FUNCTION_POINTER = 5

class Eolian_Parameter_Dir(IntEnum):
    UNKNOWN = 0
    IN = 1
    OUT = 2
    INOUT = 3

class Eolian_Class_Type(IntEnum):
    UNKNOWN_TYPE = 0
    REGULAR = 1
    ABSTRACT = 2
    MIXIN = 3
    INTERFACE = 4

class Eolian_Object_Scope(IntEnum):
    UNKNOWN = 0
    PUBLIC = 1
    PRIVATE = 2
    PROTECTED = 3

class Eolian_Typedecl_Type(IntEnum):
    UNKNOWN = 0
    STRUCT = 1
    STRUCT_OPAQUE = 2
    ENUM = 3
    ALIAS = 4
    FUNCTION_POINTER = 5

class Eolian_Type_Type(IntEnum):
    UNKNOWN_TYPE = 0
    VOID = 1
    REGULAR = 2
    CLASS = 3
    UNDEFINED = 4

class Eolian_Type_Builtin_Type(IntEnum):
    INVALID = 0
    BYTE = 1
    UBYTE = 2
    CHAR = 3
    SHORT = 4
    USHORT = 5
    INT = 6
    UINT = 7
    LONG = 8
    ULONG = 9
    LLONG = 10
    ULLONG = 11

    INT8 = 12
    UINT8 = 13
    INT16 = 14
    UINT16 = 15
    INT32 = 16
    UINT32 = 17
    INT64 = 18
    UINT64 = 19
    INT128 = 20
    UINT128 = 21

    SIZE = 22
    SSIZE = 23
    INTPTR = 24
    UINTPTR = 25
    PTRDIFF = 26

    TIME = 27

    FLOAT = 28
    DOUBLE = 29

    BOOL = 30
    VOID = 31

    ACCESSOR = 32
    ARRAY = 33
    ITERATOR = 34
    HASH = 35
    LIST = 36
    INARRAY = 37
    INLIST = 38

    FUTURE = 39

    ANY_VALUE = 40
    ANY_VALUE_PTR = 41

    MSTRING = 42
    STRING = 43
    STRINGSHARE = 44

    VOID_PTR = 45
    FREE_CB = 46

class Eolian_C_Type_Type(IntEnum):
    DEFAULT = 0
    PARAM = 1
    RETURN = 2

class Eolian_Expression_Type(IntEnum):
    UNKNOWN = 0
    INT = 1
    UINT = 2
    LONG = 3
    ULONG = 4
    LLONG = 5
    ULLONG = 6
    FLOAT = 7
    DOUBLE = 8
    STRING = 9
    CHAR = 10
    NULL = 11
    BOOL = 12
    NAME = 13
    UNARY = 14
    BINARY = 15

class Eolian_Expression_Mask(IntEnum):
    SINT   = 1 << 0
    UINT   = 1 << 1
    INT    = SINT | UINT
    FLOAT  = 1 << 2
    BOOL   = 1 << 3
    STRING = 1 << 4
    CHAR   = 1 << 5
    NULL   = 1 << 6
    SIGNED = SINT | FLOAT
    NUMBER = INT | FLOAT
    ALL    = NUMBER | BOOL | STRING | CHAR | NULL

class Eolian_Variable_Type(IntEnum):
    UNKNOWN = 0
    CONSTANT = 1
    GLOBAL = 2

class Eolian_Binary_Operator(IntEnum):
    INVALID = 0
    ADD = 1  # + int, float
    SUB = 2  # - int, float
    MUL = 3  # * int, float
    DIV = 4  # / int, float
    MOD = 5  # % int
    EQ = 6  # == all types
    NQ = 7  # != all types
    GT = 8  # >  int, float
    LT = 9  # <  int, float
    GE = 10  # >= int, float
    LE = 11  # <= int, float
    AND = 12  # && all types
    OR =  13  # || all types
    BAND = 14  # &  int
    BOR =  15  # |  int
    BXOR = 16  # ^  int
    LSH =  17  # << int
    RSH =  18  # >> int

class Eolian_Unary_Operator(IntEnum):
    INVALID = 0
    UNM = 1  # - sint
    UNP = 2  # + sint
    NOT = 3   # ! int, float, bool
    BNOT = 4  # ~ int

class Eolian_Declaration_Type(IntEnum):
    UNKNOWN = 0
    CLASS = 1
    ALIAS = 2
    STRUCT = 3
    ENUM = 4
    VAR = 5

class Eolian_Doc_Token_Type(IntEnum):
    UNKNOWN = 0
    TEXT = 1
    REF = 2
    MARK_NOTE = 3
    MARK_WARNING = 4
    MARK_REMARK = 5
    MARK_TODO = 6
    MARKUP_MONOSPACE = 7

class Eolian_Doc_Ref_Type(IntEnum):
    INVALID = 0
    CLASS = 1
    FUNC = 2
    EVENT = 3
    ALIAS = 4
    STRUCT = 5
    STRUCT_FIELD = 6
    ENUM = 7
    ENUM_FIELD = 8
    VAR = 9


###  internal Classes  ########################################################

class Iterator(object):
    """ Generic eina iterator wrapper """
    def __init__(self, conv_func, iterator):
        self.next = self.__next__ # py2 compat
        self._conv = conv_func
        self._iter = c_void_p(iterator)
        self._tmp = c_void_p(0)

    def __iter__(self):
        return self

    def __next__(self):
        if not self._iter or not self._iter.value:
            print("NULL Iterator... Error ?")
            raise StopIteration
        if not lib.eina_iterator_next(self._iter, byref(self._tmp)):
            lib.eina_iterator_free(self._iter)
            raise StopIteration
        return self._conv(self._tmp)

    def free(self):
        lib.eina_iterator_free(self._iter)


class EolianBaseObject(object):
    def __init__(self, c_obj_pointer):
        if isinstance(c_obj_pointer, c_void_p):
            self._obj = c_void_p(c_obj_pointer.value)
        elif isinstance(c_obj_pointer, int):
            self._obj = c_void_p(c_obj_pointer)
        else:
            raise TypeError('Invalid constructor of type: %s for class: %s' % (
                            type(c_obj_pointer), self.__class__.__name__))


###  Main Eolian Unit  ########################################################

class Eolian_Unit(EolianBaseObject):
    def class_get_by_name(self, class_name):
        c_cls = lib.eolian_class_get_by_name(self._obj, _str_to_bytes(class_name))
        return Class(c_cls) if c_cls else None

    def class_get_by_file(self, file_name):
        c_cls = lib.eolian_class_get_by_file(self._obj, _str_to_bytes(file_name))
        return Class(c_cls) if c_cls else None

    @property
    def all_classes(self):
        return Iterator(Class, lib.eolian_all_classes_get(self._obj))

    @property
    def typedecl_all_enums(self):
        return Iterator(Typedecl, lib.eolian_typedecl_all_enums_get(self._obj))

    def typedecl_enum_get_by_name(self, name):
        c_tdecl = lib.eolian_typedecl_enum_get_by_name(self._obj, _str_to_bytes(name))
        return Typedecl(c_tdecl) if c_tdecl else None

    def typedecl_enums_get_by_file(self, fname):
        return Iterator(Typedecl,
            lib.eolian_typedecl_enums_get_by_file(self._obj, _str_to_bytes(fname)))

    @property
    def typedecl_all_structs(self):
        return Iterator(Typedecl, lib.eolian_typedecl_all_structs_get(self._obj))

    def typedecl_struct_get_by_name(self, name):
        c_tdecl = lib.eolian_typedecl_struct_get_by_name(self._obj, _str_to_bytes(name))
        return Typedecl(c_tdecl) if c_tdecl else None

    def typedecl_structs_get_by_file(self, fname):
        return Iterator(Typedecl,
            lib.eolian_typedecl_structs_get_by_file(self._obj, _str_to_bytes(fname)))

    @property
    def typedecl_all_aliases(self):
        return Iterator(Typedecl, lib.eolian_typedecl_all_aliases_get(self._obj))

    def typedecl_alias_get_by_name(self, name):
        c_tdecl = lib.eolian_typedecl_alias_get_by_name(self._obj, _str_to_bytes(name))
        return Typedecl(c_tdecl) if c_tdecl else None

    def typedecl_aliases_get_by_file(self, fname):
        return Iterator(Typedecl,
            lib.eolian_typedecl_aliases_get_by_file(self._obj, _str_to_bytes(fname)))

    @property
    def variable_all_constants(self):
        return Iterator(Variable, lib.eolian_variable_all_constants_get(self._obj))

    def variable_constant_get_by_name(self, name):
        c_var = lib.eolian_variable_constant_get_by_name(self._obj, _str_to_bytes(name))
        return Variable(c_var) if c_var else None

    def variable_constants_get_by_file(self, fname):
        return Iterator(Variable,
            lib.eolian_variable_constants_get_by_file(self._obj, _str_to_bytes(fname)))

    @property
    def variable_all_globals(self):
        return Iterator(Variable, lib.eolian_variable_all_globals_get(self._obj))

    def variable_global_get_by_name(self, name):
        c_var = lib.eolian_variable_global_get_by_name(self._obj, _str_to_bytes(name))
        return Variable(c_var) if c_var else None

    def variable_globals_get_by_file(self, fname):
        return Iterator(Variable,
            lib.eolian_variable_globals_get_by_file(self._obj, _str_to_bytes(fname)))

    @property
    def all_declarations(self):
        return Iterator(Declaration, lib.eolian_all_declarations_get(self._obj))

    def declaration_get_by_name(self, name):
        c_decl = lib.eolian_declaration_get_by_name(self._obj, _str_to_bytes(name))
        return Declaration(c_decl) if c_decl else None

    def declarations_get_by_file(self, fname):
        return Iterator(Declaration,
            lib.eolian_declarations_get_by_file(self._obj, _str_to_bytes(fname)))


class Eolian(Eolian_Unit):
    def __init__(self):
        self._obj = lib.eolian_new()  # Eolian *

    def __del__(self):
        # TODO I'm not sure about this, It is automatically called on gc, that
        #      is fired after atexit (eolian_shutdown). Thus causing a segfault
        #      if the user do not call del before exit.
        lib.eolian_free(self._obj)

    def file_parse(self, filepath):
        c_unit = lib.eolian_file_parse(self._obj, _str_to_bytes(filepath))
        return Eolian_Unit(c_unit) if c_unit else None

    @property
    def all_eo_file_paths(self):
        return Iterator(_str_to_py, lib.eolian_all_eo_file_paths_get(self._obj))

    @property
    def all_eot_file_paths(self):
        return Iterator(_str_to_py, lib.eolian_all_eot_file_paths_get(self._obj))

    @property
    def all_eo_files(self):
        return Iterator(_str_to_py, lib.eolian_all_eo_files_get(self._obj))

    @property
    def all_eot_files(self):
        return Iterator(_str_to_py, lib.eolian_all_eot_files_get(self._obj))

    def directory_scan(self, dir_path):
        return bool(lib.eolian_directory_scan(self._obj, _str_to_bytes(dir_path)))

    def system_directory_scan(self):
        return bool(lib.eolian_system_directory_scan(self._obj))

    def all_eo_files_parse(self):
        return bool(lib.eolian_all_eo_files_parse(self._obj))

    def all_eot_files_parse(self):
        return bool(lib.eolian_all_eot_files_parse(self._obj))


###  Eolian Classes  ##########################################################

class Class(EolianBaseObject):
    def __repr__(self):
        return "<eolian.Class '{0.full_name}', {0.type!s}>".format(self)

    @property
    def name(self):
        return _str_to_py(lib.eolian_class_name_get(self._obj))

    @property
    def full_name(self):
        return _str_to_py(lib.eolian_class_full_name_get(self._obj))

    @property
    def c_name(self):
        s = lib.eolian_class_c_name_get(self._obj)
        ret = _str_to_py(s)
        lib.eina_stringshare_del(c_void_p(s))
        return ret

    @property
    def c_get_function_name(self):
        s = lib.eolian_class_c_get_function_name_get(self._obj)
        ret = _str_to_py(s)
        lib.eina_stringshare_del(c_void_p(s))
        return ret

    @property
    def type(self):
        return Eolian_Class_Type(lib.eolian_class_type_get(self._obj))
    
    @property
    def data_type(self):
        return _str_to_py(lib.eolian_class_data_type_get(self._obj))

    @property
    def c_data_type(self):
        s = lib.eolian_class_c_data_type_get(self._obj)
        ret = _str_to_py(s)
        lib.eina_stringshare_del(c_void_p(s))
        return ret

    @property
    def legacy_prefix(self):
        return _str_to_py(lib.eolian_class_legacy_prefix_get(self._obj))

    @property
    def eo_prefix(self):
        return _str_to_py(lib.eolian_class_eo_prefix_get(self._obj))

    @property
    def event_prefix(self):
        return _str_to_py(lib.eolian_class_event_prefix_get(self._obj))

    @property
    def documentation(self):
        c_doc = lib.eolian_class_documentation_get(self._obj)
        return Documentation(c_doc) if c_doc else None

    @property
    def constructors(self):
        return Iterator(Constructor, lib.eolian_class_constructors_get(self._obj))

    @property
    def events(self):
        return Iterator(Event, lib.eolian_class_events_get(self._obj))

    def event_get_by_name(self, event_name):
        c_event = lib.eolian_class_event_get_by_name(self._obj,
                                                     _str_to_bytes(event_name))
        return Event(c_event) if c_event else None

    @property
    def inherits(self):
        return Iterator(Class, lib.eolian_class_inherits_get(self._obj))

    @property
    def inherits_full(self):
        L = []
        def do_class_recursive(cls):
            for other in cls.inherits:
                if not other in L:
                    L.append(other)
                do_class_recursive(other)

        do_class_recursive(self)
        return L

    @property
    def base_class(self):
        inherits = list(self.inherits)
        if len(inherits) > 0:
            return inherits[0]

    @property
    def namespaces(self):
        return Iterator(_str_to_py, lib.eolian_class_namespaces_get(self._obj))

    @property
    def file(self):
        return _str_to_py(lib.eolian_class_file_get(self._obj))

    @property
    def ctor_enable(self):
        return bool(lib.eolian_class_ctor_enable_get(self._obj))

    @property
    def dtor_enable(self):
        return bool(lib.eolian_class_dtor_enable_get(self._obj))

    def function_get_by_name(self, func_name,
                             ftype=Eolian_Function_Type.UNRESOLVED):
        f = lib.eolian_class_function_get_by_name(self._obj,
                                                  _str_to_bytes(func_name),
                                                  ftype)
        return Function(f) if f else None

    def functions_get(self, ftype):
        return Iterator(Function, lib.eolian_class_functions_get(self._obj, ftype))

    @property
    def methods(self):
        return self.functions_get(Eolian_Function_Type.METHOD)

    @property
    def properties(self):
        return self.functions_get(Eolian_Function_Type.PROPERTY)

    @property
    def implements(self):
        return Iterator(Implement, lib.eolian_class_implements_get(self._obj))

    @property
    def parts(self):
        return Iterator(Part, lib.eolian_class_parts_get(self._obj))


class Part(EolianBaseObject):
    def __repr__(self):
        return "<eolian.Part '{0.name}'>".format(self)

    @property
    def name(self):
        return _str_to_py(lib.eolian_part_name_get(self._obj))

    @property
    def class_(self):
        return Class(lib.eolian_part_class_get(self._obj))

    @property
    def documentation(self):
        c_doc = lib.eolian_part_documentation_get(self._obj)
        return Documentation(c_doc) if c_doc else None


class Constructor(EolianBaseObject):
    def __repr__(self):
        return "<eolian.Constructor '{0.full_name}', optional={0.is_optional}>".format(self)

    @property
    def full_name(self):
        return _str_to_py(lib.eolian_constructor_full_name_get(self._obj))

    @property
    def function(self):
        return Function(lib.eolian_constructor_function_get(self._obj))

    @property
    def is_optional(self):
        return bool(lib.eolian_constructor_is_optional(self._obj))

    @property
    def class_(self):
        return Class(lib.eolian_constructor_class_get(self._obj))


class Event(EolianBaseObject):
    def __repr__(self):
        return "<eolian.Event '{0.name}', c_name='{0.c_name}'>".format(self)

    @property
    def name(self):
        return _str_to_py(lib.eolian_event_name_get(self._obj))

    @property
    def c_name(self):
        s = lib.eolian_event_c_name_get(self._obj)
        ret = _str_to_py(s)
        lib.eina_stringshare_del(c_void_p(s))
        return ret

    @property
    def type(self):
        c_type = lib.eolian_event_type_get(self._obj)
        return Type(c_type) if c_type else None

    @property
    def documentation(self):
        c_doc = lib.eolian_event_documentation_get(self._obj)
        return Documentation(c_doc) if c_doc else None

    @property
    def scope(self):
        return Eolian_Object_Scope(lib.eolian_event_scope_get(self._obj))

    @property
    def is_beta(self):
        return bool(lib.eolian_event_is_beta(self._obj))

    @property
    def is_hot(self):
        return bool(lib.eolian_event_is_hot(self._obj))

    @property
    def is_restart(self):
        return bool(lib.eolian_event_is_restart(self._obj))


class Function(EolianBaseObject):
    def __repr__(self):
        return "<eolian.Function '{0.name}'>".format(self)

    @property
    def name(self):
        return _str_to_py(lib.eolian_function_name_get(self._obj))

    def full_c_name_get(self, ftype, use_legacy=False):
        s = lib.eolian_function_full_c_name_get(self._obj, ftype, use_legacy)
        ret = _str_to_py(s)
        lib.eina_stringshare_del(c_void_p(s))
        return ret

    @property
    def full_c_method_name(self):
        return self.full_c_name_get(Eolian_Function_Type.METHOD)

    @property
    def full_c_getter_name(self):
        return self.full_c_name_get(Eolian_Function_Type.PROP_GET)
    
    @property
    def full_c_setter_name(self):
        return self.full_c_name_get(Eolian_Function_Type.PROP_SET)

    @property
    def full_c_method_name_legacy(self):
        return self.full_c_name_get(Eolian_Function_Type.METHOD, True)

    @property
    def full_c_getter_name_legacy(self):
        return self.full_c_name_get(Eolian_Function_Type.PROP_GET, True)
    
    @property
    def full_c_setter_name_legacy(self):
        return self.full_c_name_get(Eolian_Function_Type.PROP_SET, True)
    
    @property
    def type(self):
        return Eolian_Function_Type(lib.eolian_function_type_get(self._obj))

    def scope_get(self, ftype):
        return Eolian_Object_Scope(lib.eolian_function_scope_get(self._obj, ftype))

    @property
    def method_scope(self):
        return self.scope_get(Eolian_Function_Type.METHOD)

    @property
    def getter_scope(self):
        return self.scope_get(Eolian_Function_Type.PROP_GET)

    @property
    def setter_scope(self):
        return self.scope_get(Eolian_Function_Type.PROP_SET)

    def legacy_get(self, ftype):
        return _str_to_py(lib.eolian_function_legacy_get(self._obj, ftype))
    
    def is_legacy_only(self, ftype):
        return bool(lib.eolian_function_is_legacy_only(self._obj, ftype))

    @property
    def is_class(self):
        return bool(lib.eolian_function_is_class(self._obj))

    @property
    def is_beta(self):
        return bool(lib.eolian_function_is_beta(self._obj))

    @property
    def object_is_const(self):
        return bool(lib.eolian_function_object_is_const(self._obj))

    @property
    def class_(self):
        c_cls = lib.eolian_function_class_get(self._obj)
        return Class(c_cls) if c_cls else None

    def is_constructor(self, klass):
        return bool(lib.eolian_function_is_constructor(self._obj, klass._obj))

    #  @property
    #  def is_function_pointer(self):
        #  return bool(lib.eolian_function_is_function_pointer(self._obj))

    @property
    def parameters(self):
        return Iterator(Function_Parameter,
                        lib.eolian_function_parameters_get(self._obj))

    def values_get(self, ftype):
        return Iterator(Function_Parameter,
                        lib.eolian_property_values_get(self._obj, ftype))

    @property
    def getter_values(self):
        return self.values_get(Eolian_Function_Type.PROP_GET)

    @property
    def setter_values(self):
        return self.values_get(Eolian_Function_Type.PROP_SET)

    def return_type_get(self, ftype):
        c_type = lib.eolian_function_return_type_get(self._obj, ftype)
        return Type(c_type) if c_type else None

    def return_default_value(self, ftye):
        c_expr = lib.eolian_function_return_default_value_get(sel._obj, ftype)
        return Expression(c_expr) if c_expr else None
        
    def return_documentation(self, ftype):
        c_doc = lib.eolian_function_return_documentation_get(self._obj, ftype)
        return Documentation(c_doc) if c_doc else None

    def return_is_warn_unused(self, ftype):
        return bool(lib.eolian_function_return_is_warn_unused(self._obj, ftype))

    @property
    def method_return_type(self):
        return self.return_type_get(Eolian_Function_Type.METHOD)

    @property
    def getter_return_type(self):
        return self.return_type_get(Eolian_Function_Type.PROP_GET)

    @property
    def setter_return_type(self):
        return self.return_type_get(Eolian_Function_Type.PROP_SET)
    
    @property
    def prop_readable(self):
        # TODO: maybe there is a better way to do this...
        ftype = Eolian_Function_Type.PROP_GET
        scope = lib.eolian_function_scope_get(self._obj, ftype)
        return True if scope != Eolian_Object_Scope.UNKNOWN else False

    @property
    def prop_writable(self):
        # TODO: maybe there is a better way to do this...
        ftype = Eolian_Function_Type.PROP_SET
        scope = lib.eolian_function_scope_get(self._obj, ftype)
        return True if scope != Eolian_Object_Scope.UNKNOWN else False

    @property
    def implement(self):
        c_impl = lib.eolian_function_implement_get(self._obj)
        return Implement(c_impl) if c_impl else None


class Function_Parameter(EolianBaseObject):
    def __repr__(self):
        return "<eolian.Function_Parameter '{0.name}', type={0.type}," \
               " optional={0.is_optional}, nullable={0.is_nullable}>".format(self)

    @property
    def name(self):
        return _str_to_py(lib.eolian_parameter_name_get(self._obj))

    # @property
    # def name_fixed(self):
        # name = _str_to_py(lib.eolian_parameter_name_get(self._obj))
        # if name in PY_KW:
            # return name + '_'
        # return name

    # name = name_fixed

    @property
    def direction(self):
        return Eolian_Parameter_Dir(lib.eolian_parameter_direction_get(self._obj))

    @property
    def documentation(self):
        c_doc = lib.eolian_parameter_documentation_get(self._obj)
        return Documentation(c_doc) if c_doc else None

    @property
    def is_nonull(self):
        return bool(lib.eolian_parameter_is_nonull(self._obj))

    @property
    def is_nullable(self):
        return bool(lib.eolian_parameter_is_nullable(self._obj))

    @property
    def is_optional(self):
        return bool(lib.eolian_parameter_is_optional(self._obj))

    @property
    def type(self):
        c_type = lib.eolian_parameter_type_get(self._obj)
        return Type(c_type) if c_type else None

    @property
    def default_value(self):
        c_expr = lib.eolian_parameter_default_value_get(self._obj)
        return Expression(c_expr) if c_expr else None


class Implement(EolianBaseObject):
    def __repr__(self):
        return "<eolian.Implement '{0.full_name}'>".format(self)

    @property
    def full_name(self):
        return _str_to_py(lib.eolian_implement_full_name_get(self._obj))

    @property
    def class_(self):
        c_cls = lib.eolian_implement_class_get(self._obj)
        return Class(c_cls) if c_cls else None

    def function_get(self, ftype=Eolian_Function_Type.UNRESOLVED):
        c_func = lib.eolian_implement_function_get(self._obj, ftype)
        return Function(c_func) if c_func else None
    # TODO implement util properties for function_get

    def documentation_get(self, ftype=Eolian_Function_Type.METHOD):
        c_doc = lib.eolian_implement_documentation_get(self._obj, ftype)
        return Documentation(c_doc) if c_doc else None
    # TODO implement util properties for documentation_get

    def is_auto(self, ftype=Eolian_Function_Type.METHOD):
        return bool(lib.eolian_implement_is_auto(self._obj, ftype))
    # TODO implement util properties for is_auto

    def is_empty(self, ftype=Eolian_Function_Type.METHOD):
        return bool(lib.eolian_implement_is_empty(self._obj, ftype))
    # TODO implement util properties for is_empty

    def is_pure_virtual(self, ftype=Eolian_Function_Type.METHOD):
        return bool(lib.eolian_implement_is_pure_virtual(self._obj, ftype))
    # TODO implement util properties for is_pure_virtual

    @property
    def is_prop_set(self):
        return bool(lib.eolian_implement_is_prop_set(self._obj))

    @property
    def is_prop_get(self):
        return bool(lib.eolian_implement_is_prop_get(self._obj))


class Type(EolianBaseObject):  # OK  (4 eolian issue)
    def __repr__(self):
        #  return "<eolian.Type '{0.full_name}', type: {0.type!s}, c_type: '{0.c_type}'>".format(self)
        return "<eolian.Type '{0.full_name}', type={0.type!s}>".format(self)

    @property
    def name(self):
        return _str_to_py(lib.eolian_type_name_get(self._obj))

    @property
    def full_name(self):
        return _str_to_py(lib.eolian_type_full_name_get(self._obj))

    @property
    def namespaces(self):
        return Iterator(_str_to_py,
                        lib.eolian_type_namespaces_get(self._obj))

    @property
    def free_func(self):
        return _str_to_py(lib.eolian_type_free_func_get(self._obj))

    @property
    def type(self):
        return Eolian_Type_Type(lib.eolian_type_type_get(self._obj))

    @property
    def builtin_type(self):
        return Eolian_Type_Builtin_Type(lib.eolian_type_builtin_type_get(self._obj))

    # TODO FIXME STRANGE API (need Eolian_Unit*)
    @property
    def c_type(self):
        #  return _str_to_py(lib.eolian_type_c_type_get(self._obj))
        return 'FIXME'

    # TODO FIXME STRANGE API (need Eolian_Unit*)
    #  @property
    #  def typedecl(self):
        #  c_tdecl = lib.eolian_type_typedecl_get(self._obj)
        #  return Typedecl(c_tdecl) if c_tdecl else None

    @property
    def base_type(self):
        c_type = lib.eolian_type_base_type_get(self._obj)
        return Type(c_type) if c_type else None 

    @property
    def next_type(self):
        c_type = lib.eolian_type_next_type_get(self._obj)
        return Type(c_type) if c_type else None 

    # TODO FIXME STRANGE API (need Eolian_Unit*)
    #  @property
    #  def aliased_base(self):
        #  c_type = lib.eolian_type_aliased_base_get(self._obj)
        #  return Type(c_type) if c_type else None 

    # TODO FIXME STRANGE API (need Eolian_Unit*)
    #  @property
    #  def class_(self):
        #  c_cls = lib.eolian_type_class_get(self._obj)
        #  return Class(c_cls) if c_cls else None

    @property
    def file(self):
        return _str_to_py(lib.eolian_type_file_get(self._obj))

    @property
    def is_owned(self):
        return bool(lib.eolian_type_is_owned(self._obj))

    @property
    def is_const(self):
        return bool(lib.eolian_type_is_const(self._obj))

    @property
    def is_ptr(self):
        return bool(lib.eolian_type_is_ptr(self._obj))


class Typedecl(EolianBaseObject):  # OK (2 TODO)
    def __repr__(self):
        return "<eolian.Typedecl '{0.full_name}', type={0.type!s}>".format(self)

    @property
    def name(self):
        return _str_to_py(lib.eolian_typedecl_name_get(self._obj))

    @property
    def full_name(self):
        return _str_to_py(lib.eolian_typedecl_full_name_get(self._obj))

    @property
    def file(self):
        return _str_to_py(lib.eolian_typedecl_file_get(self._obj))

    @property
    def type(self):
        return Eolian_Typedecl_Type(lib.eolian_typedecl_type_get(self._obj))

    # TODO FIX THIS, need Eolian_Unit* param  ???
    #  @property
    #  def c_type(self):
        #  return _str_to_py(lib.eolian_typedecl_c_type_get(self._obj))

    @property
    def namespaces(self):
        return Iterator(_str_to_py,
                        lib.eolian_typedecl_namespaces_get(self._obj))

    @property
    def free_func(self):
        return _str_to_py(lib.eolian_typedecl_free_func_get(self._obj))

    @property
    def is_extern(self):
        return bool(lib.eolian_typedecl_is_extern(self._obj))

    @property
    def enum_fields(self):
        return Iterator(Enum_Type_Field,
                        lib.eolian_typedecl_enum_fields_get(self._obj))

    def enum_field_get(self, field):
        c_field = lib.eolian_typedecl_enum_field_get(self._obj, _str_to_bytes(field))
        return Enum_Type_Field(c_field) if c_field else None

    @property
    def struct_fields(self):
        return Iterator(Struct_Type_Field,
                        lib.eolian_typedecl_struct_fields_get(self._obj)) 

    def struct_field_get(self, field):
        c_field = lib.eolian_typedecl_struct_field_get(self._obj, _str_to_bytes(field))
        return Struct_Type_Field(c_field) if c_field else None

    @property
    def base_type(self):
        c_type = lib.eolian_typedecl_base_type_get(self._obj)
        return Type(c_type) if c_type else None

    # TODO FIX THIS, need Eolian_Unit* param  ???
    #  @property
    #  def aliased_base(self):
        #  c_type = lib.eolian_typedecl_aliased_base_get(self._obj)
        #  return Type(c_type) if c_type else None

    @property
    def documentation(self):
        c_doc = lib.eolian_typedecl_documentation_get(self._obj)
        return Documentation(c_doc) if c_doc else None

    @property
    def enum_legacy_prefix(self):
        return _str_to_py(lib.eolian_typedecl_enum_legacy_prefix_get(self._obj))

    @property
    def function_pointer(self):
        c_func = lib.eolian_typedecl_function_pointer_get(self._obj)
        return Function(c_func) if c_func else None


class Enum_Type_Field(EolianBaseObject):
    def __repr__(self):
        return "<eolian.Enum_Type_Field '{0.name}', c_name='{0.c_name}'>".format(self)

    @property
    def name(self):
        return _str_to_py(lib.eolian_typedecl_enum_field_name_get(self._obj))

    @property
    def c_name(self):
        s = lib.eolian_typedecl_enum_field_c_name_get(self._obj)
        ret = _str_to_py(s)
        lib.eina_stringshare_del(c_void_p(s))
        return ret

    @property
    def value(self):
        c_expr = lib.eolian_typedecl_enum_field_value_get(self._obj, True)
        return Expression(c_expr) if c_expr else None

    @property
    def documentation(self):
        c_doc = lib.eolian_typedecl_enum_field_documentation_get(self._obj)
        return Documentation(c_doc) if c_doc else None


class Struct_Type_Field(EolianBaseObject):
    def __repr__(self):
        return "<eolian.Struct_Type_Field '{0.name}', type={0.type!s}>".format(self)

    @property
    def name(self):
        return _str_to_py(lib.eolian_typedecl_struct_field_name_get(self._obj))

    @property
    def type(self):
        c_type = lib.eolian_typedecl_struct_field_type_get(self._obj)
        return Type(c_type) if c_type else None
    
    @property
    def documentation(self):
        c_doc = lib.eolian_typedecl_struct_field_documentation_get(self._obj)
        return Documentation(c_doc) if c_doc else None


class Expression(EolianBaseObject):
    def __repr__(self):
        return "<eolian.Expression type={0.type!s}, serialize='{0.serialize}'>".format(self)

    @property
    def type(self):
        return Eolian_Expression_Type(lib.eolian_expression_type_get(self._obj))

    # TODO: EAPI Eolian_Value eolian_expression_value_get(const Eolian_Expression *expr);

    @property
    def serialize(self):
        s = lib.eolian_expression_serialize(self._obj)
        ret = _str_to_py(s)
        lib.eina_stringshare_del(c_void_p(s))
        return ret
    
    @property
    def binary_operator(self):
        c_op = lib.eolian_expression_binary_operator_get(self._obj)
        return Eolian_Binary_Operator(c_op) if c_op is not None else None

    @property
    def binary_lhs(self):
        c_expr = lib.eolian_expression_binary_lhs_get(self._obj)
        return Expression(c_expr) if c_expr else None

    @property
    def binary_rhs(self):
        c_expr = lib.eolian_expression_binary_rhs_get(self._obj)
        return Expression(c_expr) if c_expr else None

    @property
    def unary_operator(self):
        c_op = lib.eolian_expression_unary_operator_get(self._obj)
        return Eolian_Unary_Operator(c_op) if c_op is not None else None

    @property
    def unary_expression(self):
        c_expr = lib.eolian_expression_unary_expression_get(self._obj)
        return Expression(c_expr) if c_expr is not None else None


class Variable(EolianBaseObject):
    def __repr__(self):
        return "<eolian.Variable '{0.full_name}', type={0.type!s}, file={0.file}>".format(self)

    @property
    def name(self):
        return _str_to_py(lib.eolian_variable_name_get(self._obj))

    @property
    def full_name(self):
        return _str_to_py(lib.eolian_variable_full_name_get(self._obj))

    @property
    def namespaces(self):
        return Iterator(_str_to_py,
                        lib.eolian_variable_namespaces_get(self._obj))

    @property
    def type(self):
        return Eolian_Variable_Type(lib.eolian_variable_type_get(self._obj))

    @property
    def value(self):
        c_expr = lib.eolian_variable_value_get(self._obj)
        return Expression(c_expr) if c_expr else None

    @property
    def file(self):
        return _str_to_py(lib.eolian_variable_file_get(self._obj))

    @property
    def base_type(self):
        c_type = lib.eolian_variable_base_type_get(self._obj)
        return Type(c_type) if c_type else None

    @property
    def is_extern(self):
        return bool(lib.eolian_variable_is_extern(self._obj))
    
    
    @property
    def documentation(self):
        c_doc = lib.eolian_variable_documentation_get(self._obj)
        return Documentation(c_doc) if c_doc else None


class Declaration(EolianBaseObject):
    def __repr__(self):
        return "<eolian.Declaration '{0.name}'>".format(self)

    @property
    def name(self):
        return _str_to_py(lib.eolian_declaration_name_get(self._obj))

    @property
    def type(self):
        return Eolian_Declaration_Type(lib.eolian_declaration_type_get(self._obj))

    @property
    def class_(self):
        c_cls = lib.eolian_declaration_class_get(self._obj)
        return Class(c_cls) if c_cls else None

    @property
    def data_type(self):
        c_typedec = lib.eolian_declaration_data_type_get(self._obj)
        return Typedecl(c_typedec) if c_typedec else None

    @property
    def variable(self):
        c_var = lib.eolian_declaration_variable_get(self._obj)
        return Variable(c_var) if c_var else None


class Documentation(EolianBaseObject):
    # def __repr__(self):
        # return "<eolian.Documentation '{0.name}'>".format(self)

    # this is too much for py, just use string.split('\n\n')
    # def string_split(self, string):
    #    c_list = lib.eolian_documentation_string_split

    @property
    def summary(self):
        return _str_to_py(lib.eolian_documentation_summary_get(self._obj))

    @property
    def description(self):
        return _str_to_py(lib.eolian_documentation_description_get(self._obj))

    @property
    def since(self):
        return _str_to_py(lib.eolian_documentation_since_get(self._obj))


###  internal string encode/decode  ###########################################

def _str_to_bytes(s):
    return s.encode('utf-8')

def _str_to_py(s):
    if s is None:
        return None    
    if isinstance(s, bytes):
        return s.decode('utf-8')
    if isinstance(s, c_char_p):
        return s.value.decode('utf-8')
    if isinstance(s, c_void_p):
        return cast(s, c_char_p).value.decode('utf-8')
    if isinstance(s, int):
        return cast(s, c_char_p).value.decode('utf-8')
    print('WARNING !!!!!!!!! Unknown type: %s' % type(s))


###  module init/shutdown  ####################################################

import atexit
lib.eolian_init()
atexit.register(lambda: lib.eolian_shutdown())



###  API coverage statistics  #################################################

if __name__ == '__main__':
    import sys
    import os
    import re

    # find Eolian.h in source tree
    script_path = os.path.dirname(os.path.realpath(__file__))
    eolian_header = os.path.join(script_path, '..', '..', 'lib', 'eolian', 'Eolian.h')
    eolian_header = os.path.abspath(eolian_header)

    # prepare the two regexp
    flags = re.S | re.M
    DEFINED_RE = re.compile('^EAPI[\w\n *]*(eolian_\w*)\([\w *,]*\);', flags)
    USED_RE = re.compile('lib\.(eolian_[\w]*)\(', flags)

    # extract all EAPI functions from Eolian.h
    defined_funcs = []
    with open(eolian_header, 'r') as fh:
        header = fh.read()
        for match in re.finditer(DEFINED_RE, header):
            func_name = match.group(1)
            defined_funcs.append(func_name)
    defined_funcs = set(defined_funcs)

    # extract all called functions in eolian.py (this file)
    used_funcs = []
    with open(__file__, 'r') as fh:
        source = fh.read()
        for match in re.finditer(USED_RE, source):
            func_name = match.group(1)
            used_funcs.append(func_name)
    used_funcs = set(used_funcs)

    # show general info
    num_def = len(defined_funcs)
    num_usd = len(used_funcs)
    print('Pyolian coverage results')
    print('========================')
    print('Found %d functions defined in Eolian.h (%s)' % (num_def, eolian_header))
    print('Found %d functions used in eolian.py (hopefully not commented out)' % num_usd)
    print('Total API coverage %.1f%%' % (num_usd / num_def * 100))
    print()

    # list all missing functions
    missing = defined_funcs - used_funcs
    print('{} Missing functions in eolian.py'.format(len(missing)))
    print('=================================')
    for i, func_name in enumerate(sorted(missing), 1):
        print('{:02d}. {}'.format(i, func_name))
    print()

    # List all functions found in Eolian.h  (--all option)
    if '--all' in sys.argv:
        print('{} functions found in Eolian.h'.format(num_def))
        print('===============================')
        for i, func_name in enumerate(sorted(defined_funcs), 1):
            print('{:03d}. {}'.format(i, func_name))
        print()
    else:
        print('Additional arguments')
        print('====================')
        print(' --all   To list all functions')
        print()