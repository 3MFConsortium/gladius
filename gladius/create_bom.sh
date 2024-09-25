#!/bin/bash

# Function to check if a field contains "null" or "NOASSERTION" and replace it with ""
check_field() {
    if [ "$1" == "null" ] || [ "$1" == "NOASSERTION" ]; then
        echo ""
    else
        echo "$1"
    fi
}

# Common license file names
license_files=("LICENSE" "LICENSE.txt" "LICENSE.md" "LICENSE-MIT" "LICENSE-APACHE" "COPYING" "COPYING.txt" "COPYING.md" "licence.txt ")

# Known open source licenses
declare -A licenses
licenses=(
        ["Eclipse Public License 1.0"]="EPL-1.0"
        ["Creative Commons Attribution-NonCommercial-NoDerivatives 3.0 Unported"]="CC-BY-NC-ND-3.0"
        ["Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported"]="CC-BY-NC-SA-3.0"
        ["Creative Commons Attribution-NoDerivatives 3.0 Unported"]="CC-BY-ND-3.0"
        ["Creative Commons Attribution-NonCommercial 3.0 Unported"]="CC-BY-NC-3.0"
        ["Creative Commons Attribution-ShareAlike 3.0 Unported"]="CC-BY-SA-3.0"
        ["Creative Commons Attribution 3.0 Unported"]="CC-BY-3.0"
        ["BSD 3-Clause Clear"]="BSD-3-Clause-Clear"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with GPL and LGPL compatibility exception"]="MPL-2.0-with-GPL-LGPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with LGPL compatibility exception"]="MPL-2.0-with-LGPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with GPL compatibility exception"]="MPL-2.0-with-GPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with exception"]="MPL-2.0-with-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) or later"]="MPL-2.0-or-later"
        ["Mozilla Public License 1.0 or later"]="MPL-1.0-or-later"
        ["Mozilla Public License 1.1 or later"]="MPL-1.1-or-later"
        ["GNU Lesser General Public License v3.0 or later"]="LGPL-3.0-or-later"
        ["GNU Lesser General Public License v2.1 or later"]="LGPL-2.1-or-later"
        ["GNU Lesser General Public License v2.0 or later"]="LGPL-2.0-or-later"
        ["GNU General Public License v3.0 or later"]="GPL-3.0-or-later"
        ["GNU General Public License v2.0 or later"]="GPL-2.0-or-later"
        ["GNU General Public License v1.0 or later"]="GPL-1.0-or-later"
        ["GNU Affero General Public License v2.0 or later"]="AGPL-2.0-or-later"
        ["GNU Affero General Public License v1.0 or later"]="AGPL-1.0-or-later"
        ["Eclipse Public License 1.0"]="EPL-1.0"
        ["Creative Commons Attribution-NonCommercial-NoDerivatives 3.0 Unported"]="CC-BY-NC-ND-3.0"
        ["Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported"]="CC-BY-NC-SA-3.0"
        ["Creative Commons Attribution-NoDerivatives 3.0 Unported"]="CC-BY-ND-3.0"
        ["Creative Commons Attribution-NonCommercial 3.0 Unported"]="CC-BY-NC-3.0"
        ["Creative Commons Attribution-ShareAlike 3.0 Unported"]="CC-BY-SA-3.0"
        ["Creative Commons Attribution 3.0 Unported"]="CC-BY-3.0"
        ["BSD 3-Clause Clear"]="BSD-3-Clause-Clear"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with GPL and LGPL compatibility exception"]="MPL-2.0-with-GPL-LGPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with LGPL compatibility exception"]="MPL-2.0-with-LGPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with GPL compatibility exception"]="MPL-2.0-with-GPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with exception"]="MPL-2.0-with-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) or later"]="MPL-2.0-or-later"
        ["Mozilla Public License 1.0 or later"]="MPL-1.0-or-later"
        ["Mozilla Public License 1.1 or later"]="MPL-1.1-or-later"
        ["GNU Lesser General Public License v3.0 or later"]="LGPL-3.0-or-later"
        ["GNU Lesser General Public License v2.1 or later"]="LGPL-2.1-or-later"
        ["GNU Lesser General Public License v2.0 or later"]="LGPL-2.0-or-later"
        ["GNU General Public License v3.0 or later"]="GPL-3.0-or-later"
        ["GNU General Public License v2.0 or later"]="GPL-2.0-or-later"
        ["GNU General Public License v1.0 or later"]="GPL-1.0-or-later"
        ["GNU Affero General Public License v2.0 or later"]="AGPL-2.0-or-later"
        ["GNU Affero General Public License v1.0 or later"]="AGPL-1.0-or-later"
        ["Eclipse Public License 1.0"]="EPL-1.0"
        ["Creative Commons Attribution-NonCommercial-NoDerivatives 3.0 Unported"]="CC-BY-NC-ND-3.0"
        ["Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported"]="CC-BY-NC-SA-3.0"
        ["Creative Commons Attribution-NoDerivatives 3.0 Unported"]="CC-BY-ND-3.0"
        ["Creative Commons Attribution-NonCommercial 3.0 Unported"]="CC-BY-NC-3.0"
        ["Creative Commons Attribution-ShareAlike 3.0 Unported"]="CC-BY-SA-3.0"
        ["Creative Commons Attribution 3.0 Unported"]="CC-BY-3.0"
        ["BSD 3-Clause Clear"]="BSD-3-Clause-Clear"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with GPL and LGPL compatibility exception"]="MPL-2.0-with-GPL-LGPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with LGPL compatibility exception"]="MPL-2.0-with-LGPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with GPL compatibility exception"]="MPL-2.0-with-GPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with exception"]="MPL-2.0-with-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) or later"]="MPL-2.0-or-later"
        ["Mozilla Public License 1.0 or later"]="MPL-1.0-or-later"
        ["Mozilla Public License 1.1 or later"]="MPL-1.1-or-later"
        ["GNU Lesser General Public License v3.0 or later"]="LGPL-3.0-or-later"
        ["GNU Lesser General Public License v2.1 or later"]="LGPL-2.1-or-later"
        ["GNU Lesser General Public License v2.0 or later"]="LGPL-2.0-or-later"
        ["GNU General Public License v3.0 or later"]="GPL-3.0-or-later"
        ["GNU General Public License v2.0 or later"]="GPL-2.0-or-later"
        ["GNU General Public License v1.0 or later"]="GPL-1.0-or-later"
        ["GNU Affero General Public License v2.0 or later"]="AGPL-2.0-or-later"
        ["GNU Affero General Public License v1.0 or later"]="AGPL-1.0-or-later"
        ["Eclipse Public License 1.0"]="EPL-1.0"
        ["Creative Commons Attribution-NonCommercial-NoDerivatives 3.0 Unported"]="CC-BY-NC-ND-3.0"
        ["Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported"]="CC-BY-NC-SA-3.0"
        ["Creative Commons Attribution-NoDerivatives 3.0 Unported"]="CC-BY-ND-3.0"
        ["Creative Commons Attribution-NonCommercial 3.0 Unported"]="CC-BY-NC-3.0"
        ["Creative Commons Attribution-ShareAlike 3.0 Unported"]="CC-BY-SA-3.0"
        ["Creative Commons Attribution 3.0 Unported"]="CC-BY-3.0"
        ["BSD 3-Clause Clear"]="BSD-3-Clause-Clear"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with GPL and LGPL compatibility exception"]="MPL-2.0-with-GPL-LGPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with LGPL compatibility exception"]="MPL-2.0-with-LGPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with GPL compatibility exception"]="MPL-2.0-with-GPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with exception"]="MPL-2.0-with-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) or later"]="MPL-2.0-or-later"
        ["Mozilla Public License 1.0 or later"]="MPL-1.0-or-later"
        ["Mozilla Public License 1.1 or later"]="MPL-1.1-or-later"
        ["GNU Lesser General Public License v3.0 or later"]="LGPL-3.0-or-later"
        ["GNU Lesser General Public License v2.1 or later"]="LGPL-2.1-or-later"
        ["GNU Lesser General Public License v2.0 or later"]="LGPL-2.0-or-later"
        ["GNU General Public License v3.0 or later"]="GPL-3.0-or-later"
        ["GNU General Public License v2.0 or later"]="GPL-2.0-or-later"
        ["GNU General Public License v1.0 or later"]="GPL-1.0-or-later"
        ["GNU Affero General Public License v2.0 or later"]="AGPL-2.0-or-later"
        ["GNU Affero General Public License v1.0 or later"]="AGPL-1.0-or-later"
        ["Eclipse Public License 1.0"]="EPL-1.0"
        ["Creative Commons Attribution-NonCommercial-NoDerivatives 3.0 Unported"]="CC-BY-NC-ND-3.0"
        ["Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported"]="CC-BY-NC-SA-3.0"
        ["Creative Commons Attribution-NoDerivatives 3.0 Unported"]="CC-BY-ND-3.0"
        ["Creative Commons Attribution-NonCommercial 3.0 Unported"]="CC-BY-NC-3.0"
        ["Creative Commons Attribution-ShareAlike 3.0 Unported"]="CC-BY-SA-3.0"
        ["Creative Commons Attribution 3.0 Unported"]="CC-BY-3.0"
        ["BSD 3-Clause Clear"]="BSD-3-Clause-Clear"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with GPL and LGPL compatibility exception"]="MPL-2.0-with-GPL-LGPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with LGPL compatibility exception"]="MPL-2.0-with-LGPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with GPL compatibility exception"]="MPL-2.0-with-GPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with exception"]="MPL-2.0-with-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) or later"]="MPL-2.0-or-later"
        ["Mozilla Public License 1.0 or later"]="MPL-1.0-or-later"
        ["Mozilla Public License 1.1 or later"]="MPL-1.1-or-later"
        ["GNU Lesser General Public License v3.0 or later"]="LGPL-3.0-or-later"
        ["GNU Lesser General Public License v2.1 or later"]="LGPL-2.1-or-later"
        ["GNU Lesser General Public License v2.0 or later"]="LGPL-2.0-or-later"
        ["GNU General Public License v3.0 or later"]="GPL-3.0-or-later"
        ["GNU General Public License v2.0 or later"]="GPL-2.0-or-later"
        ["GNU General Public License v1.0 or later"]="GPL-1.0-or-later"
        ["GNU Affero General Public License v2.0 or later"]="AGPL-2.0-or-later"
        ["GNU Affero General Public License v1.0 or later"]="AGPL-1.0-or-later"
        ["Eclipse Public License 1.0"]="EPL-1.0"
        ["Creative Commons Attribution-NonCommercial-NoDerivatives 3.0 Unported"]="CC-BY-NC-ND-3.0"
        ["Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported"]="CC-BY-NC-SA-3.0"
        ["Creative Commons Attribution-NoDerivatives 3.0 Unported"]="CC-BY-ND-3.0"
        ["Creative Commons Attribution-NonCommercial 3.0 Unported"]="CC-BY-NC-3.0"
        ["Creative Commons Attribution-ShareAlike 3.0 Unported"]="CC-BY-SA-3.0"
        ["Creative Commons Attribution 3.0 Unported"]="CC-BY-3.0"
        ["BSD 3-Clause Clear"]="BSD-3-Clause-Clear"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with GPL and LGPL compatibility exception"]="MPL-2.0-with-GPL-LGPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with LGPL compatibility exception"]="MPL-2.0-with-LGPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with GPL compatibility exception"]="MPL-2.0-with-GPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with exception"]="MPL-2.0-with-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) or later"]="MPL-2.0-or-later"
        ["Mozilla Public License 1.0 or later"]="MPL-1.0-or-later"
        ["Mozilla Public License 1.1 or later"]="MPL-1.1-or-later"
        ["GNU Lesser General Public License v3.0 or later"]="LGPL-3.0-or-later"
        ["GNU Lesser General Public License v2.1 or later"]="LGPL-2.1-or-later"
        ["GNU Lesser General Public License v2.0 or later"]="LGPL-2.0-or-later"
        ["GNU General Public License v3.0 or later"]="GPL-3.0-or-later"
        ["GNU General Public License v2.0 or later"]="GPL-2.0-or-later"
        ["GNU General Public License v1.0 or later"]="GPL-1.0-or-later"
        ["GNU Affero General Public License v2.0 or later"]="AGPL-2.0-or-later"
        ["GNU Affero General Public License v1.0 or later"]="AGPL-1.0-or-later"
        ["Eclipse Public License 1.0"]="EPL-1.0"
        ["Creative Commons Attribution-NonCommercial-NoDerivatives 3.0 Unported"]="CC-BY-NC-ND-3.0"
        ["Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported"]="CC-BY-NC-SA-3.0"
        ["Creative Commons Attribution-NoDerivatives 3.0 Unported"]="CC-BY-ND-3.0"
        ["Creative Commons Attribution-NonCommercial 3.0 Unported"]="CC-BY-NC-3.0"
        ["Creative Commons Attribution-ShareAlike 3.0 Unported"]="CC-BY-SA-3.0"
        ["Creative Commons Attribution 3.0 Unported"]="CC-BY-3.0"
        ["BSD 3-Clause Clear"]="BSD-3-Clause-Clear"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with GPL and LGPL compatibility exception"]="MPL-2.0-with-GPL-LGPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with LGPL compatibility exception"]="MPL-2.0-with-LGPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with GPL compatibility exception"]="MPL-2.0-with-GPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with exception"]="MPL-2.0-with-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) or later"]="MPL-2.0-or-later"
        ["Mozilla Public License 1.0 or later"]="MPL-1.0-or-later"
        ["Mozilla Public License 1.1 or later"]="MPL-1.1-or-later"
        ["GNU Lesser General Public License v3.0 or later"]="LGPL-3.0-or-later"
        ["GNU Lesser General Public License v2.1 or later"]="LGPL-2.1-or-later"
        ["GNU Lesser General Public License v2.0 or later"]="LGPL-2.0-or-later"
        ["GNU General Public License v3.0 or later"]="GPL-3.0-or-later"
        ["GNU General Public License v2.0 or later"]="GPL-2.0-or-later"
        ["GNU General Public License v1.0 or later"]="GPL-1.0-or-later"
        ["GNU Affero General Public License v2.0 or later"]="AGPL-2.0-or-later"
        ["GNU Affero General Public License v1.0 or later"]="AGPL-1.0-or-later"
        ["Eclipse Public License 1.0"]="EPL-1.0"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with exception"]="MPL-2.0-with-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with GPL compatibility exception"]="MPL-2.0-with-GPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with LGPL compatibility exception"]="MPL-2.0-with-LGPL-exception"
        ["Mozilla Public License 2.0 (Mozilla-2.0) with GPL and LGPL compatibility exception"]="MPL-2.0-with-GPL-LGPL-exception"
        ["BSD 3-Clause Clear"]="BSD-3-Clause-Clear"
        ["Creative Commons Attribution 3.0 Unported"]="CC-BY-3.0"
        ["Creative Commons Attribution-ShareAlike 3.0 Unported"]="CC-BY-SA-3.0"
        ["Creative Commons Attribution-NonCommercial 3.0 Unported"]="CC-BY-NC-3.0"
        ["Creative Commons Attribution-NoDerivatives 3.0 Unported"]="CC-BY-ND-3.0"
        ["Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported"]="CC-BY-NC-SA-3.0"
        ["Creative Commons Attribution-NonCommercial-NoDerivatives 3.0 Unported"]="CC-BY-NC-ND-3.0"
        ["Eclipse Public License 1.0"]="EPL-1.0"
        ["MIT License with employer disclaimer"]="MIT"
        ["MIT License with no attribution"]="MIT-0"
        ["MIT License with no warranty"]="MIT-NW"
        ["MIT License"]="MIT"
    )

