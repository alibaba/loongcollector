#!/usr/bin/env bash
#-------------------------------------------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License. See https://go.microsoft.com/fwlink/?linkid=2090316 for license information.
#-------------------------------------------------------------------------------------------------------------
#
# Docs: https://github.com/microsoft/vscode-dev-containers/blob/main/script-library/docs/docker-in-docker.md
# Maintainer: The Dev Container spec maintainers


DOCKER_VERSION="${VERSION:-"latest"}" # The Docker/Moby Engine + CLI should match in version
USE_MOBY="${MOBY:-"true"}"
MOBY_BUILDX_VERSION="${MOBYBUILDXVERSION:-"latest"}"
DOCKER_DASH_COMPOSE_VERSION="${DOCKERDASHCOMPOSEVERSION:-"v2"}" #v1, v2 or none
AZURE_DNS_AUTO_DETECTION="${AZUREDNSAUTODETECTION:-"true"}"
DOCKER_DEFAULT_ADDRESS_POOL="${DOCKERDEFAULTADDRESSPOOL:-""}"
USERNAME="${USERNAME:-"${_REMOTE_USER:-"automatic"}"}"
INSTALL_DOCKER_BUILDX="${INSTALLDOCKERBUILDX:-"true"}"
INSTALL_DOCKER_COMPOSE_SWITCH="${INSTALLDOCKERCOMPOSESWITCH:-"false"}"
MICROSOFT_GPG_KEYS_URI="https://packages.microsoft.com/keys/microsoft.asc"
MICROSOFT_GPG_KEYS_ROLLING_URI="https://packages.microsoft.com/keys/microsoft-rolling.asc"
DOCKER_MOBY_ARCHIVE_VERSION_CODENAMES="trixie bookworm buster bullseye bionic focal jammy noble"
DOCKER_LICENSED_ARCHIVE_VERSION_CODENAMES="trixie bookworm buster bullseye bionic focal hirsute impish jammy noble"
DISABLE_IP6_TABLES="${DISABLEIP6TABLES:-false}"

# Default: Exit on any failure.
set -e

