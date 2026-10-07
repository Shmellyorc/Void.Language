#ifndef VOIDC_SEMANTIC_H
#define VOIDC_SEMANTIC_H

#include "ast.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint32_t VcSemanticType;

typedef enum VcSemanticStandardConversionKind
{
    VC_SEM_CONVERSION_NONE = 0,
    VC_SEM_CONVERSION_IDENTITY,
    VC_SEM_CONVERSION_NUMERIC,
    VC_SEM_CONVERSION_NULLABLE,
    VC_SEM_CONVERSION_ENUM,
    VC_SEM_CONVERSION_REFERENCE,
    VC_SEM_CONVERSION_NULL,
    VC_SEM_CONVERSION_POINTER,
    VC_SEM_CONVERSION_FUNCTION_POINTER
} VcSemanticStandardConversionKind;

/* Compiler-recognized exact primitive constant categories used by the
   StandardLibrary built-in associated scopes.  The category is intentionally
   independent of the public member name: the field's semantic type plus this
   value determines the exact backend constant representation. */
typedef enum VcPrimitiveConstantKind
{
    VC_PRIMITIVE_CONSTANT_NONE = 0,
    VC_PRIMITIVE_CONSTANT_MIN,
    VC_PRIMITIVE_CONSTANT_MAX,
    VC_PRIMITIVE_CONSTANT_EPSILON,
    VC_PRIMITIVE_CONSTANT_NAN,
    VC_PRIMITIVE_CONSTANT_POSITIVE_INFINITY,
    VC_PRIMITIVE_CONSTANT_NEGATIVE_INFINITY,
    VC_PRIMITIVE_CONSTANT_ZERO,
    VC_PRIMITIVE_CONSTANT_ONE,
    VC_PRIMITIVE_CONSTANT_MINUS_ONE
} VcPrimitiveConstantKind;

enum
{
    VC_SEM_TYPE_ERROR = 0,
    VC_SEM_TYPE_UNKNOWN,
    VC_SEM_TYPE_VOID,
    VC_SEM_TYPE_BOOL,
    VC_SEM_TYPE_BYTE,
    VC_SEM_TYPE_SBYTE,
    VC_SEM_TYPE_SHORT,
    VC_SEM_TYPE_USHORT,
    VC_SEM_TYPE_INT,
    VC_SEM_TYPE_UINT,
    VC_SEM_TYPE_LONG,
    VC_SEM_TYPE_ULONG,
    VC_SEM_TYPE_FLOAT,
    VC_SEM_TYPE_DOUBLE,
    VC_SEM_TYPE_DECIMAL,
    VC_SEM_TYPE_CHAR,
    VC_SEM_TYPE_STRING,
    VC_SEM_TYPE_OBJECT,
    VC_SEM_TYPE_TYPE,
    VC_SEM_TYPE_NULL,
    VC_SEM_TYPE_STRUCT_BASE = 1024,
    VC_SEM_TYPE_NULLABLE_BASE = 0x20000000u,
    VC_SEM_TYPE_ARRAY_BASE = 0x40000000u
};

#define VC_SEM_TYPE_POINTER_BASE UINT32_C(0x80000000)
#define VC_SEM_TYPE_FUNCTION_POINTER_BASE UINT32_C(0xC0000000)

typedef struct VcSemanticUnit
{
    const VcSource *source;
    VcAstTree *tree;
} VcSemanticUnit;

typedef struct VcSemanticField
{
    const VcAstNode *node;
    VcSemanticType type;
    char c_name[32];
    char event_add_c_name[32];
    char event_remove_c_name[32];
    bool is_event;
    bool is_custom_event;
    bool is_const;
    VcPrimitiveConstantKind primitive_constant_kind;
    bool is_readonly;
    bool is_ref;
    bool ref_readonly;
    bool is_static;
    bool is_virtual;
    bool is_override;
    bool is_abstract;
    bool is_sealed;
    bool has_virtual_root;
    size_t virtual_root_struct_index;
    size_t virtual_root_field_index;
    bool event_add_reachable;
    bool event_remove_reachable;
    unsigned char event_add_analysis_state;
    unsigned char event_remove_analysis_state;
    unsigned char const_analysis_state;
} VcSemanticField;

typedef struct VcSemanticProperty
{
    const VcAstNode *node;
    VcSemanticType type;
    bool returns_ref;
    bool returns_ref_readonly;
    char getter_c_name[32];
    char setter_c_name[32];
    char backing_c_name[32];
    bool has_getter;
    bool has_setter;
    bool getter_readonly;
    bool is_auto;
    bool is_static;
    bool is_virtual;
    bool is_override;
    bool is_abstract;
    bool is_sealed;
    bool has_virtual_root;
    size_t virtual_root_struct_index;
    size_t virtual_root_property_index;
    bool getter_reachable;
    bool setter_reachable;
    unsigned char getter_analysis_state;
    unsigned char setter_analysis_state;
} VcSemanticProperty;

