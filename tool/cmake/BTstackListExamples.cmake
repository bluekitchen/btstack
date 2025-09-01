include (${BTSTACK_ROOT}/example/CMakeLists.txt)

function(list_examples title examples)
    message("==== ${title} ====")
    foreach(item IN LISTS ${examples})
        message(" - ${item}")
    endforeach()
endfunction()

list_examples("General Examples"      EXAMPLES_GENERAL)
list_examples("Classic-only Examples" EXAMPLES_CLASSIC_ONLY)
list_examples("LE-only Examples"      EXAMPLES_LE_ONLY)
list_examples("Dual-Mode Examples"    EXAMPLES_DUAL_MODE)
