cnt=$(git rev-list --count master)
hsh=$(git rev-parse HEAD | cut -c1-7)
echo "#define VERSION \"$cnt-$hsh\"" > src/version.hpp