# Function to determine the license family from a license text file
determine_license_family() {
    for license in "${!licenses[@]}"; do
        if grep -q "$license" "$1"; then
            echo "${licenses[$license]}"
            return
        fi
    done
    echo ""
}

# Takes the github url and returns the license family

extract_owner_from_github_url() {
    echo $1 | awk -F'/' '{print $4}'
}

extract_repo_from_github_url() {
    echo $1 | awk -F'/' '{print $5}'
}

determine_license_family_from_github() {
    owner=$(extract_owner_from_github_url $1)
    repo=$(extract_repo_from_github_url $1)
    license=$(curl -s "https://api.github.com/repos/$owner/$repo/license" | jq -r '.license.spdx_id')
    echo $license
}

determine_license_url_from_github() {
    owner=$(extract_owner_from_github_url $1)
    repo=$(extract_repo_from_github_url $1)
    license=$(curl -s "https://api.github.com/repos/$owner/$repo/license" | jq -r '.download_url')
    
    #use the license url as fallback if the license file is not found
    if [ "$license" == "null" ]; then
        license=$(curl -s "https://api.github.com/repos/$owner/$repo/license" | jq -r '.url')
    fi

    if [ "$license" == "null" ]; then
        license=""
    fi

    echo $license
}

check_for_cve() {
    cve=$(curl -s -d "{\"commit\": \"$1\"}" "https://api.osv.dev/v1/query" | jq -r '.aliases[0].id')
    if [ "$cve" == "null" ]; then
        echo ""
    else
        echo "$cve"
    fi
}

