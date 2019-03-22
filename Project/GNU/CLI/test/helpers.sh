if type -p md5sum ; then
    md5cmd="md5sum"
elif type -p md5 ; then
    md5cmd="md5 -r"
else
    exit 1
fi >/dev/null 2>&1

fd=1
if command exec >&9 ; then
    fd=9
fi >/dev/null 2>&1

valgrind=""
if command -v valgrind && test -n "${VALGRIND}" ; then
    valgrind="valgrind --quiet --log-file=valgrind.log"
fi >/dev/null 2>&1