typedef struct VcSemanticStruct
{
    const VcAstNode *node;
    const VcSource *source;
    const char *namespace_name;
    const char *name;
    VcSemanticField *fields;
    size_t field_count;
    size_t field_capacity;
    VcSemanticProperty *properties;
    size_t property_count;
    size_t property_capacity;
    char c_name[32];
    bool is_class;
    bool is_interface;
    bool is_delegate;
    bool is_enum;
    unsigned char initializer_analysis_state;
    unsigned char static_initializer_analysis_state;
    bool static_initializer_reachable;
    const VcAstNode *static_constructor_node;
    VcSemanticType enum_underlying_type;
    const char **enum_member_names;
    int64_t *enum_member_values;
    size_t enum_member_count;
    size_t enum_member_capacity;
    bool is_abstract;
    bool is_sealed;
    bool is_readonly;
    bool is_ref_struct;
    bool has_base_class;
    size_t base_class_index;
    bool has_implicit_base_constructor;
    size_t implicit_base_constructor_index;
    bool implicit_base_params_expanded;
    size_t *interfaces;
    size_t interface_count;
    size_t interface_capacity;
    bool swizzle_enabled;
    char swizzle_family[64];
    VcSemanticType swizzle_component_type;
    size_t swizzle_component_count;
    size_t swizzle_component_fields[4];
    size_t delegate_invoke_method_index;
    /* Compiler-owned declaration scope associated with a canonical built-in
       semantic type. This is a compile-time member owner, never a runtime
       wrapper object for the primitive/string/object value itself. */
    bool is_builtin_associated_scope;
    VcSemanticType associated_builtin_type;
} VcSemanticStruct;

typedef struct VcSemanticArray
{
    VcSemanticType element_type;
    size_t rank;
    char name[96];
} VcSemanticArray;

typedef struct VcSemanticNullable
{
    VcSemanticType underlying_type;
    char name[96];
    char c_name[32];
} VcSemanticNullable;

typedef struct VcSemanticPointer
{
    VcSemanticType element_type;
    char name[96];
} VcSemanticPointer;

typedef struct VcSemanticFunctionPointer
{
    VcSemanticType return_type;
    VcSemanticType *parameter_types;
    size_t parameter_count;
    char name[256];
    char c_name[32];
} VcSemanticFunctionPointer;

typedef enum VcNativeStringContract
{
    VC_NATIVE_STRING_CONTRACT_NONE = 0,
    VC_NATIVE_STRING_CONTRACT_BORROWED_NUL,
    VC_NATIVE_STRING_CONTRACT_BORROWED_POINTER_LENGTH,
    VC_NATIVE_STRING_CONTRACT_RETAINED_NUL,
    VC_NATIVE_STRING_CONTRACT_RETAINED_POINTER_LENGTH,
    VC_NATIVE_STRING_CONTRACT_STATIC_NUL,
    VC_NATIVE_STRING_CONTRACT_STATIC_POINTER_LENGTH,
    VC_NATIVE_STRING_CONTRACT_OWNED_NUL,
    VC_NATIVE_STRING_CONTRACT_OWNED_POINTER_LENGTH
} VcNativeStringContract;

typedef struct VcSemanticConstructor
{
    const VcAstNode *node;
    const VcSource *source;
    size_t struct_index;
    VcSemanticType *parameter_types;
    VcTokenKind *parameter_modifiers;
    size_t parameter_count;
    bool has_base_constructor;
    size_t base_constructor_index;
    bool base_params_expanded;
    size_t *base_argument_parameters;
    size_t base_argument_count;
    bool has_this_constructor;
    size_t this_constructor_index;
    bool this_params_expanded;
    size_t *this_argument_parameters;
    size_t this_argument_count;
    char c_name[32];
    bool reachable;
    unsigned char analysis_state;
} VcSemanticConstructor;

