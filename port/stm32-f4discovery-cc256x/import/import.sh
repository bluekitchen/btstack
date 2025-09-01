#!/bin/bash
SCRIPT_PATH="$(dirname -- "$0")"
STM32F4_FW_VERSION="V1.28.0"
STM32F4_FW_PATH="${HOME}/STM32Cube/Repository/STM32Cube_FW_F4_"

die() { echo "$*" >&2; exit 2; }  # complain to STDERR and exit with error

usage() {                                 # Function: Print a help message.
    echo "Usage: $0 [OPTION..]
    -m|--middleware  copy middleware from $STM32F4_FW_PATH
    -b|--bsp         copy BSP from $STM32F4_FW_PATH
    -p|--patch       apply changes required to firware source
    -c|--commit      commit changes
    -a|--all         enable all actions, equal to \"-m -b -p -c\"
    -v|--fw-version  specify firmware version for
    -f|--fw-path     location of STM32Cube FW repository
    " 1>&2 
    die
}

needs_arg() { if [ -z "$OPTARG" ]; then die "No arg for --$OPT option"; fi; }

dosomething=false
domdlware=false
dobsp=false
dopatch=false
docommit=false
bravo="$HOME/Downloads"       # Overridden by the value set by -b or --bravo
charlie_default="brown"       # Only set given -c or --charlie without an arg
while getopts mbpcahf:v:-: OPT; do  # allow -a, -b with arg, -c, and -- "with arg"
  # support long options: https://stackoverflow.com/a/28466267/519360
  if [ "$OPT" = "-" ]; then   # long option: reformulate OPT and OPTARG
    OPT="${OPTARG%%=*}"       # extract long option name
    OPTARG="${OPTARG#"$OPT"}" # extract long option argument (may be empty)
    OPTARG="${OPTARG#=}"      # if long option argument, remove assigning `=`
  fi
  case "$OPT" in
    m | middleware  ) domdlware=true; dosomething=true ;;
    b | bsp )         dobsp=true; dosomething=true ;;
    p | patch )       dopatch=true; dosomething=true ;;
    c | commit )      docommit=true; dosomething=true ;;
    a | all )         domdlware=true; dobsp=true; dopatch=true; docommit=true; dosomething=true ;;
    h | help )        usage ;;
    f | fw-path )     needs_arg; STM32F4_FW_PATH="$OPTARG" ;;
    v | fw-version )  needs_arg; STM32F4_FW_VERSION="$OPTARG" ;;
    \? )           exit 2 ;;  # bad short option (error reported via getopts)
    * )            die "Illegal option --$OPT" ;;            # bad long option
  esac
done
shift $((OPTIND-1)) # remove parsed options and args from $@ list

if [ "$dosomething" = false ]; then
    usage
fi

STM32F4_FW_PATH="${STM32F4_FW_PATH}${STM32F4_FW_VERSION}"
AMEND_MESSAGE="stm32-f4discovery-nrf3540: update STM32F4 Firmware to ${STM32F4_FW_VERSION}"
echo "used version: ${STM32F4_FW_VERSION}"
echo "used location: ${STM32F4_FW_PATH}"

if [ "$domdlware" = true ]; then 
    echo "copy Middleware to " $SCRIPT_PATH/../Middlewares
    mkdir -v -p $SCRIPT_PATH/../Middlewares/ST/
    cp -a $STM32F4_FW_PATH/Middlewares/ST/STM32_Audio/ $SCRIPT_PATH/../Middlewares/ST/
    retVal=$?
    if [ $retVal -ne 0 ]; then
        die "Failed to copy Middleware"
    fi
fi

if [ "$dobsp" = true ]; then
    echo "copy BSP to " $SCRIPT_PATH/Drivers/BSP
    cp -a $STM32F4_FW_PATH/Drivers/BSP $SCRIPT_PATH/../Drivers/
    retVal=$?
    if [ $retVal -ne 0 ]; then
        die "Failed to copy Drivers"
    fi
fi

if [ "$dopatch" = true ]; then
    patch -d $SCRIPT_PATH/../ -r - --forward -p3 < $SCRIPT_PATH/stm32f4_discovery_audio.diff
    patch -d $SCRIPT_PATH/../ -r - --forward -p3 < $SCRIPT_PATH/stm32f4xx_hal_uart.diff
fi

if [ "$docommit" = true ]; then
    git add $SCRIPT_PATH/../STM32F407VGTx_FLASH.ld
    git add $SCRIPT_PATH/../startup_stm32f407xx.s
    git add $SCRIPT_PATH/../Middlewares
    git add $SCRIPT_PATH/../Drivers
    git add $SCRIPT_PATH/../Src
    git add $SCRIPT_PATH/../Inc
    git commit -m "$AMEND_MESSAGE"
fi
