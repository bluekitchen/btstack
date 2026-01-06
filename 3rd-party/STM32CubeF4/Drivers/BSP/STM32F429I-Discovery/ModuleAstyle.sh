#!/bin/bash
../../../../__INTERNAL__tools/tools/McuAStyle/mcuAStyle.exe -I --no_output_warning --options=../../../../__INTERNAL__tools/tools/McuAStyle/formattingOption.txt "*.c" "*.h"
../../../../__INTERNAL__tools/tools/McuAStyle/mcuAStyle.exe -I --no_output_style   --options=../../../../__INTERNAL__tools/tools/McuAStyle/formattingOption.txt "*.c" "*.h"