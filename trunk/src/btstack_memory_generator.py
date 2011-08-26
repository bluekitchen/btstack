#!/usr/bin/env python

header_template = """void * btstack_memory_STRUCT_NAME_get(void);
void   btstack_memory_STRUCT_NAME_free(void *STRUCT_NAME);"""

code_template = """
// MARK: STRUCT_TYPE
#ifdef POOL_COUNT
static STRUCT_TYPE STRUCT_NAME_storage[POOL_COUNT];
static memory_pool_t STRUCT_NAME_pool;
void * btstack_memory_STRUCT_NAME_get(void){
    return memory_pool_get(&STRUCT_NAME_pool);
}
void   btstack_memory_STRUCT_NAME_free(void *STRUCT_NAME){
    memory_pool_free(&STRUCT_NAME_pool, STRUCT_NAME);
}
#elif defined(HAVE_MALLOC)
void * btstack_memory_STRUCT_NAME_get(void){
    return malloc(sizeof(STRUCT_TYPE));
}
void  btstack_memory_STRUCT_NAME_free(void *STRUCT_NAME){
    free(STRUCT_NAME);
}
#endif
"""

init_template = """#ifdef POOL_COUNT
    memory_pool_create(&STRUCT_NAME_pool, STRUCT_NAME_storage, POOL_COUNT, sizeof(STRUCT_TYPE));
#endif"""

def replacePlaceholder(template, struct_name):
    struct_type = struct_name + '_t'
    pool_count = "MAX_NO_" + struct_name.upper() + "S"
    
    snippet = template.replace("STRUCT_TYPE", struct_type).replace("STRUCT_NAME", struct_name).replace("POOL_COUNT", pool_count)
    return snippet
    
list_of_structs = [ "hci_connection", "l2cap_service", "l2cap_channel", "rfcomm_multiplexer", "rfcomm_service", "rfcomm_channel", "db_mem_device_name", "db_mem_device_link_key", "db_mem_service"]

print "// header file"
for struct_name in list_of_structs:
    print replacePlaceholder(header_template, struct_name)

print "// template code"
for struct_name in list_of_structs:
    print replacePlaceholder(code_template, struct_name)

print "// init"
print "void btstack_memory_init(void){"
for struct_name in list_of_structs:
    print replacePlaceholder(init_template, struct_name)
print "}"

    