# Clean up
rm -rf /var/lib/apt/lists/*

# Setup STDERR.
err() {
    echo "(!) $*" >&2
}

if [ "$(id -u)" -ne 0 ]; then
    err 'Script must be run as root. Use sudo, su, or add "USER root" to your Dockerfile before running this script.'
    exit 1
fi

###################
# Helper Functions
# See: https://github.com/microsoft/vscode-dev-containers/blob/main/script-library/shared/utils.sh
###################

# Determine the appropriate non-root user
if [ "${USERNAME}" = "auto" ] || [ "${USERNAME}" = "automatic" ]; then
    USERNAME=""
    POSSIBLE_USERS=("vscode" "node" "codespace" "$(awk -v val=1000 -F ":" '$3==val{print $1}' /etc/passwd)")
    for CURRENT_USER in "${POSSIBLE_USERS[@]}"; do
        if id -u ${CURRENT_USER} > /dev/null 2>&1; then
            USERNAME=${CURRENT_USER}
            break
        fi
    done
    if [ "${USERNAME}" = "" ]; then
        USERNAME=root
    fi
elif [ "${USERNAME}" = "none" ] || ! id -u ${USERNAME} > /dev/null 2>&1; then
    USERNAME=root
fi

# Package manager update function
pkg_mgr_update() {
    case ${ADJUSTED_ID} in
        debian)
            if [ "$(find /var/lib/apt/lists/* | wc -l)" = "0" ]; then
                echo "Running apt-get update..."
                apt-get update -y
            fi
            ;;
        rhel)
            if [ ${PKG_MGR_CMD} = "microdnf" ]; then
                cache_check_dir="/var/cache/yum"
            else
                cache_check_dir="/var/cache/${PKG_MGR_CMD}"
            fi
            if [ "$(ls ${cache_check_dir}/* 2>/dev/null | wc -l)" = 0 ]; then
                echo "Running ${PKG_MGR_CMD} makecache ..."
                ${PKG_MGR_CMD} makecache
            fi
            ;;
    esac
}

# Checks if packages are installed and installs them if not
check_packages() {
    case ${ADJUSTED_ID} in
        debian)
            if ! dpkg -s "$@" > /dev/null 2>&1; then
                pkg_mgr_update
                apt-get -y install --no-install-recommends "$@"
            fi
            ;;
        rhel)
            if ! rpm -q "$@" > /dev/null 2>&1; then
                pkg_mgr_update
                ${PKG_MGR_CMD} -y install "$@"
            fi
            ;;
    esac
}

# Figure out correct version of a three part version number is not passed
find_version_from_git_tags() {
    local variable_name=$1
    local requested_version=${!variable_name}
    if [ "${requested_version}" = "none" ]; then return; fi
    local repository=$2
    local prefix=${3:-"tags/v"}
    local separator=${4:-"."}
    local last_part_optional=${5:-"false"}
    if [ "$(echo "${requested_version}" | grep -o "." | wc -l)" != "2" ]; then
        local escaped_separator=${separator//./\\.}
        local last_part
        if [ "${last_part_optional}" = "true" ]; then
            last_part="(${escaped_separator}[0-9]+)?"
        else
            last_part="${escaped_separator}[0-9]+"
        fi
        local regex="${prefix}\\K[0-9]+${escaped_separator}[0-9]+${last_part}$"
        local version_list="$(git ls-remote --tags ${repository} | grep -oP "${regex}" | tr -d ' ' | tr "${separator}" "." | sort -rV)"
        if [ "${requested_version}" = "latest" ] || [ "${requested_version}" = "current" ] || [ "${requested_version}" = "lts" ]; then
            declare -g ${variable_name}="$(echo "${version_list}" | head -n 1)"
        else
            set +e
                declare -g ${variable_name}="$(echo "${version_list}" | grep -E -m 1 "^${requested_version//./\\.}([\\.\\s]|$)")"
            set -e
        fi
    fi
    if [ -z "${!variable_name}" ] || ! echo "${version_list}" | grep "^${!variable_name//./\\.}$" > /dev/null 2>&1; then
        err "Invalid ${variable_name} value: ${requested_version}\nValid values:\n${version_list}" >&2
        exit 1
    fi
    echo "${variable_name}=${!variable_name}"
}

# Use semver logic to decrement a version number then look for the closest match
find_prev_version_from_git_tags() {
    local variable_name=$1
    local current_version=${!variable_name}
    local repository=$2
    # Normally a "v" is used before the version number, but support alternate cases
    local prefix=${3:-"tags/v"}
    # Some repositories use "_" instead of "." for version number part separation, support that
    local separator=${4:-"."}
    # Some tools release versions that omit the last digit (e.g. go)
    local last_part_optional=${5:-"false"}
    # Some repositories may have tags that include a suffix (e.g. actions/node-versions)
    local version_suffix_regex=$6
    # Try one break fix version number less if we get a failure. Use "set +e" since "set -e" can cause failures in valid scenarios.
    set +e
        major="$(echo "${current_version}" | grep -oE '^[0-9]+' || echo '')"
        minor="$(echo "${current_version}" | grep -oP '^[0-9]+\.\K[0-9]+' || echo '')"
        breakfix="$(echo "${current_version}" | grep -oP '^[0-9]+\.[0-9]+\.\K[0-9]+' 2>/dev/null || echo '')"

        if [ "${minor}" = "0" ] && [ "${breakfix}" = "0" ]; then
            ((major=major-1))
            declare -g ${variable_name}="${major}"
            # Look for latest version from previous major release
            find_version_from_git_tags "${variable_name}" "${repository}" "${prefix}" "${separator}" "${last_part_optional}"
        # Handle situations like Go's odd version pattern where "0" releases omit the last part
        elif [ "${breakfix}" = "" ] || [ "${breakfix}" = "0" ]; then
            ((minor=minor-1))
            declare -g ${variable_name}="${major}.${minor}"
            # Look for latest version from previous minor release
            find_version_from_git_tags "${variable_name}" "${repository}" "${prefix}" "${separator}" "${last_part_optional}"
        else
            ((breakfix=breakfix-1))
            if [ "${breakfix}" = "0" ] && [ "${last_part_optional}" = "true" ]; then
                declare -g ${variable_name}="${major}.${minor}"
            else 
                declare -g ${variable_name}="${major}.${minor}.${breakfix}"
            fi
        fi
    set -e
}

# Function to fetch the version released prior to the latest version
get_previous_version() {
    local url=$1
    local repo_url=$2
    local variable_name=$3
    prev_version=${!variable_name}

    output=$(curl -s "$repo_url");
    if echo "$output" | jq -e 'type == "object"' > /dev/null; then
      message=$(echo "$output" | jq -r '.message')
      
      if [[ $message == "API rate limit exceeded"* ]]; then
            echo -e "\nAn attempt to find latest version using GitHub Api Failed... \nReason: ${message}"
            echo -e "\nAttempting to find latest version using GitHub tags."
            find_prev_version_from_git_tags prev_version "$url" "tags/v"
            declare -g ${variable_name}="${prev_version}"
       fi
    elif echo "$output" | jq -e 'type == "array"' > /dev/null; then 
        echo -e "\nAttempting to find latest version using GitHub Api."
        version=$(echo "$output" | jq -r '.[1].tag_name')
        declare -g ${variable_name}="${version#v}"
    fi  
    echo "${variable_name}=${!variable_name}"
}

get_github_api_repo_url() {
    local url=$1
    echo "${url/https:\/\/github.com/https:\/\/api.github.com\/repos}/releases"
}

###########################################
# Start docker-in-docker installation
###########################################

# Ensure apt is in non-interactive to avoid prompts
export DEBIAN_FRONTEND=noninteractive

# Source /etc/os-release to get OS info
. /etc/os-release

# Determine adjusted ID and package manager
if [ "${ID}" = "debian" ] || [ "${ID_LIKE}" = "debian" ]; then
    ADJUSTED_ID="debian"
    PKG_MGR_CMD="apt-get"
    # Use dpkg for Debian-based systems
    architecture="$(dpkg --print-architecture 2>/dev/null || uname -m)"
elif [[ "${ID}" = "rhel" || "${ID}" = "fedora" || "${ID}" = "azurelinux" || "${ID}" = "mariner" || "${ID_LIKE}" = *"rhel"* || "${ID_LIKE}" = *"fedora"* || "${ID_LIKE}" = *"azurelinux"* || "${ID_LIKE}" = *"mariner"* ]]; then
    ADJUSTED_ID="rhel"
    # Determine the appropriate package manager for RHEL-based systems
    for pkg_mgr in tdnf dnf microdnf yum; do
        if command -v "$pkg_mgr" >/dev/null 2>&1; then
            PKG_MGR_CMD="$pkg_mgr"
            break
        fi
    done
    
    if [ -z "${PKG_MGR_CMD}" ]; then
        err "Unable to find a supported package manager (tdnf, dnf, microdnf, yum)"
        exit 1
    fi
    
    architecture="$(rpm --eval '%{_arch}' 2>/dev/null || uname -m)"
else
    err "Linux distro ${ID} not supported."
    exit 1
fi

# Azure Linux specific setup
if [ "${ID}" = "azurelinux" ]; then
    VERSION_CODENAME="azurelinux${VERSION_ID}"
fi

# Prevent attempting to install Moby on Debian trixie (packages removed)
if [ "${USE_MOBY}" = "true" ] && [ "${ID}" = "debian" ] && [ "${VERSION_CODENAME}" = "trixie" ]; then
    err "The 'moby' option is not supported on Debian 'trixie' because 'moby-cli' and related system packages have been removed from that distribution."
    err "To continue, either set the feature option '\"moby\": false' or use a different base image (for example: 'debian:bookworm' or 'ubuntu-24.04')."
    exit 1
fi

# Check if distro is supported
if [ "${USE_MOBY}" = "true" ]; then
    if [ "${ADJUSTED_ID}" = "debian" ]; then
        if [[ "${DOCKER_MOBY_ARCHIVE_VERSION_CODENAMES}" != *"${VERSION_CODENAME}"* ]]; then
            err "Unsupported distribution version '${VERSION_CODENAME}'. To resolve, either: (1) set feature option '\"moby\": false' , or (2) choose a compatible OS distribution"
            err "Supported distributions include: ${DOCKER_MOBY_ARCHIVE_VERSION_CODENAMES}"
            exit 1
        fi
        echo "(*) ${VERSION_CODENAME} is supported for Moby installation  - setting up Microsoft repository"
    elif [ "${ADJUSTED_ID}" = "rhel" ]; then
        if [ "${ID}" = "azurelinux" ] || [ "${ID}" = "mariner" ]; then
            echo " (*) ${ID} ${VERSION_ID} detected - using Microsoft repositories for Moby packages"
        else
            echo "RHEL-based system (${ID}) detected - Moby packages may require additional configuration"
        fi
    fi
else
    if [ "${ADJUSTED_ID}" = "debian" ]; then
        if [[ "${DOCKER_LICENSED_ARCHIVE_VERSION_CODENAMES}" != *"${VERSION_CODENAME}"* ]]; then
            err "Unsupported distribution version '${VERSION_CODENAME}'. To resolve, please choose a compatible OS distribution"
            err "Supported distributions include: ${DOCKER_LICENSED_ARCHIVE_VERSION_CODENAMES}"
            exit 1
        fi
        echo "(*) ${VERSION_CODENAME} is supported for Docker CE installation (supported: ${DOCKER_LICENSED_ARCHIVE_VERSION_CODENAMES}) - setting up Docker repository"
    elif [ "${ADJUSTED_ID}" = "rhel" ]; then
        
        echo "RHEL-based system (${ID}) detected - using Docker CE packages"
    fi
fi

# Install base dependencies
base_packages="curl ca-certificates pigz iptables gnupg2 wget jq"
case ${ADJUSTED_ID} in
    debian)
        check_packages apt-transport-https $base_packages dirmngr
        ;;
    rhel)
        check_packages $base_packages tar gawk shadow-utils policycoreutils  procps-ng systemd-libs systemd-devel
       
        ;;
esac

# Install git if not already present
if ! command -v git >/dev/null 2>&1; then
    check_packages git
fi

# Update CA certificates to ensure HTTPS connections work properly
# This is especially important for Ubuntu 24.04 (Noble) and Debian Trixie
# Only run for Debian-based systems (RHEL uses update-ca-trust instead)
if [ "${ADJUSTED_ID}" = "debian" ] && command -v update-ca-certificates > /dev/null 2>&1; then
    update-ca-certificates
fi

# Swap to legacy iptables for compatibility (Debian only)
if [ "${ADJUSTED_ID}" = "debian" ] && type iptables-legacy > /dev/null 2>&1; then
    update-alternatives --set iptables /usr/sbin/iptables-legacy
    update-alternatives --set ip6tables /usr/sbin/ip6tables-legacy
fi

# Set up the necessary repositories
if [ "${USE_MOBY}" = "true" ]; then
    # Name of open source engine/cli
    engine_package_name="moby-engine"
    cli_package_name="moby-cli"

    case ${ADJUSTED_ID} in
        debian)
            # Import key safely and import Microsoft apt repo
            {
                curl -sSL ${MICROSOFT_GPG_KEYS_URI}
                curl -sSL ${MICROSOFT_GPG_KEYS_ROLLING_URI}
            } | gpg --dearmor > /usr/share/keyrings/microsoft-archive-keyring.gpg
            echo "deb [arch=${architecture} signed-by=/usr/share/keyrings/microsoft-archive-keyring.gpg] https://packages.microsoft.com/repos/microsoft-${ID}-${VERSION_CODENAME}-prod ${VERSION_CODENAME} main" > /etc/apt/sources.list.d/microsoft.list
            ;;
        rhel)
            echo "(*) ${ID} detected - checking for Moby packages..."
        
            # Check if moby packages are available in default repos
            if ${PKG_MGR_CMD} list available moby-engine >/dev/null 2>&1; then
                echo "(*) Using built-in ${ID} Moby packages"
            else
                case "${ID}" in
                    azurelinux)
                        echo "(*) Moby packages not found in Azure Linux repositories"
                        echo "(*) For Azure Linux, Docker CE ('moby': false) is recommended"
                        err "Moby packages are not available for Azure Linux ${VERSION_ID}."
                        err "Recommendation: Use '\"moby\": false' to install Docker CE instead."
                        exit 1
                        ;;
                    mariner)
                        echo "(*) Adding Microsoft repository for CBL-Mariner..."
                        # Add Microsoft repository if packages aren't available locally
                        curl -sSL ${MICROSOFT_GPG_KEYS_URI} | gpg --dearmor > /etc/pki/rpm-gpg/microsoft.gpg
                        cat > /etc/yum.repos.d/microsoft.repo << EOF
[microsoft]
name=Microsoft Repository
baseurl=https://packages.microsoft.com/repos/microsoft-cbl-mariner-2.0-prod-base/
enabled=1
gpgcheck=1
gpgkey=file:///etc/pki/rpm-gpg/microsoft.gpg
EOF
                # Verify packages are available after adding repo
                pkg_mgr_update
                if ! ${PKG_MGR_CMD} list available moby-engine >/dev/null 2>&1; then
                    echo "(*) Moby packages not found in Microsoft repository either"
                    err "Moby packages are not available for CBL-Mariner ${VERSION_ID}."
                    err "Recommendation: Use '\"moby\": false' to install Docker CE instead."
                    exit 1
                fi
                ;;
            *)
                err "Moby packages are not available for ${ID}. Please use 'moby': false option."
                exit 1
                ;;
            esac    
        fi
        ;;
    esac
else
    # Name of licensed engine/cli
    engine_package_name="docker-ce"
    cli_package_name="docker-ce-cli"
    case ${ADJUSTED_ID} in
        debian)
            curl -fsSL https://download.docker.com/linux/${ID}/gpg | gpg --dearmor > /usr/share/keyrings/docker-archive-keyring.gpg
            echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/${ID} ${VERSION_CODENAME} stable" > /etc/apt/sources.list.d/docker.list
            ;;
        rhel)
            # Docker CE repository setup for RHEL-based systems
            setup_docker_ce_repo() {
                curl -fsSL https://download.docker.com/linux/centos/gpg > /etc/pki/rpm-gpg/docker-ce.gpg
                cat > /etc/yum.repos.d/docker-ce.repo << EOF  
[docker-ce-stable]
name=Docker CE Stable
baseurl=https://download.docker.com/linux/centos/9/\$basearch/stable
enabled=1
gpgcheck=1
gpgkey=file:///etc/pki/rpm-gpg/docker-ce.gpg
skip_if_unavailable=1
module_hotfixes=1
EOF
            }
            install_azure_linux_deps() {
                echo "(*) Installing device-mapper libraries for Docker CE..."
                [ "${ID}" != "mariner" ] && ${PKG_MGR_CMD} -y install device-mapper-libs 2>/dev/null || echo "(*) Device-mapper install failed, proceeding"   
                echo "(*) Installing additional Docker CE dependencies..."
                ${PKG_MGR_CMD} -y install libseccomp libtool-ltdl systemd-libs libcgroup tar xz || {
                    echo "(*) Some optional dependencies could not be installed, continuing..."
                }
            }
            setup_selinux_context() {
                if command -v getenforce >/dev/null 2>&1 && [ "$(getenforce 2>/dev/null)" != "Disabled" ]; then
                    echo "(*) Creating minimal SELinux context for Docker compatibility..."
                    mkdir -p /etc/selinux/targeted/contexts/files/ 2>/dev/null || true
                    echo "/var/lib/docker(/.*)? system_u:object_r:container_file_t:s0" >> /etc/selinux/targeted/contexts/files/file_contexts.local 2>/dev/null || true
                fi
            }
            
            # Special handling for RHEL Docker CE installation
            case "${ID}" in
                azurelinux|mariner)
                    echo "(*) ${ID} detected"
                    echo "(*) Note: Moby packages work better on Azure Linux. Consider using 'moby': true"
                    echo "(*) Setting up Docker CE repository..."
                    
                    setup_docker_ce_repo
                    install_azure_linux_deps
                    
                    if [ "${USE_MOBY}" != "true" ]; then
                        echo "(*) Docker CE installation for Azure Linux - skipping container-selinux"
                        echo "(*) Note: SELinux policies will be minimal but Docker will function normally"
                        setup_selinux_context
                    else
                        echo "(*) Using Moby - container-selinux not required"
                    fi
                    ;;
                *)     
                    # Standard RHEL/CentOS/Fedora approach
                    if command -v dnf >/dev/null 2>&1; then
                        dnf config-manager --add-repo https://mirrors.aliyun.com/docker-ce/linux/centos/docker-ce.repo
                    elif command -v yum-config-manager >/dev/null 2>&1; then
                        yum-config-manager --add-repo https://mirrors.aliyun.com/docker-ce/linux/centos/docker-ce.repo
                    else  
                        # Manual fallback
                        setup_docker_ce_repo
            fi
            ;;
        esac
        ;;
    esac    
fi

# Refresh package database
case ${ADJUSTED_ID} in
    debian)
        apt-get update
        ;;
    rhel)
        pkg_mgr_update
        ;;
esac

# Soft version matching
if [ "${DOCKER_VERSION}" = "latest" ] || [ "${DOCKER_VERSION}" = "lts" ] || [ "${DOCKER_VERSION}" = "stable" ]; then
    # Empty, meaning grab whatever "latest" is in apt repo
    engine_version_suffix=""
    cli_version_suffix=""
else
    case ${ADJUSTED_ID} in
        debian)
    # Fetch a valid version from the apt-cache (eg: the Microsoft repo appends +azure, breakfix, etc...)
    docker_version_dot_escaped="${DOCKER_VERSION//./\\.}"
    docker_version_dot_plus_escaped="${docker_version_dot_escaped//+/\\+}"
    # Regex needs to handle debian package version number format: https://www.systutorials.com/docs/linux/man/5-deb-version/
    docker_version_regex="^(.+:)?${docker_version_dot_plus_escaped}([\\.\\+ ~:-]|$)"
    set +e # Don't exit if finding version fails - will handle gracefully
        cli_version_suffix="=$(apt-cache madison ${cli_package_name} | awk -F"|" '{print $2}' | sed -e 's/^[ \t]*//' | grep -E -m 1 "${docker_version_regex}")"
        engine_version_suffix="=$(apt-cache madison ${engine_package_name} | awk -F"|" '{print $2}' | sed -e 's/^[ \t]*//' | grep -E -m 1 "${docker_version_regex}")"
    set -e
    if [ -z "${engine_version_suffix}" ] || [ "${engine_version_suffix}" = "=" ] || [ -z "${cli_version_suffix}" ] || [ "${cli_version_suffix}" = "=" ] ; then
        err "No full or partial Docker / Moby version match found for \"${DOCKER_VERSION}\" on OS ${ID} ${VERSION_CODENAME} (${architecture}). Available versions:"
        apt-cache madison ${cli_package_name} | awk -F"|" '{print $2}' | grep -oP '^(.+:)?\K.+'
        exit 1
    fi
    ;;  