# Start of the table
echo "{| class=\"wikitable sortable\""
echo "!Library"
echo "!Version"
echo "!License Family"
echo "!License URL"
echo "!License Copy"
echo "!Product URL"
echo "!Used&nbsp;by"

# Find all vcpkg.spdx.json files in the current directory and its subdirectories
find . -name "vcpkg.spdx.json" | while read file; do
    # Extract the name, license family, and download location fields
    name=$(jq -r '.name' "$file")

    #Extract the git commit hash from the name
    commit_hash=$(echo $name | awk -F' ' '{print $NF}')

    #skip the vcpkg package if the name starts with "vcpkg"
    if [[ "$name" == vcpkg* ]]; then
        continue
    fi

    license_family=$(jq -r '.packages[0].licenseConcluded' "$file")
    download_location=$(jq -r '.packages[0].downloadLocation' "$file")
    homepage=$(jq -r '.packages[0].homepage' "$file")
   
    cve=$(check_for_cve "$commit_hash")
    # Add a warning to the version if a cve is found
    if [ "$cve" != "" ]; then
        version="(Warning: CVE: $cve) $version"
    fi


    # If the download location is empty or contains "NOASSERTION", use the homepage
    if [ "$download_location" == "" ] || [ "$download_location" == "NOASSERTION" ]; then
        download_location=$homepage
    fi


    download_location=$(check_field "$download_location")
    name=$(check_field "$name")

    license_family=$(check_field "$license_family")

   
    # If the download location appears to be a git repository, try to identify the license file
    license_url=""
    if [[ "$download_location" == git@* || "$download_location" == https://github.com/* ]]; then
        # Clone the repository to a temporary directory
        temp_dir=$(mktemp -d)
        git clone --depth 1 "$download_location" "$temp_dir"

        # Try to find a license file and construct its URL
        for license_file in "${license_files[@]}"; do
            if [ -f "$temp_dir/$license_file" ]; then
                # Determine the license family from the license file if it is not already set
                if [ "$license_family" == "" ] || [ "$license_family" == "NOASSERTION" ]; then
                    license_family=$(determine_license_family "$temp_dir/$license_file")
                fi

                if [ "$license_family" == "" ]; then
                    license_family=$(determine_license_family_from_github "$download_location")
                fi

                # Construct the URL of the license file
                license_url="$download_location/blob/master/$license_file"
                break
            fi
        done

        license_url=$(check_field "$license_url")
        if [ "$license_url" == "" ]; then
            license_url=$(determine_license_url_from_github "$download_location")
        fi

        # Remove the temporary directory
        rm -rf "$temp_dir"
    fi

    # Split the name into application name and version
    IFS='@' read -ra parts <<< "$name"
    app_name_with_platform=${parts[0]}
    version=${parts[1]}

    # Remove the platform name from the application name
    IFS=':' read -ra parts <<< "$app_name_with_platform"
    app_name=${parts[0]}

    # Print the application name, version, license family, and download location in the MediaWiki table format
    echo "|-"
    echo "|<!-- Library -->$app_name"
    echo "|<!-- Version -->$version"
    echo "|<!-- License Family -->$license_family"  # Add the License Family here
    echo "|<!-- License URL --><small>$license_url</small>"  # Add the License URL here
    echo "|<!-- License Copy --><small></small>"  # Add the License Copy here
    echo "|<!-- Product URL --><small>$download_location</small>"  # Add the Product URL here
    echo "|<!-- Used by -->Gui + API (Gladius)"  # Add the Used by information here
    
done



# Find all git submodules in the current directory and its subdirectories
git submodule status | while read submodule; do
    # Extract the commit hash and path
    commit_hash=$(echo $submodule | awk '{print $1}')
    path=$(echo $submodule | awk '{print $2}')

    # Extract the repository name from the path
    IFS='/' read -ra parts <<< "$path"
    repo_name=${parts[-1]}

    # Extract the URL of the origin
    origin_url=$(git -C $path config --get remote.origin.url)

    # Extract the tag (if available)
    version=$(git -C $path describe --tags --always)

    # Try to find a license file and construct its URL
    license_url=""
    for license_file in "${license_files[@]}"; do
        if [ -f "$path/$license_file" ]; then
            
            # Construct the URL of the license file
            license_family=$(determine_license_family "$path/$license_file")

            if [ "$license_family" == "" ]; then
                license_family=$(determine_license_family_from_github "$download_location")
            fi

            #remove the .git from the url
            origin_url_without_git=$(echo $origin_url | sed 's/\.git//g')
            license_url="$origin_url_without_git/blob/$commit_hash/$license_file"
            break
        fi
    done

    license_url=$(check_field "$license_url")
    if [ "$license_url" == "" ]; then
        license_url=$(determine_license_url_from_github "$download_location")
    fi

    cve=$(check_for_cve "$commit_hash")
    # Add a warning to the version if a cve is found
    if [ "$cve" != "" ]; then
        version="(Warning: CVE: $cve) $version"
    fi

    # Check if the fields contain "null" or "NOASSERTION" and replace it with ""
    repo_name=$(check_field "$repo_name")
    version=$(check_field "$version")
  
    license_family=$(check_field "$license_family")

    # Print the repository name, version, and license URL in the MediaWiki table format
    echo "|-"
    echo "|<!-- Library -->$repo_name"
    echo "|<!-- Version -->$version"
    echo "|<!-- License Family --><small>$license_family</small>"  # Add the License Family here
    echo "|<!-- License URL --><small>$license_url</small>"  # Add the License URL here
    echo "|<!-- License Copy --><small></small>"  # Add the License Copy here
    echo "|<!-- Product URL --><small>$origin_url</small>"  # Add the Product URL here
    echo "|<!-- Used by -->Gui + API (Gladius)"  # Add the Used by information here
done


# End of the table
echo "|}"