#!/bin/bash

# Fetch the latest commit SHA from the 'implicit' branch of the '3MFConsortium/lib3mf' repository
commit_sha=$(curl -s "https://api.github.com/repos/3MFConsortium/lib3mf/commits/implicit" | grep sha | head -n 1 | cut -d '"' -f 4)

# Replace the REF value in the portfile.cmake file with the fetched commit SHA
sed -i "0,/REF .*/s/REF .*/REF $commit_sha/" portfile.cmake

# Store previous directory
old_dir=$(pwd)
cd /tmp/

vcpkg remove lib3mf 

# Capture the output of the vcpkg install command
output=$(vcpkg install lib3mf)

# Extract the actual hash from the output
actual_hash=$(echo "$output" | grep 'Actual hash:' | cut -d ' ' -f 3)
echo "Actual hash: $actual_hash"

# Replace the expected hash in the portfile.cmake file with the actual hash
sed -i "0,/SHA512 .*/s//SHA512 $actual_hash/" $old_dir/portfile.cmake

vcpkg install lib3mf

# Restore the previous directory
cd $old_dir