rhel)
     # For RHEL-based systems, use dnf/yum to find versions
            docker_version_escaped="${DOCKER_VERSION//./\\.}"
            set +e # Don't exit if finding version fails - will handle gracefully
                if [ "${USE_MOBY}" = "true" ]; then
                    available_versions=$(${PKG_MGR_CMD} list --available moby-engine 2>/dev/null | grep -v "Available Packages" | awk '{print $2}' | grep -E "^${docker_version_escaped}" | head -1)
                else
                    available_versions=$(${PKG_MGR_CMD} list --available docker-ce 2>/dev/null | grep -v "Available Packages" | awk '{print $2}' | grep -E "^${docker_version_escaped}" | head -1)
                fi
            set -e
            if [ -n "${available_versions}" ]; then
                engine_version_suffix="-${available_versions}"
                cli_version_suffix="-${available_versions}"
            else
                echo "(*) Exact version ${DOCKER_VERSION} not found, using latest available"
                engine_version_suffix=""
                cli_version_suffix=""
            fi
            ;;
    esac
fi

# Version matching for moby-buildx
if [ "${USE_MOBY}" = "true" ]; then
    if [ "${MOBY_BUILDX_VERSION}" = "latest" ]; then
        # Empty, meaning grab whatever "latest" is in apt repo
        buildx_version_suffix=""
    else
        case ${ADJUSTED_ID} in
            debian)
        buildx_version_dot_escaped="${MOBY_BUILDX_VERSION//./\\.}"
        buildx_version_dot_plus_escaped="${buildx_version_dot_escaped//+/\\+}"
        buildx_version_regex="^(.+:)?${buildx_version_dot_plus_escaped}([\\.\\+ ~:-]|$)"
        set +e
            buildx_version_suffix="=$(apt-cache madison moby-buildx | awk -F"|" '{print $2}' | sed -e 's/^[ \t]*//' | grep -E -m 1 "${buildx_version_regex}")"
        set -e
        if [ -z "${buildx_version_suffix}" ] || [ "${buildx_version_suffix}" = "=" ]; then
            err "No full or partial moby-buildx version match found for \"${MOBY_BUILDX_VERSION}\" on OS ${ID} ${VERSION_CODENAME} (${architecture}). Available versions:"
            apt-cache madison moby-buildx | awk -F"|" '{print $2}' | grep -oP '^(.+:)?\K.+'
            exit 1
        fi
        ;;
            rhel)
                # For RHEL-based systems, try to find buildx version or use latest
                buildx_version_escaped="${MOBY_BUILDX_VERSION//./\\.}"
                set +e
                available_buildx=$(${PKG_MGR_CMD} list --available moby-buildx 2>/dev/null | grep -v "Available Packages" | awk '{print $2}' | grep -E "^${buildx_version_escaped}" | head -1)
                set -e
                if [ -n "${available_buildx}" ]; then
                    buildx_version_suffix="-${available_buildx}"
                else
                    echo "(*) Exact buildx version ${MOBY_BUILDX_VERSION} not found, using latest available"
                    buildx_version_suffix=""
                fi
                ;;
        esac
        echo "buildx_version_suffix ${buildx_version_suffix}"
    fi
