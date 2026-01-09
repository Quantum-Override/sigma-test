# objectify — embed files as C resources

objectify is a small build-time utility that converts a file (text or binary) into a C byte array, emits a generated C source/header-like file under `src/templates/`, and compiles that generated file into an object file placed in `resources/`. The compiled object contains symbols you can link into your program (for example: `main_data` and `main_data_size`).

## Purpose

Some parts of this project need templates or other assets available at runtime without depending on external files. `objectify` lets you embed those assets directly into the program binary by producing linkable object files that contain the resource bytes.

## How it works (high-level)

1. Run `objectify` on an input file (for example `templates/main.txt`).
2. `objectify`:
   - extracts the input file base name (strips directories and extension)
   - emits `src/templates/<base>_template.ct` containing:
     - an include guard: `<BASE_UPPER>_TEMPLATE_CT`
     - `#include <stddef.h>`
     - `const unsigned char <basename_lower>_data[] = { ... };`
     - `const size_t <basename_lower>_data_size = <N>;`
   - compiles the generated `.ct` file with `gcc` to produce `resources/<base>_template.ro`
3. Link `resources/<base>_template.ro` into your executable — the linker decides by file format, so a valid object file named `.ro` will link fine.

## Usage

Build the tool:

- The repo includes a `objectify` build target in `build.json`.
- Or build manually:

  ```sh
  gcc -Wall -g -Iinclude -o bin/objectify tools/objectify.c
  ```

Run:

```sh
./bin/objectify [-v] <path/to/input.file>
```

Example:

```sh
./bin/objectify templates/main.txt
# Generates: src/templates/main_template.ct
# Compiles to: resources/main_template.ro
```

## Generated symbols

For an input file named `<something>.<ext>`, `objectify` creates:

- `src/templates/<something>_template.ct` containing:
  - `const unsigned char something_data[] = { ... };`
  - `const size_t something_data_size = <N>;`

Consume these symbols in C code:

```c
#include <stddef.h>
extern const unsigned char main_data[];
extern const size_t main_data_size;

/* Use the embedded bytes */
void use_template(void) {
    fwrite(main_data, 1, main_data_size, stdout);
}
```

## Naming & file layout conventions

- Generated template (C source/header): `src/templates/<base>_template.ct`
- Compiled resource object: `resources/<base>_template.ro`
- Include guard: `<BASE_UPPER>_TEMPLATE_CT`
- Data symbol suffix: `_data`
- Size symbol suffix: `_data_size`

## Important notes & caveats

- `.ro` extension
  - `.ro` is a repository-specific convention (likely meaning “resource object”). It is not a special `gcc`/`ld` extension.
  - `gcc`/`ld` link by file contents/format, so a valid object file named `.ro` will link fine. However, many tools (glob patterns, IDEs, packaging scripts) expect `.o` and may not pick up `.ro` files automatically.
  - Recommendation: keep `.ro` if you want the semantic distinction, but either update other build scripts to include `*.ro` or consider switching to `.o` for maximum tooling compatibility.

- `src/templates/` directory
  - `objectify` currently ensures `resources/` exists (`mkdir -p resources`) but does not create `src/templates/`. If `src/templates/` is missing, `objectify` will fail when opening the output file. Create `src/templates/` before running or consider updating the tool to create it automatically.

- Mutating base name
  - `objectify` duplicates the input filename and then mutates it in-place with `to_upper()`/`to_lower()` for guard and symbol generation. Documented here for awareness.

- memmove usage
  - The code uses `memmove(base_name, slash + 1, strlen(slash));`. This works but is slightly non-idiomatic; using `strlen(slash + 1) + 1` to include the terminating NUL is clearer.

- build.json
  - `build.json` includes a `clean-objectify` op that removes `src/templates/*.ct` and `resources/*.ro`. If you change the extension to `.o`, update the clean rules.

## Troubleshooting

- If the generated `.ct` file exists but compilation fails:
  - Inspect the compile command printed by the tool (use `-v`).
  - Run the compile command manually to see errors.
  - Check include path: `objectify` adds `-Iinclude` to the compile command; the generated files include `<stddef.h>` only, so errors are unlikely here.

- Validate object file:
  - `file resources/<name>_template.ro`
  - `readelf -h resources/<name>_template.ro`
  - `nm resources/<name>_template.ro`

- Linking:
  - Link with `gcc` as usual, passing the `.ro` explicitly:
    ```sh
    gcc -o myprog main.o resources/<name>_template.ro -L... -l...
    ```

## Suggested improvements (optional)

- Create `src/templates/` automatically if absent.
- Make the output extension configurable or switch to `.o` for broader tooling compatibility.
- Add a flag to `objectify` to skip compilation and only emit the `.ct` file (useful for dry-runs or CI tasks).
- Tidy the `memmove` call for clarity and correctness.

---

This file documents the `objectify` tool and the repository convention for embedding resources. If you want, I can also open a PR to add this file on a branch (e.g., `docs/objectify-readme`) instead of committing directly to `main`.
