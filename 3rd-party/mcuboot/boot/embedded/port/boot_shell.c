
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FreeRTOS_CLI.h>

#ifdef CONFIG_ARM64
#include "initcall.h"
#endif

extern int main_boot( void );

BaseType_t boot_shell(char *pcWriteBuffer, size_t xWriteBufferLen, size_t argc, char **argv)
{
	return 0;
}

static int boot_subcmd_tool(size_t argc, char **argv)
{

	main_boot();

	return 0;
}

static const struct cli_subcmd boot_subcmd[] = {
	{
		"start",
		"[1]: enable tool, [0]: disable tool.",
		boot_subcmd_tool,
	},
	{
		NULL,
	},
};

static const CLI_Command_Definition_t boot_command = {
	"boot",
	boot_subcmd,
	"shell cmds of boot",
	boot_shell, /* The function to run. */
	-1 /* The user can enter any number of commands. */
};

#ifdef CONFIG_ARM64
CMD_INIT(boot_command);
#endif