fi

# Install Docker / Moby CLI if not already installed
if type docker > /dev/null 2>&1 && type dockerd > /dev/null 2>&1; then
    echo "Docker / Moby CLI and Engine already installed."
else
        case ${ADJUSTED_ID} in
        debian)
            if [ "${USE_MOBY}" = "true" ]; then
                # Install engine
                set +e # Handle error gracefully
                    apt-get -y install --no-install-recommends moby-cli${cli_version_suffix} moby-buildx${buildx_version_suffix} moby-engine${engine_version_suffix}
                    exit_code=$?
                set -e    
                
                if [ ${exit_code} -ne 0 ]; then
                    err "Packages for moby not available in OS ${ID} ${VERSION_CODENAME} (${architecture}). To resolve, either: (1) set feature option '\"moby\": false' , or (2) choose a compatible OS version (eg: 'ubuntu-24.04')."
                    exit 1
                fi

                # Install compose
                apt-get -y install --no-install-recommends moby-compose || err "Package moby-compose (Docker Compose v2) not available for OS ${ID} ${VERSION_CODENAME} (${architecture}). Skipping."
            else
                apt-get -y install --no-install-recommends docker-ce-cli${cli_version_suffix} docker-ce${engine_version_suffix}
                # Install compose
                apt-mark hold docker-ce docker-ce-cli
                apt-get -y install --no-install-recommends docker-compose-plugin || echo "(*) Package docker-compose-plugin (Docker Compose v2) not available for OS ${ID} ${VERSION_CODENAME} (${architecture}). Skipping."
            fi
            ;;
        rhel)
            if [ "${USE_MOBY}" = "true" ]; then
                set +e # Handle error gracefully
                    ${PKG_MGR_CMD} -y install moby-cli${cli_version_suffix} moby-engine${engine_version_suffix}
                    exit_code=$?
                set -e
                
                if [ ${exit_code} -ne 0 ]; then
                    err "Packages for moby not available in OS ${ID} ${VERSION_CODENAME} (${architecture}). To resolve, either: (1) set feature option '\"moby\": false' , or (2) choose a compatible OS version."
                    exit 1
                fi

                # Install compose
                if [ "${DOCKER_DASH_COMPOSE_VERSION}" != "none" ]; then
                    ${PKG_MGR_CMD} -y install moby-compose || echo "(*) Package moby-compose not available for ${ID} ${VERSION_CODENAME} (${architecture}). Skipping."
                fi
            else
                               # Special handling for Azure Linux Docker CE installation
                if [ "${ID}" = "azurelinux" ] || [ "${ID}" = "mariner" ]; then
                    echo "(*) Installing Docker CE on Azure Linux (bypassing container-selinux dependency)..."
                    
                    # Use rpm with --force and --nodeps for Azure Linux
                    set +e  # Don't exit on error for this section
                    ${PKG_MGR_CMD} -y install docker-ce${cli_version_suffix} docker-ce-cli${engine_version_suffix} containerd.io
                    install_result=$?
                    set -e
                    
                    if [ $install_result -ne 0 ]; then
                        echo "(*) Standard installation failed, trying manual installation..."
                        
                        echo "(*) Standard installation failed, trying manual installation..."
                        
                        # Create directory for downloading packages
                        mkdir -p /tmp/docker-ce-install
                        
                        # Download packages manually using curl since tdnf doesn't support download
                        echo "(*) Downloading Docker CE packages manually..."
                        
                        # Get the repository baseurl
                        repo_baseurl="https://download.docker.com/linux/centos/9/x86_64/stable"
                        
                        # Download packages directly
                        cd /tmp/docker-ce-install
                        
                        # Get package names with versions
                        if [ -n "${cli_version_suffix}" ]; then
                            docker_ce_version="${cli_version_suffix#-}"
                            docker_cli_version="${engine_version_suffix#-}"
                        else
                            # Get latest version from repository
                            docker_ce_version="latest"
                        fi
                        
                        echo "(*) Attempting to download Docker CE packages from repository..."
                        
                        # Try to download latest packages if specific version fails
                        if ! curl -fsSL "${repo_baseurl}/Packages/docker-ce-${docker_ce_version}.el9.x86_64.rpm" -o docker-ce.rpm 2>/dev/null; then
                            # Fallback: try to get latest available version
                            echo "(*) Specific version not found, trying latest..."
                            latest_docker=$(curl -s "${repo_baseurl}/Packages/" | grep -o 'docker-ce-[0-9][^"]*\.el9\.x86_64\.rpm' | head -1)
                            latest_cli=$(curl -s "${repo_baseurl}/Packages/" | grep -o 'docker-ce-cli-[0-9][^"]*\.el9\.x86_64\.rpm' | head -1)
                            latest_containerd=$(curl -s "${repo_baseurl}/Packages/" | grep -o 'containerd\.io-[0-9][^"]*\.el9\.x86_64\.rpm' | head -1)
                            
                            if [ -n "${latest_docker}" ]; then
                                curl -fsSL "${repo_baseurl}/Packages/${latest_docker}" -o docker-ce.rpm
                                curl -fsSL "${repo_baseurl}/Packages/${latest_cli}" -o docker-ce-cli.rpm
                                curl -fsSL "${repo_baseurl}/Packages/${latest_containerd}" -o containerd.io.rpm
                            else
                                echo "(*) ERROR: Could not find Docker CE packages in repository"
                                echo "(*) Please check repository configuration or use 'moby': true"
                                exit 1
                            fi
                        fi
                        # Install systemd libraries required by Docker CE
                        echo "(*) Installing systemd libraries required by Docker CE..."
                        ${PKG_MGR_CMD} -y install systemd-libs || ${PKG_MGR_CMD} -y install systemd-devel || {
                            echo "(*) WARNING: Could not install systemd libraries"
                            echo "(*) Docker may fail to start without these"
                        }
                         
                        # Install with rpm --force --nodeps
                        echo "(*) Installing Docker CE packages with dependency override..."
                        rpm -Uvh --force --nodeps *.rpm
                        
                        # Cleanup
                        cd /
                        rm -rf /tmp/docker-ce-install
                        
                        echo "(*) Docker CE installation completed with dependency bypass"
                        echo "(*) Note: Some SELinux functionality may be limited without container-selinux"
                    fi
                else
                    # Standard installation for other RHEL-based systems
                    ${PKG_MGR_CMD} -y install docker-ce${cli_version_suffix} docker-ce-cli${engine_version_suffix} containerd.io
                fi
                # Install compose
                if [ "${DOCKER_DASH_COMPOSE_VERSION}" != "none" ]; then
                    ${PKG_MGR_CMD} -y install docker-compose-plugin || echo "(*) Package docker-compose-plugin not available for ${ID} ${VERSION_CODENAME} (${architecture}). Skipping."
                fi
            fi
            ;;
    esac
