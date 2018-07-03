cnt=$(git rev-list --count master)
hsh=$(git rev-parse HEAD | cut -c1-7)
vsn=$(cat VERSION)
echo "#define VERSION \"$vsn-$cnt-$hsh\"" > src/Version.hpp
