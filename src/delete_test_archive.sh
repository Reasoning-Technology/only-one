#!/bin/bash

pushd test_archive/store > /dev/null
rm -f *
popd > /dev/null

pushd test_archive/tax > /dev/null
rm -f sources\;?
rm -f sources\-*
popd > /dev/null

pushd test_archive > /dev/null
cat <<EOF > version
0.3
EOF
popd > /dev/null
