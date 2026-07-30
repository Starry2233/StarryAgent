#!/usr/bin/env python3
import argparse
import json
import os
import re
import subprocess
import sys
from collections import deque
from pathlib import Path

SYSTEM_LIBS = {
    "ld-android.so",
    "libandroid.so",
    "libc.so",
    "libdl.so",
    "libEGL.so",
    "libGLESv2.so",
    "libGLESv3.so",
    "libjnigraphics.so",
    "liblog.so",
    "libm.so",
    "libmediandk.so",
    "libOpenSLES.so",
    "libz.so",
}

QT_MODULE_TO_LIB = {
    "Core": "libQt6Core.so",
    "Gui": "libQt6Gui.so",
    "Qml": "libQt6Qml.so",
    "Quick": "libQt6Quick.so",
    "QuickControls2": "libQt6QuickControls2.so",
    "Network": "libQt6Network.so",
    "Multimedia": "libQt6Multimedia.so",
}

ABI_TRIPLES = {
    "arm64-v8a": "aarch64-linux-android",
    "x86_64": "x86_64-linux-android",
    "x86": "i686-linux-android",
}

HOST_TAGS = {
    "win32": "windows-x86_64",
    "cygwin": "windows-x86_64",
    "darwin": "darwin-x86_64",
    "linux": "linux-x86_64",
}


def as_posix(path: Path) -> str:
    return path.as_posix()


def resolve_existing(path: Path) -> Path:
    return path.expanduser().resolve(strict=True)


def resolve_maybe(path: Path) -> Path:
    return path.expanduser().resolve(strict=False)


def host_tag() -> str:
    for prefix, tag in HOST_TAGS.items():
        if sys.platform.startswith(prefix):
            return tag
    raise RuntimeError(f"Unsupported host platform: {sys.platform}")


def exe_name(name: str) -> str:
    if os.name == "nt" and not name.endswith(".exe"):
        return f"{name}.exe"
    return name


def find_tool(root: Path, relative_parts: list[str], command: str) -> Path:
    candidates = [root.joinpath(*relative_parts, exe_name(command)), root.joinpath(*relative_parts, command)]
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    for path_dir in os.environ.get("PATH", "").split(os.pathsep):
        if not path_dir:
            continue
        candidate = Path(path_dir) / exe_name(command)
        if candidate.exists():
            return candidate.resolve()
    raise FileNotFoundError(f"{command} not found")


def ndk_libcxx_path(ndk: Path, abi: str) -> Path:
    triple = ABI_TRIPLES.get(abi)
    if not triple:
        raise RuntimeError(f"Unsupported ABI: {abi}")
    return ndk / "toolchains" / "llvm" / "prebuilt" / host_tag() / "sysroot" / "usr" / "lib" / triple / "libc++_shared.so"


def qml_uri_to_relative(uri: str) -> str:
    return uri.replace(".", "/")