fi

echo "Finished installing docker / moby!"

docker_home="/usr/libexec/docker"
cli_plugins_dir="${docker_home}/cli-plugins"

# fallback for docker-compose
fallback_compose(){
    local url=$1
    local repo_url=$(get_github_api_repo_url "$url")
    echo -e "\n(!) Failed to fetch the latest artifacts for docker-compose v${compose_version}..."
    get_previous_version "${url}" "${repo_url}" compose_version
    echo -e "\nAttempting to install v${compose_version}"
    curl -fsSL "https://github.com/docker/compose/releases/download/v${compose_version}/docker-compose-linux-${target_compose_arch}" -o ${docker_compose_path}
}

# If 'docker-compose' command is to be included
if [ "${DOCKER_DASH_COMPOSE_VERSION}" != "none" ]; then
    case "${architecture}" in
    amd64|x86_64) target_compose_arch=x86_64 ;;
    arm64|aarch64) target_compose_arch=aarch64 ;;
    *)
        echo "(!) Docker in docker does not support machine architecture '$architecture'. Please use an x86-64 or ARM64 machine."
        exit 1
    esac

    docker_compose_path="/usr/local/bin/docker-compose"
    if [ "${DOCKER_DASH_COMPOSE_VERSION}" = "v1" ]; then
        err "The final Compose V1 release, version 1.29.2, was May 10, 2021. These packages haven't received any security updates since then. Use at your own risk."
        INSTALL_DOCKER_COMPOSE_SWITCH="false"

        if [ "${target_compose_arch}" = "x86_64" ]; then
            echo "(*) Installing docker compose v1..."
            curl -fsSL "https://github.com/docker/compose/releases/download/1.29.2/docker-compose-Linux-x86_64" -o ${docker_compose_path}
            chmod +x ${docker_compose_path}

            # Download the SHA256 checksum
            DOCKER_COMPOSE_SHA256="$(curl -sSL "https://github.com/docker/compose/releases/download/1.29.2/docker-compose-Linux-x86_64.sha256" | awk '{print $1}')"
            echo "${DOCKER_COMPOSE_SHA256}  ${docker_compose_path}" > docker-compose.sha256sum
            sha256sum -c docker-compose.sha256sum --ignore-missing
        elif [ "${VERSION_CODENAME}" = "bookworm" ]; then
            err "Docker compose v1 is unavailable for 'bookworm' on Arm64. Kindly switch to use v2"
            exit 1
        else
            # Use pip to get a version that runs on this architecture
            check_packages python3-minimal python3-pip libffi-dev python3-venv
            echo "(*) Installing docker compose v1 via pip..."
            export PYTHONUSERBASE=/usr/local
            pip3 install --disable-pip-version-check --no-cache-dir --user "Cython<3.0" pyyaml wheel docker-compose --no-build-isolation
        fi
    else
        compose_version=${DOCKER_DASH_COMPOSE_VERSION#v}
        docker_compose_url="https://github.com/docker/compose"
        find_version_from_git_tags compose_version "$docker_compose_url" "tags/v"
        echo "(*) Installing docker-compose ${compose_version}..."
        curl -fsSL "https://github.com/docker/compose/releases/download/v${compose_version}/docker-compose-linux-${target_compose_arch}" -o ${docker_compose_path} || {
                 echo -e "\n(!) Failed to fetch the latest artifacts for docker-compose v${compose_version}..." 
                 fallback_compose "$docker_compose_url"
        }

        chmod +x ${docker_compose_path}

        # Download the SHA256 checksum
        DOCKER_COMPOSE_SHA256="$(curl -sSL "https://github.com/docker/compose/releases/download/v${compose_version}/docker-compose-linux-${target_compose_arch}.sha256" | awk '{print $1}')"
        echo "${DOCKER_COMPOSE_SHA256}  ${docker_compose_path}" > docker-compose.sha256sum
        sha256sum -c docker-compose.sha256sum --ignore-missing

        mkdir -p ${cli_plugins_dir}
        cp ${docker_compose_path} ${cli_plugins_dir}
    fi
fi

# fallback method for compose-switch
fallback_compose-switch() {
    local url=$1
    local repo_url=$(get_github_api_repo_url "$url")
    echo -e "\n(!) Failed to fetch the latest artifacts for compose-switch v${compose_switch_version}..."
    get_previous_version "$url" "$repo_url" compose_switch_version
    echo -e "\nAttempting to install v${compose_switch_version}"
    curl -fsSL "https://github.com/docker/compose-switch/releases/download/v${compose_switch_version}/docker-compose-linux-${target_switch_arch}" -o /usr/local/bin/compose-switch
}
# Install docker-compose switch if not already installed - https://github.com/docker/compose-switch#manual-installation
if [ "${INSTALL_DOCKER_COMPOSE_SWITCH}" = "true" ] && ! type compose-switch > /dev/null 2>&1; then
    if type docker-compose > /dev/null 2>&1; then
        echo "(*) Installing compose-switch..."
        current_compose_path="$(command -v docker-compose)"
        target_compose_path="$(dirname "${current_compose_path}")/docker-compose-v1"
        compose_switch_version="latest"
        compose_switch_url="https://github.com/docker/compose-switch"
        # Try to get latest version, fallback to known stable version if GitHub API fails
        set +e
        find_version_from_git_tags compose_switch_version "$compose_switch_url"
        if [ $? -ne 0 ] || [ -z "${compose_switch_version}" ] || [ "${compose_switch_version}" = "latest" ]; then
            echo "(*) GitHub API rate limited or failed, using fallback method"
            fallback_compose-switch "$compose_switch_url"
        fi
        set -e
        
        # Map architecture for compose-switch downloads
        case "${architecture}" in
            amd64|x86_64) target_switch_arch=amd64 ;;
            arm64|aarch64) target_switch_arch=arm64 ;;
            *) target_switch_arch=${architecture} ;;
        esac
        curl -fsSL "https://github.com/docker/compose-switch/releases/download/v${compose_switch_version}/docker-compose-linux-${target_switch_arch}" -o /usr/local/bin/compose-switch || fallback_compose-switch "$compose_switch_url"
        chmod +x /usr/local/bin/compose-switch
        # TODO: Verify checksum once available: https://github.com/docker/compose-switch/issues/11
        # Setup v1 CLI as alternative in addition to compose-switch (which maps to v2)
        mv "${current_compose_path}" "${target_compose_path}"
        update-alternatives --install ${docker_compose_path} docker-compose /usr/local/bin/compose-switch 99
        update-alternatives --install ${docker_compose_path} docker-compose "${target_compose_path}" 1
    else
        err "Skipping installation of compose-switch as docker compose is unavailable..."
    fi