typedef struct VcSemanticMethod
{
    const VcAstNode *node;
    const VcSource *source;
    const char *namespace_name;
    const char *type_name;
    size_t owner_struct_index;
    bool has_owner_struct;
    bool is_static;
    bool is_extern;
    bool is_unsafe;
    bool is_operator;
    bool is_interface_method;
    bool is_delegate_invoke;
    bool is_extension_method;
    bool is_virtual;
    bool is_override;
    bool is_abstract;
    bool is_sealed;
    bool is_readonly;
    bool is_async;
    VcSemanticType async_result_type;
    bool async_returns_value_task;
    bool has_virtual_root;
    size_t virtual_root_index;
    VcTokenKind operator_kind;
    VcSemanticType return_type;
    bool returns_ref;
    bool returns_ref_readonly;
    VcSemanticType *parameter_types;
    VcTokenKind *parameter_modifiers;
    VcNativeStringContract *native_string_parameter_contracts;
    VcNativeStringContract native_string_return_contract;
    char native_string_release_c_name[128];
    size_t parameter_count;
    char c_name[128];
    char export_c_name[128];
    bool is_exported;
    bool reachable;
    unsigned char analysis_state;
} VcSemanticMethod;

typedef enum VcSemanticCaptureKind
{
    VC_SEM_CAPTURE_LOCAL,
    VC_SEM_CAPTURE_THIS
} VcSemanticCaptureKind;

typedef enum VcSemanticCaptureScopeKind
{
    VC_SEM_CAPTURE_SCOPE_METHOD,
    VC_SEM_CAPTURE_SCOPE_LAMBDA,
    VC_SEM_CAPTURE_SCOPE_FOREACH
} VcSemanticCaptureScopeKind;

typedef struct VcSemanticCapture
{
    VcSemanticCaptureKind kind;
    VcSemanticCaptureScopeKind scope_kind;
    size_t scope_index;
    const VcAstNode *scope_node;
    const VcAstNode *declaration_node;
    const char *name;
    VcSemanticType type;
    VcTokenKind modifier;
} VcSemanticCapture;

typedef struct VcSemanticLambda
{
    const VcAstNode *node;
    const VcSource *source;
    size_t delegate_type_index;
    size_t owner_method_index;
    VcSemanticType return_type;
    VcSemanticType async_result_type;
    bool is_async;
    bool async_returns_value_task;
    VcSemanticType *parameter_types;
    VcTokenKind *parameter_modifiers;
    size_t parameter_count;
    VcSemanticCapture *captures;
    size_t capture_count;
    size_t capture_capacity;
} VcSemanticLambda;

typedef enum VcSemanticStringOperation
{
    VC_SEM_STRING_OP_NONE = 0,
    VC_SEM_STRING_OP_INSTANCE_EQUALS,
    VC_SEM_STRING_OP_STATIC_EQUALS,
    VC_SEM_STRING_OP_COMPARE_ORDINAL,
    VC_SEM_STRING_OP_CONTAINS,
    VC_SEM_STRING_OP_STARTS_WITH,
    VC_SEM_STRING_OP_ENDS_WITH,
    VC_SEM_STRING_OP_INDEX_OF_STRING,
    VC_SEM_STRING_OP_INDEX_OF_CHAR,
    VC_SEM_STRING_OP_LAST_INDEX_OF_STRING,
    VC_SEM_STRING_OP_LAST_INDEX_OF_CHAR,
    VC_SEM_STRING_OP_INSERT,
    VC_SEM_STRING_OP_REMOVE_START,
    VC_SEM_STRING_OP_REMOVE_RANGE,
    VC_SEM_STRING_OP_REPLACE_STRING,
    VC_SEM_STRING_OP_REPLACE_CHAR
} VcSemanticStringOperation;

