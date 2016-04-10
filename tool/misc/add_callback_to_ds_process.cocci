
// replace struct btstack_data_source with btstack_data_source_t in function definitions
@@
identifier handler;
identifier ds;
typedef btstack_data_source_t;
@@
- int handler(struct btstack_data_source * ds)
+ int handler(btstack_data_source_t* ds)
{ ... }

@btstack_run_loop_set_data_source_handler@
identifier handler;
expression ds;
@@
btstack_run_loop_set_data_source_handler(ds, &handler);

@@
identifier btstack_run_loop_set_data_source_handler.handler;
identifier ds;
typedef btstack_data_source_callback_type_t;
@@
- int handler(btstack_data_source_t * ds)
+ void handler(btstack_data_source_t * ds, btstack_data_source_callback_type_t callback_type)
{ ... }