fi

# If init file already exists, exit
if [ -f "/usr/local/share/docker-init.sh" ]; then
    echo "/usr/local/share/docker-init.sh already exists, so exiting."
    # Clean up
    rm -rf /var/lib/apt/lists/*
    exit 0
fi
echo "docker-init doesn't exist, adding..."

if ! cat /etc/group | grep -e "^docker:" > /dev/null 2>&1; then
        groupadd -r docker
fi

usermod -aG docker ${USERNAME}

# fallback for docker/buildx
fallback_buildx() {
    local url=$1
    local repo_url=$(get_github_api_repo_url "$url")
    echo -e "\n(!) Failed to fetch the latest artifacts for docker buildx v${buildx_version}..."
    get_previous_version "$url" "$repo_url" buildx_version
    buildx_file_name="buildx-v${buildx_version}.linux-${target_buildx_arch}"
    echo -e "\nAttempting to install v${buildx_version}"
    wget https://github.com/docker/buildx/releases/download/v${buildx_version}/${buildx_file_name}
}
 
if [ "${INSTALL_DOCKER_BUILDX}" = "true" ]; then
    buildx_version="latest"
    docker_buildx_url="https://github.com/docker/buildx"
    find_version_from_git_tags buildx_version "$docker_buildx_url" "refs/tags/v"
    echo "(*) Installing buildx ${buildx_version}..."

      # Map architecture for buildx downloads
    case "${architecture}" in
        amd64|x86_64) target_buildx_arch=amd64 ;;
        arm64|aarch64) target_buildx_arch=arm64 ;;
        *) target_buildx_arch=${architecture} ;;
    esac

    buildx_file_name="buildx-v${buildx_version}.linux-${target_buildx_arch}"

    cd /tmp
    wget https://github.com/docker/buildx/releases/download/v${buildx_version}/${buildx_file_name} || fallback_buildx "$docker_buildx_url"
    
    docker_home="/usr/libexec/docker"
    cli_plugins_dir="${docker_home}/cli-plugins"

    mkdir -p ${cli_plugins_dir}
    mv ${buildx_file_name} ${cli_plugins_dir}/docker-buildx
    chmod +x ${cli_plugins_dir}/docker-buildx

    chown -R "${USERNAME}:docker" "${docker_home}"
    chmod -R g+r+w "${docker_home}"
    find "${docker_home}" -type d -print0 | xargs -n 1 -0 chmod g+s
fi

DOCKER_DEFAULT_IP6_TABLES=""
if [ "$DISABLE_IP6_TABLES" == true ]; then
    requested_version=""
    # checking whether the version requested either is in semver format or just a number denoting the major version
    # and, extracting the major version number out of the two scenarios
    semver_regex="^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-([0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*))?(\+([0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*))?$"
    if echo "$DOCKER_VERSION" | grep -Eq $semver_regex; then
        requested_version=$(echo $DOCKER_VERSION | cut -d. -f1)
    elif echo "$DOCKER_VERSION" | grep -Eq "^[1-9][0-9]*$"; then
        requested_version=$DOCKER_VERSION
    fi
    if [ "$DOCKER_VERSION" = "latest" ] || [[ -n "$requested_version" && "$requested_version" -ge 27 ]] ; then
        DOCKER_DEFAULT_IP6_TABLES="--ip6tables=false"
        echo "(!) As requested, passing '${DOCKER_DEFAULT_IP6_TABLES}'"
    fi
fi

if [ ! -d /usr/local/share ]; then
    mkdir -p /usr/local/share
fi

tee /usr/local/share/docker-init.sh > /dev/null \
<< EOF
#!/bin/sh
#-------------------------------------------------------------------------------------------------------------
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License. See https://go.microsoft.com/fwlink/?linkid=2090316 for license information.
#-------------------------------------------------------------------------------------------------------------

set -e

AZURE_DNS_AUTO_DETECTION=${AZURE_DNS_AUTO_DETECTION}
DOCKER_DEFAULT_ADDRESS_POOL=${DOCKER_DEFAULT_ADDRESS_POOL}
DOCKER_DEFAULT_IP6_TABLES=${DOCKER_DEFAULT_IP6_TABLES}
EOF

tee -a /usr/local/share/docker-init.sh > /dev/null \
<< 'EOF'
dockerd_start="AZURE_DNS_AUTO_DETECTION=${AZURE_DNS_AUTO_DETECTION} DOCKER_DEFAULT_ADDRESS_POOL=${DOCKER_DEFAULT_ADDRESS_POOL} DOCKER_DEFAULT_IP6_TABLES=${DOCKER_DEFAULT_IP6_TABLES} $(cat << 'INNEREOF'
    # explicitly remove dockerd and containerd PID file to ensure that it can start properly if it was stopped uncleanly
    find /run /var/run -iname 'docker*.pid' -delete || :
    find /run /var/run -iname 'container*.pid' -delete || :

    # -- Start: dind wrapper script --
    # Maintained: https://github.com/moby/moby/blob/master/hack/dind

    export container=docker

    if [ -d /sys/kernel/security ] && ! mountpoint -q /sys/kernel/security; then
        mount -t securityfs none /sys/kernel/security || {
            echo >&2 'Could not mount /sys/kernel/security.'
            echo >&2 'AppArmor detection and --privileged mode might break.'
        }
    fi

    # Mount /tmp (conditionally)
    if ! mountpoint -q /tmp; then
        mount -t tmpfs none /tmp
    fi

    set_cgroup_nesting()
    {
        # cgroup v2: enable nesting
        if [ -f /sys/fs/cgroup/cgroup.controllers ]; then
            # move the processes from the root group to the /init group,
            # otherwise writing subtree_control fails with EBUSY.
            # An error during moving non-existent process (i.e., "cat") is ignored.
            mkdir -p /sys/fs/cgroup/init
            xargs -rn1 < /sys/fs/cgroup/cgroup.procs > /sys/fs/cgroup/init/cgroup.procs || :
            # enable controllers
            sed -e 's/ / +/g' -e 's/^/+/' < /sys/fs/cgroup/cgroup.controllers \
                > /sys/fs/cgroup/cgroup.subtree_control
        fi
    }

    # Set cgroup nesting, retrying if necessary
    retry_cgroup_nesting=0

    until [ "${retry_cgroup_nesting}" -eq "5" ];
    do
        set +e
            set_cgroup_nesting

            if [ $? -ne 0 ]; then
                echo "(*) cgroup v2: Failed to enable nesting, retrying..."
            else
                break
            fi

            retry_cgroup_nesting=`expr $retry_cgroup_nesting + 1`
        set -e
    done

    # -- End: dind wrapper script --

    # Handle DNS
    set +e
        cat /etc/resolv.conf | grep -i 'internal.cloudapp.net' > /dev/null 2>&1
        if [ $? -eq 0 ] && [ "${AZURE_DNS_AUTO_DETECTION}" = "true" ]
        then
            echo "Setting dockerd Azure DNS."
            CUSTOMDNS="--dns 168.63.129.16"
        else
            echo "Not setting dockerd DNS manually."
            CUSTOMDNS=""
        fi
    set -e

    if [ -z "$DOCKER_DEFAULT_ADDRESS_POOL" ]
    then
        DEFAULT_ADDRESS_POOL=""
    else
        DEFAULT_ADDRESS_POOL="--default-address-pool $DOCKER_DEFAULT_ADDRESS_POOL"
    fi

    # Start docker/moby engine
    ( dockerd $CUSTOMDNS $DEFAULT_ADDRESS_POOL $DOCKER_DEFAULT_IP6_TABLES > /tmp/dockerd.log 2>&1 ) &
INNEREOF
)"

sudo_if() {
    COMMAND="$*"

    if [ "$(id -u)" -ne 0 ]; then
        sudo $COMMAND
    else
        $COMMAND
    fi
}

retry_docker_start_count=0
docker_ok="false"

until [ "${docker_ok}" = "true"  ] || [ "${retry_docker_start_count}" -eq "5" ];
do
    # Start using sudo if not invoked as root
    if [ "$(id -u)" -ne 0 ]; then
        sudo /bin/sh -c "${dockerd_start}"
    else
        eval "${dockerd_start}"
    fi

    retry_count=0
    until [ "${docker_ok}" = "true"  ] || [ "${retry_count}" -eq "5" ];
    do
        sleep 1s
        set +e
            docker info > /dev/null 2>&1 && docker_ok="true"
        set -e

        retry_count=`expr $retry_count + 1`
    done

    if [ "${docker_ok}" != "true" ] && [ "${retry_docker_start_count}" != "4" ]; then
        echo "(*) Failed to start docker, retrying..."
        set +e
            sudo_if pkill dockerd
            sudo_if pkill containerd
        set -e
    fi

    retry_docker_start_count=`expr $retry_docker_start_count + 1`
done

# Execute whatever commands were passed in (if any). This allows us
# to set this script to ENTRYPOINT while still executing the default CMD.
exec "$@"
EOF

chmod +x /usr/local/share/docker-init.sh
chown ${USERNAME}:root /usr/local/share/docker-init.sh

# Clean up
rm -rf /var/lib/apt/lists/*

echo 'docker-in-docker-debian script has completed!'