typedef struct VcSemanticBinding
{
    const VcAstNode *node;
    const VcAstNode *declaration_node;
    VcSemanticType type;
    const char *generic_parameter_origin;
    bool is_type_receiver;
    /* Source type ref retains constructed arguments; type is the concrete identity. */
    const VcAstTypeRef *receiver_type_ref;
    size_t method_index;
    size_t constructor_index;
    size_t struct_index;
    size_t field_index;
    size_t conversion_method_index;
    bool has_method;
    bool method_returns_ref;
    bool method_returns_ref_readonly;
    bool has_constructor;
    bool has_field;
    bool has_conversion;
    bool lifted_nullable_conversion;
    VcSemanticType conversion_target_type;
    bool has_constant;
    bool has_event;
    bool has_property;
    bool property_returns_ref;
    bool property_returns_ref_readonly;
    bool has_swizzle;
    bool has_array_length;
    bool has_array_rank;
    bool has_string_length;
    bool has_string_byte_length;
    bool has_string_is_empty;
    bool has_string_empty;
    bool has_array_get_length;
    bool has_indexer_get;
    bool has_indexer_set;
    bool has_index_consumer;
    bool has_range_consumer;
    bool consumer_array;
    size_t consumer_length_struct_index;
    size_t consumer_length_property_index;
    size_t consumer_normalize_method_index;
    bool consumer_length_interface_dispatch;
    bool consumer_length_virtual_dispatch;
    bool has_compound_method;
    size_t compound_method_index;
    bool virtual_dispatch;
    bool interface_dispatch;
    bool base_call;
    bool extension_method;
    bool params_expanded;
    size_t *argument_parameters;
    size_t argument_count;
    bool has_delegate_create;
    bool has_delegate_invoke;
    bool has_function_pointer_address;
    bool has_function_pointer_invoke;
    VcSemanticType function_pointer_type;
    size_t function_pointer_method_index;
    bool has_delegate_combine;
    bool has_delegate_remove;
    bool has_delegate_equal;
    bool native_callback;
    bool has_sizeof;
    VcSemanticType sizeof_type;
    bool has_typeof;
    VcSemanticType typeof_type;
    bool has_type_metadata_member;
    bool has_nullable_has_value;
    bool has_nullable_value;
    bool null_conditional_lifted;
    VcSemanticType null_conditional_underlying_type;
    bool lifted_nullable_operator;
    VcSemanticType lifted_underlying_type;
    bool has_enum_member;
    int64_t enum_value;
    bool has_pointer_index;
    bool has_string_index;
    bool has_string_range;
    bool string_index_uses_index;
    bool has_string_substring;
    bool string_substring_has_length;
    bool has_string_concat;
    VcSemanticType string_concat_left_type;
    VcSemanticType string_concat_right_type;
    VcSemanticStringOperation string_operation;
    bool has_lambda_create;
    bool has_runtime_hash_method;
    bool has_runtime_equals_method;
    bool has_type_relation;
    VcSemanticType relation_target_type;
    size_t runtime_hash_method_index;
    size_t runtime_equals_method_index;
    size_t lambda_index;
    bool delegate_has_target;
    bool delegate_target_is_value_struct;
    size_t delegate_target_struct_index;
    size_t delegate_type_index;
    size_t delegate_method_index;
    bool has_foreach;
    bool foreach_async;
    bool foreach_array;
    bool foreach_string;
    VcSemanticType foreach_item_type;
    VcSemanticType foreach_enumerator_type;
    size_t foreach_get_enumerator_method_index;
    size_t foreach_move_next_method_index;
    size_t foreach_current_struct_index;
    size_t foreach_current_property_index;
    bool foreach_get_enumerator_virtual_dispatch;
    bool foreach_get_enumerator_interface_dispatch;
    bool foreach_move_next_virtual_dispatch;
    bool foreach_move_next_interface_dispatch;
    bool foreach_current_interface_dispatch;
    bool foreach_has_dispose;
    size_t foreach_dispose_method_index;
    bool foreach_dispose_virtual_dispatch;
    bool foreach_dispose_interface_dispatch;
    bool has_using;
    bool using_async;
    VcSemanticType using_resource_type;
    size_t using_dispose_method_index;
    bool using_dispose_virtual_dispatch;
    bool using_dispose_interface_dispatch;
    bool using_skip_dispose_if_null;
    bool has_await_protocol;
    VcSemanticType awaitable_type;
    VcSemanticType awaiter_type;
    size_t await_get_awaiter_method_index;
    size_t await_is_completed_struct_index;
    size_t await_is_completed_property_index;
    size_t await_on_completed_method_index;
    size_t await_get_result_method_index;
    bool await_get_awaiter_interface_dispatch;
    bool await_get_awaiter_virtual_dispatch;
    bool await_get_awaiter_extension_method;
    bool await_is_completed_interface_dispatch;
    bool await_is_completed_virtual_dispatch;
    bool await_on_completed_interface_dispatch;
    bool await_on_completed_virtual_dispatch;
    bool await_get_result_interface_dispatch;
    bool await_get_result_virtual_dispatch;
    bool has_fixed;
    VcSemanticType fixed_pointer_type;
    VcSemanticType fixed_owner_type;
    const VcAstNode *fixed_owner_expression;
    bool fixed_array;
    bool fixed_pinnable_protocol;
    size_t fixed_pinnable_method_index;
    bool fixed_pinnable_interface_dispatch;
    bool fixed_pinnable_virtual_dispatch;
    bool fixed_pinnable_ref_readonly;
    size_t swizzle_target_struct_index;
    size_t property_index;
    size_t swizzle_result_struct_index;
    size_t swizzle_count;
    size_t swizzle_indices[4];
} VcSemanticBinding;

