case ${1##*/} in
    "") redo ${1}main.bin ;;
    * ) test -f "$1" && mv "$1" "$3" ;;
esac