def run_qmlimportscanner(qmlimportscanner: Path, qml_root: Path, qt_qml_root: Path) -> list[dict]:
    proc = subprocess.run(
        [str(qmlimportscanner), "-rootPath", str(qml_root), "-importPath", str(qt_qml_root)],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if proc.returncode != 0:
        raise RuntimeError(f"qmlimportscanner failed:\n{proc.stderr}")
    if not proc.stdout.strip():
        return []
    return json.loads(proc.stdout)


def read_qmldir_dependencies(qmldir_path: Path) -> list[str]:
    if not qmldir_path.exists():
        return []
    deps: list[str] = []
    for raw_line in qmldir_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if parts and parts[0] == "depends" and len(parts) >= 2:
            deps.append(parts[1])
    return deps


def collect_qml_modules(scanner_modules: list[dict], qt_qml_root: Path, required: list[str], warnings: list[str]) -> list[dict]:
    qt_qml_real = qt_qml_root.resolve(strict=True)
    module_paths: dict[str, str] = {}
    module_uris: dict[str, str] = {}

    for module in scanner_modules:
        if module.get("type") != "module":
            continue
        relative_path = module.get("relativePath")
        module_path = module.get("path")
        if not relative_path or not module_path:
            continue
        try:
            resolved_path = Path(module_path).resolve(strict=True)
            resolved_path.relative_to(qt_qml_real)
        except Exception:
            warnings.append(f"Ignoring qmlimportscanner module outside Qt QML root: {module_path}")
            continue
        normalized = Path(relative_path).as_posix().strip("/")
        if normalized:
            module_paths[normalized] = normalized
            if module.get("name"):
                module_uris[normalized] = str(module["name"])

    for uri in required:
        relative = qml_uri_to_relative(uri)
        module_paths[relative] = relative
        module_uris[relative] = uri

    queue = deque(sorted(module_paths.keys()))
    seen = set(queue)
    while queue:
        relative = queue.popleft()
        qmldir = qt_qml_root / relative / "qmldir"
        for dependency_uri in read_qmldir_dependencies(qmldir):
            dep_relative = qml_uri_to_relative(dependency_uri)
            if dep_relative not in seen:
                seen.add(dep_relative)
                module_paths[dep_relative] = dep_relative
                module_uris[dep_relative] = dependency_uri
                queue.append(dep_relative)

    imports = []
    for relative in sorted(module_paths.keys()):
        source = qt_qml_root / relative
        if not source.exists():
            warnings.append(f"QML module directory not found: {source}")
            continue
        imports.append(
            {
                "uri": module_uris.get(relative, relative.replace("/", ".")),
                "relativePath": relative,
                "source": as_posix(source.resolve()),
                "destination": f"assets/qt-project.org/imports/{relative}",
            }
        )
    return imports


def collect_qml_plugin_libraries(qml_imports: list[dict]) -> list[Path]:
    libs: set[Path] = set()
    for item in qml_imports:
        source = Path(item["source"])
        if source.exists():
            libs.update(path.resolve() for path in source.rglob("*.so") if path.is_file())
    return sorted(libs, key=lambda p: p.as_posix().lower())


def read_needed(readelf: Path, library: Path) -> list[str]:
    proc = subprocess.run(
        [str(readelf), "-d", str(library)],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if proc.returncode != 0:
        raise RuntimeError(f"llvm-readelf failed for {library}:\n{proc.stderr}")
    needed = []
    for line in proc.stdout.splitlines():
        match = re.search(r"Shared library: \[(.+?)\]", line)
        if match:
            needed.append(match.group(1))
    return needed


def build_library_index(search_roots: list[Path]) -> dict[str, Path]:
    index: dict[str, Path] = {}
    for root in search_roots:
        if not root.exists():
            continue
        for lib in root.rglob("*.so"):
            index.setdefault(lib.name, lib.resolve())
    return index


def library_kind(path: Path, qt_sdk: Path, app_so: Path) -> str:
    if path.resolve() == app_so.resolve():
        return "app"
    try:
        rel = path.resolve().relative_to(qt_sdk.resolve())
        first = rel.parts[0] if rel.parts else ""
        if first == "lib":
            return "qt-lib"
        if first == "plugins":
            return "qt-plugin"
        if first == "qml":
            return "qml-plugin"
    except ValueError:
        pass
    if path.name == "libc++_shared.so":
        return "ndk-lib"
    return "native-lib"


def staged_library_name(source: Path, abi: str, app_so: Path, app_lib_name: str) -> str:
    if source.resolve() == app_so.resolve():
        return f"lib{app_lib_name}_{abi}.so"
    stem = source.stem
    if source.name.endswith(f"_{abi}.so"):
        return source.name
    if stem.startswith("libQt6"):
        return f"{stem}_{abi}.so"
    if stem == "libqtforandroid" and "platforms" in source.parts:
        return f"libplugins_platforms_qtforandroid_{abi}.so"
    if "plugins" in source.parts:
        plugin_index = source.parts.index("plugins")
        rel_parts = source.parts[plugin_index + 1 :]
        return "libplugins_" + "_".join(part.removesuffix(".so") for part in rel_parts) + f"_{abi}.so"
    if "qml" in source.parts:
        qml_index = source.parts.index("qml")
        rel_parts = source.parts[qml_index + 1 :]
        return "libqml_" + "_".join(part.removesuffix(".so") for part in rel_parts) + f"_{abi}.so"
    return source.name


def qt_library_candidates(qt_lib: Path, module: str, abi: str) -> list[Path]:
    stem = QT_MODULE_TO_LIB.get(module.strip(), f"libQt6{module.strip()}.so").removesuffix(".so")
    return [qt_lib / f"{stem}_{abi}.so", qt_lib / f"{stem}.so"]


def platform_plugin_candidates(qt_plugins: Path, abi: str) -> list[Path]:
    return [
        qt_plugins / "platforms" / f"libplugins_platforms_qtforandroid_{abi}.so",
        qt_plugins / "platforms" / "libqtforandroid.so",
    ]


def collect_runtime_plugin_libraries(qt_plugins: Path, categories: list[str]) -> list[Path]:
    libs: set[Path] = set()
    for category in categories:
        plugin_dir = qt_plugins / category
        if plugin_dir.exists():
            libs.update(path.resolve() for path in plugin_dir.glob("*.so") if path.is_file())
    return sorted(libs, key=lambda p: p.as_posix().lower())


def trace_native_libraries(
    readelf: Path,
    qt_sdk: Path,
    ndk: Path,
    abi: str,
    app_so: Path,
    app_lib_name: str,
    linked_qt_modules: list[str],
    qml_plugin_libs: list[Path],
    runtime_plugin_libs: list[Path],
    extra_native_libs: list[Path],
    warnings: list[str],
) -> list[dict]:
    qt_lib = qt_sdk / "lib"
    qt_plugins = qt_sdk / "plugins"
    qt_qml = qt_sdk / "qml"
    libcxx = ndk_libcxx_path(ndk, abi).resolve(strict=True)
    platform_plugin = qt_plugins / "platforms" / "libqtforandroid.so"

    seeds: list[Path] = [app_so.resolve(strict=True), libcxx]
    for module in linked_qt_modules:
        candidates = qt_library_candidates(qt_lib, module, abi)
        candidate = next((item for item in candidates if item.exists()), None)
        if candidate:
            seeds.append(candidate.resolve())
        else:
            warnings.append(f"Linked Qt module library not found: {candidates[0]}")
    platform_plugin = next((item for item in platform_plugin_candidates(qt_plugins, abi) if item.exists()), None)
    if platform_plugin:
        seeds.append(platform_plugin.resolve())
    else:
        warnings.append(f"Qt Android platform plugin not found: {platform_plugin_candidates(qt_plugins, abi)[0]}")
    seeds.extend(qml_plugin_libs)
    seeds.extend(runtime_plugin_libs)
    seeds.extend(path.resolve(strict=True) for path in extra_native_libs)

    index = build_library_index([qt_lib, qt_plugins, qt_qml, libcxx.parent])
    for seed in seeds:
        index.setdefault(seed.name, seed)

    queue = deque(seeds)
    seen: dict[Path, Path] = {}
    while queue:
        current = queue.popleft().resolve()
        if current in seen:
            continue
        seen[current] = current
        for needed in read_needed(readelf, current):
            if needed in SYSTEM_LIBS:
                continue
            resolved = index.get(needed)
            if not resolved:
                warnings.append(f"Unresolved DT_NEEDED {needed} from {current}")
                continue
            if resolved.resolve() not in seen:
                queue.append(resolved.resolve())

    libraries = []
    staged_names: set[str] = set()
    for source in sorted(seen.keys(), key=lambda p: p.as_posix().lower()):
        staged_name = staged_library_name(source, abi, app_so, app_lib_name)
        if staged_name in staged_names:
            warnings.append(f"Duplicate staged library name {staged_name} from {source}")
        staged_names.add(staged_name)
        libraries.append(
            {
                "source": as_posix(source),
                "destination": f"lib/{abi}/{staged_name}",
                "kind": library_kind(source, qt_sdk, app_so),
            }
        )
    return libraries


def libs_xml_items(native_libraries: list[dict], abi: str) -> tuple[list[str], list[str]]:
    qt_libs: list[str] = []
    load_local: list[str] = []
    for lib in native_libraries:
        basename = Path(lib["destination"]).name
        kind = lib.get("kind")
        if basename == "libc++_shared.so":
            qt_libs.append(f"{abi};c++_shared")
        elif basename == f"libQt6Core_{abi}.so":
            qt_libs.append(f"{abi};Qt6Core_{abi}")
        elif kind == "qt-plugin" and basename == f"libplugins_platforms_qtforandroid_{abi}.so":
            load_local.append(f"{abi};{basename}")
    return sorted(set(qt_libs)), sorted(set(load_local))


def main() -> int:
    parser = argparse.ArgumentParser(description="Trace Qt Android native/QML dependencies without androiddeployqt.")
    parser.add_argument("--repo-root", required=True)
    parser.add_argument("--qt-sdk", required=True)
    parser.add_argument("--qt-host", required=True)
    parser.add_argument("--ndk", required=True)
    parser.add_argument("--abi", required=True)
    parser.add_argument("--app-so", required=True)
    parser.add_argument("--app-lib-name", required=True)
    parser.add_argument("--qml-root", required=True)
    parser.add_argument("--linked-qt-modules", default="")
    parser.add_argument("--required-qml-module", action="append", default=[])
    parser.add_argument("--runtime-plugin-category", action="append", default=[])
    parser.add_argument("--extra-native-lib", action="append", default=[])
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    warnings: list[str] = []
    repo_root = resolve_existing(Path(args.repo_root))
    qt_sdk = resolve_existing(Path(args.qt_sdk))
    qt_host = resolve_existing(Path(args.qt_host))
    ndk = resolve_existing(Path(args.ndk))
    app_so = resolve_existing(Path(args.app_so))
    qml_root = resolve_existing(Path(args.qml_root))
    output = resolve_maybe(Path(args.output))

    qmlimportscanner = find_tool(qt_host, ["bin"], "qmlimportscanner")
    readelf = find_tool(ndk, ["toolchains", "llvm", "prebuilt", host_tag(), "bin"], "llvm-readelf")
    qt_qml_root = resolve_existing(qt_sdk / "qml")

    scanner_modules = run_qmlimportscanner(qmlimportscanner, qml_root, qt_qml_root)
    qml_imports = collect_qml_modules(scanner_modules, qt_qml_root, args.required_qml_module, warnings)
    qml_plugin_libs = collect_qml_plugin_libraries(qml_imports)
    runtime_plugin_libs = collect_runtime_plugin_libraries(qt_sdk / "plugins", args.runtime_plugin_category)
    extra_native_libs = [resolve_existing(Path(item)) for item in args.extra_native_lib]
    linked_qt_modules = [item.strip() for item in args.linked_qt_modules.split(",") if item.strip()]
    native_libraries = trace_native_libraries(
        readelf,
        qt_sdk,
        ndk,
        args.abi,
        app_so,
        args.app_lib_name,
        linked_qt_modules,
        qml_plugin_libs,
        runtime_plugin_libs,
        extra_native_libs,
        warnings,
    )
    qt_libs_xml, load_local_libs_xml = libs_xml_items(native_libraries, args.abi)

    manifest = {
        "abi": args.abi,
        "repoRoot": as_posix(repo_root),
        "qtSdk": as_posix(qt_sdk),
        "qmlImports": qml_imports,
        "nativeLibraries": native_libraries,
        "qtLibsXml": qt_libs_xml,
        "loadLocalLibsXml": load_local_libs_xml,
        "warnings": sorted(set(warnings)),
    }

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(str(output))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