typedef struct VcSemanticModel
{
    VcSemanticStruct *structs;
    size_t struct_count;
    size_t struct_capacity;
    VcSemanticArray *arrays;
    size_t array_count;
    size_t array_capacity;
    VcSemanticNullable *nullables;
    size_t nullable_count;
    size_t nullable_capacity;
    VcSemanticPointer *pointers;
    size_t pointer_count;
    size_t pointer_capacity;
    VcSemanticFunctionPointer *function_pointers;
    size_t function_pointer_count;
    size_t function_pointer_capacity;
    VcSemanticConstructor *constructors;
    size_t constructor_count;
    size_t constructor_capacity;
    VcSemanticMethod *methods;
    size_t method_count;
    size_t method_capacity;
    VcSemanticBinding *bindings;
    size_t binding_count;
    VcSemanticLambda *lambdas;
    size_t lambda_count;
    size_t lambda_capacity;
    size_t binding_capacity;
    size_t entry_method;
    bool has_entry;
    bool iterator_prelower;
    bool async_prelower;
    bool generic_inference_requested;
    const VcSemanticUnit *units;
    size_t unit_count;
    /* Canonical semantic type -> ordinary declaration/member owner bridge.
       SIZE_MAX means that the built-in has no registered associated scope. */
    size_t builtin_associated_owners[VC_SEM_TYPE_NULL + 1];
} VcSemanticModel;

typedef VcDiagnostic VcSemanticDiagnostic;

void vc_semantic_model_init(VcSemanticModel *model);
void vc_semantic_model_destroy(VcSemanticModel *model);

const char *vc_semantic_type_name(const VcSemanticModel *model, VcSemanticType type);
const char *vc_semantic_type_display_name(const VcSemanticModel *model, VcSemanticType type,
    char *output, size_t output_size);
VcSemanticType vc_semantic_builtin_type_from_ast(const VcAstTypeRef *type);
bool vc_semantic_builtin_associated_owner(
    const VcSemanticModel *model,
    VcSemanticType type,
    size_t *struct_index);
bool vc_semantic_type_is_struct(VcSemanticType type);
size_t vc_semantic_struct_index(VcSemanticType type);
VcSemanticType vc_semantic_struct_type(size_t struct_index);
bool vc_semantic_type_is_array(VcSemanticType type);
size_t vc_semantic_array_index(VcSemanticType type);
VcSemanticType vc_semantic_array_type(size_t array_index);
VcSemanticType vc_semantic_array_element_type(const VcSemanticModel *model, VcSemanticType type);
size_t vc_semantic_array_rank(const VcSemanticModel *model, VcSemanticType type);
bool vc_semantic_type_is_nullable(VcSemanticType type);
size_t vc_semantic_nullable_index(VcSemanticType type);
VcSemanticType vc_semantic_nullable_type(size_t nullable_index);
VcSemanticType vc_semantic_nullable_underlying_type(const VcSemanticModel *model, VcSemanticType type);
bool vc_semantic_type_is_pointer(VcSemanticType type);
size_t vc_semantic_pointer_index(VcSemanticType type);
VcSemanticType vc_semantic_pointer_type(size_t pointer_index);
VcSemanticType vc_semantic_pointer_element_type(const VcSemanticModel *model, VcSemanticType type);
bool vc_semantic_type_is_function_pointer(VcSemanticType type);
size_t vc_semantic_function_pointer_index(VcSemanticType type);
VcSemanticType vc_semantic_function_pointer_type(size_t function_pointer_index);
VcSemanticType vc_semantic_function_pointer_return_type(const VcSemanticModel *model, VcSemanticType type);
size_t vc_semantic_function_pointer_parameter_count(const VcSemanticModel *model, VcSemanticType type);
VcSemanticType vc_semantic_function_pointer_parameter_type(const VcSemanticModel *model, VcSemanticType type, size_t parameter_index);
bool vc_semantic_type_is_unmanaged(const VcSemanticModel *model, VcSemanticType type);
bool vc_semantic_span_type_info(
    const VcSemanticModel *model,
    VcSemanticType type,
    bool *readonly_span,
    VcSemanticType *element_type);
VcSemanticStandardConversionKind vc_semantic_classify_standard_conversion(
    const VcSemanticModel *model,
    VcSemanticType target,
    VcSemanticType source,
    bool explicit_context);
const VcSemanticBinding *vc_semantic_binding(const VcSemanticModel *model, const VcAstNode *node);

bool vc_semantic_analyze(
    const VcSemanticUnit *units,
    size_t unit_count,
    bool allow_unsafe,
    bool library_output,
    VcSemanticModel *model,
    VcSemanticDiagnostic *diagnostic);

#endif
