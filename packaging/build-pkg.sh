#!/usr/bin/env bash
# Build the macOS installer: Duster-<version>-macOS.pkg with AU and VST3 as separate choices.
#
#   packaging/build-pkg.sh [artefacts-dir] [out-dir]
#
# Defaults: build/dist/Duster_artefacts/Release and build/dist/pkg (run the dist preset first).
#
# Signing is optional and driven by the environment, so the same script works unsigned on a
# laptop and signed in CI:
#   DUSTER_SIGN_APP        "Developer ID Application: ..." identity for the plugin bundles
#   DUSTER_SIGN_INSTALLER  "Developer ID Installer: ..." identity for the .pkg
#   DUSTER_NOTARY_PROFILE  notarytool keychain profile (xcrun notarytool store-credentials);
#                           needs both identities, then the pkg is notarized and stapled.
#   DUSTER_NOTARY_KEYCHAIN optional keychain holding that profile (CI uses a temporary one)
# Without them the bundles are ad-hoc signed and the pkg is unsigned.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly ROOT
readonly BUNDLE_ID="com.LVST.Duster"
WORK_DIR="" # global so the EXIT trap still sees it after main returns

die() {
    printf 'build-pkg: %s\n' "$*" >&2
    exit 1
}

project_version() {
    local version
    version="$(sed -nE 's/^project\(Duster VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' "$ROOT/CMakeLists.txt")"
    [[ -n "$version" ]] || die "could not read the project version from CMakeLists.txt"
    printf '%s\n' "$version"
}

sign_bundle() {
    local bundle="$1"
    if [[ -n "${DUSTER_SIGN_APP:-}" ]]; then
        codesign --force --deep --options runtime --timestamp --sign "$DUSTER_SIGN_APP" "$bundle"
    else
        codesign --force --deep --sign - "$bundle"
    fi
    codesign --verify --strict "$bundle"
}

# One component package per format. BundleIsRelocatable=false stops Installer from "upgrading"
# a stray copy of the plugin somewhere else on disk instead of writing to the plugin folder.
build_component_pkg() {
    local bundle="$1" install_dir="$2" suffix="$3" version="$4" work="$5"
    local stage="$work/root-$suffix" plist="$work/components-$suffix.plist"
    mkdir -p "$stage"
    ditto "$bundle" "$stage/$(basename "$bundle")"
    sign_bundle "$stage/$(basename "$bundle")"
    pkgbuild --analyze --root "$stage" "$plist" >/dev/null
    plutil -replace 0.BundleIsRelocatable -bool NO "$plist"
    pkgbuild --root "$stage" --component-plist "$plist" \
        --identifier "$BUNDLE_ID.pkg.$suffix" --version "$version" \
        --install-location "$install_dir" "$work/Duster-$suffix.pkg" >/dev/null
}

main() {
    local artefacts="${1:-$ROOT/build/dist/Duster_artefacts/Release}"
    local out_dir="${2:-$ROOT/build/dist/pkg}"
    local au="$artefacts/AU/Duster.component" vst3="$artefacts/VST3/Duster.vst3"
    [[ -d "$au" ]] || die "missing $au (build the dist preset first)"
    [[ -d "$vst3" ]] || die "missing $vst3 (build the dist preset first)"
    if [[ -n "${DUSTER_NOTARY_PROFILE:-}" ]] && [[ -z "${DUSTER_SIGN_APP:-}" || -z "${DUSTER_SIGN_INSTALLER:-}" ]]; then
        die "notarizing needs DUSTER_SIGN_APP and DUSTER_SIGN_INSTALLER"
    fi

    local version work pkg
    version="$(project_version)"
    WORK_DIR="$(mktemp -d)"
    trap 'rm -rf "$WORK_DIR"' EXIT
    work="$WORK_DIR"
    pkg="$out_dir/Duster-$version-macOS.pkg"
    mkdir -p "$out_dir"

    build_component_pkg "$au" /Library/Audio/Plug-Ins/Components au "$version" "$work"
    build_component_pkg "$vst3" /Library/Audio/Plug-Ins/VST3 vst3 "$version" "$work"

    sed "s/@VERSION@/$version/g" "$ROOT/packaging/distribution.xml" >"$work/distribution.xml"
    local sign_args=()
    [[ -n "${DUSTER_SIGN_INSTALLER:-}" ]] && sign_args=(--sign "$DUSTER_SIGN_INSTALLER" --timestamp)
    productbuild --distribution "$work/distribution.xml" --resources "$ROOT/packaging/resources" \
        --package-path "$work" "${sign_args[@]+"${sign_args[@]}"}" "$pkg" >/dev/null

    if [[ -n "${DUSTER_NOTARY_PROFILE:-}" ]]; then
        local keychain_args=()
        [[ -n "${DUSTER_NOTARY_KEYCHAIN:-}" ]] && keychain_args=(--keychain "$DUSTER_NOTARY_KEYCHAIN")
        xcrun notarytool submit "$pkg" --keychain-profile "$DUSTER_NOTARY_PROFILE" \
            "${keychain_args[@]+"${keychain_args[@]}"}" --wait
        xcrun stapler staple "$pkg"
    fi

    (cd "$out_dir" && shasum -a 256 "$(basename "$pkg")" >"$(basename "$pkg").sha256")
    printf '%s\n' "$pkg"
}

main "$@"
