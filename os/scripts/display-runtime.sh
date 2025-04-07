#!/usr/bin/env bash
#https://www.youtube.com/watch?v=Jp58Osb1uFo
#from host, run:
#$ echo $DISPLAY && xauth list $DISPLAY
#
#in the container, run $ export DISPLAY=display && ./display-runtime.sh -x xauthtoken
#e.g., $ export DISPLAY=:0.0 && ./display-runtime.sh -x deaddeaddeaddeaddeaddeaddeaddead
#export DISPLAY=:0.0
#xauth add :0.0 . deaddeaddeaddeaddeaddeaddeaddead

#run this from host using docker exec after running the container

while getopts ":hx:" opt; do
  case ${opt} in
    h )
	  echo "Usage:"
	  echo "  display-runtime -h							Display this help message."
	  echo "  display-runtime -x xauthtoken					Setup a display in container that is authorized with host."
	  exit 0
	  ;;
    x ) # process option x
      xauthtoken=$OPTARG
      ;;
    \? ) echo "Incorrect usage.  For help, run display-runtime -h"
	  exit 1
      ;;
  esac
done

#from container:
touch ~/.Xauthority
echo $(xauth add $DISPLAY . ${xauthtoken})