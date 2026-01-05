#!/bin/bash

SCRIPT_NAME=$(basename "$0")
SCRIPT_DIR="$(dirname "$(realpath "$(readlink -f "$0")")")"
FILE_WITH_VERSION="$SCRIPT_DIR/src/version.h"

usage() {
    echo -e "\e[1m$SCRIPT_NAME\e[22m – Tag a new release\e[0m.

Usage:
    $SCRIPT_NAME <version>
    $SCRIPT_NAME -h\e[2m|\e[22m--help

Options & arguments:
    <version>      A version number, like 20
    -h\e[2m|\e[22m--help      Display this help.
"
}

usage_error() {
    echo -e "\e[31m$1\e[0m" >&2
    echo >&2
    usage >&2
    exit 2
}

while [[ -n "$1" ]]; do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        -*)
            usage_error "Unknown option: \e[1m$1\e[22m"
            ;;
        *)
            version_string="$1"
            shift
            if [[ -n "$1" ]]; then
                usage_error "Extraneous arguments after <version_string> (\e[1m$version_string\e[22m)."
            fi
            ;;
    esac
done

if [[ -z "$version_string" ]]; then
    usage_error "Missing \e[1m<version_string>\e[22m argument."
fi

if [[ ! "$version_string" =~ ^[0-9]+$ ]]; then
    usage_error "\e[1m$version_string\e[22m is not an int."
fi

cmd() {
    msg="$1"; shift
    shift
    echo -e "$msg: \e[1m$(printf '%q ' "$@")\e[22m" >&2
    "$@"
    local retval=$?
    if [[ "$retval" -eq 0 ]]; then
        echo -e "\e[32;1m✔ done\e[39;22m" >&2
    else
        echo -e "\e[31;1m❌failed with exit code \e[4m$retval\e[24m\e[39;22m" >&2
    fi
    echo >&2
    return "$retval"
}

if output=$(git status --porcelain) && [[ -n "$output" ]]; then
    1>&2 echo "Git not clean"
    exit 3
fi


git_tag="v$version_string"
git_msg="v$version_string"

if git rev-parse "$git_tag" &>/dev/null; then
    1>&2 echo "Git tag seems already in use"
    exit 3
fi

if ! cmd "Setting project version" -- sed -i -E "/int version/ s/^constexpr int version =.*$/constexpr int version = $version_string;/" "$FILE_WITH_VERSION"; then
    exit 3
fi

if ! cmd "Staging changes to \e[1m$FILE_WITH_VERSION\e[22m" -- git add "$FILE_WITH_VERSION"; then
    exit 3
fi

if ! cmd "Committing release $version_string" -- git commit -m "$git_msg"; then
    exit 3
fi

cmd "Tagging release $version_string" -- git tag -a -m "$git_msg" "$git_tag"

# vim: set expandtab tabstop=4 shiftwidth=4:
