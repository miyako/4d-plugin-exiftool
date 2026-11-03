![version](https://img.shields.io/badge/version-19%2B-5682DF)
![platform](https://img.shields.io/static/v1?label=platform&message=mac-intel%20|%20mac-arm&color=blue)
[![license](https://img.shields.io/github/license/miyako/4d-plugin-exiftool)](LICENSE)
![downloads](https://img.shields.io/github/downloads/miyako/4d-plugin-exiftool/total)

# 4d-plugin-exiftool

4D interface for [ExifTool](https://exiftool.org) by Phil Harvey. The plugin ships ExifTool 12.40 (the Perl script and its `lib/` folder) inside the bundle and runs it as a child process for each call: one command reads every tag of a file into a 4D `Object`, the other writes tags from a `Collection`. Text values come back as `Text`. Binary values (thumbnails, previews, ICC profiles, …) come back as a `Picture` that wraps the raw bytes, which you turn into a `BLOB` with `PICTURE TO BLOB`. It covers far more formats and tags than the native [SET PICTURE METADATA](https://doc.4d.com/4Dv19/4D/19.1/SET-PICTURE-METADATA.301-5652803.en.html), and it deploys with a built 4D application: the user's machine needs no ExifTool installation.

| Command | Returns | Purpose |
|-|-|-|
| [ExifTool Get Metadata](#exiftool-get-metadata) | `Object` | Read all tags of a file |
| [ExifTool Set Metadata](#exiftool-set-metadata) | `Object` | Write, change or delete tags in a file |

The plugin also exports one constant, `ExifTool data encapsulation` (`"private.exiftool.data"`), the codec used to wrap binary tag data in a `Picture`.

**Platforms:** macOS (Apple silicon and Intel). Windows is not supported.

Releases: [github.com/miyako/4d-plugin-exiftool/releases](https://github.com/miyako/4d-plugin-exiftool/releases)

---

## Requirements & platform notes

- **macOS only.** The bundle contains `Windows64/exiftool.exe`, but there is no Windows plugin binary, so the commands are not available on Windows.
- **Needs the system Perl.** The bundled `exiftool` script starts with `#!/usr/bin/perl`. macOS still includes `/usr/bin/perl`, but Apple has said that scripting runtimes may be removed from future macOS versions. If that happens, every call fails with a launch error in `errorMessage`.
- **Paths are platform paths.** Pass `File.platformPath` (on macOS, the HFS form `Macintosh HD:Users:…`). A POSIX path (`/Users/…`) is not converted and does not work.
- **Both parameters of each command are mandatory.**
- **Failure is reported in the result, not as a 4D error.** Always test `status.success`, and read `status.errorMessage` (ExifTool's own error output) when it is `False`.
- **Time limits.** [ExifTool Get Metadata](#exiftool-get-metadata) waits at most 3 seconds and [ExifTool Set Metadata](#exiftool-set-metadata) at most 5 seconds for ExifTool to finish. Then ExifTool is stopped (see [Error handling](#error-handling--troubleshooting)). While it waits, the calling 4D process yields to other processes.
- **Thread-safe.** Both commands are declared thread-safe, so you can call them from preemptive processes. Each call starts its own ExifTool process, so concurrent calls don't share state.
- **Writes keep a backup.** ExifTool's default applies: when it modifies `photo.jpg`, it renames the original to `photo.jpg_original` in the same folder. Delete the backup yourself if you don't want it.

---

## ExifTool Get Metadata

### Syntax

```4d
status:=ExifTool Get Metadata(file) -> Object
```

| Parameter | Type | Description |
|-|-|-|
| `file` | Text | Platform path of the file to read |
| Result | Object | `success`, `errorMessage` and, on success, `tagInfo` (see below) |

### Description

Runs `exiftool <file> -b -php -l -G:0:1:2:4 -D -sep ", "` and turns its output into one object per tag. ExifTool decides which tags exist. The plugin returns all of them, including ExifTool's own entries such as `SourceFile` (the POSIX path), `FileName`, `Directory` and `FileSize`.

Result object:

| Property | Type | Description |
|-|-|-|
| `success` | Boolean | `True` when at least one tag was read |
| `errorMessage` | Text | ExifTool's error output (`stderr`), e.g. `Error: File not found - /path/x.jpg`. Usually empty on success. Also set when ExifTool could not be started (see [Error handling](#error-handling--troubleshooting)) |
| `tagInfo` | Collection | One object per tag, in ExifTool's order. Only present when `success` is `True` |

Tag object:

| Property | Type | Description |
|-|-|-|
| `name` | Text | Tag name, e.g. `Orientation`, `ThumbnailImage` |
| `value` | Text | The value as ExifTool prints it (its "converted" value, e.g. `Rotate 90 CW`, `9.6 KiB`). Numbers are returned as text too |
| `data` | Picture | Instead of `value`, for binary values: the raw bytes wrapped in a picture (see below) |
| `description` | Text | Human-readable tag name, e.g. `File Size` |
| `id` | Text | Tag ID in its group (for EXIF, the decimal tag number) |
| `group[0]` | Text | Family 0 group (`EXIF`, `File`, `XMP`, `ExifTool`, …) |
| `group[1]` | Text | Family 1 group (`IFD0`, `System`, `XMP-dc`, …) |
| `group[2]` | Text | Family 2 group (`Image`, `Camera`, `Time`, …) |
| `copyNum` | Real | Copy number, only when the file contains the same tag more than once |

The `group[n]` property names contain brackets, so read them with the bracket syntax: `$tag["group[0]"]`.

How a value is classified: the plugin converts the bytes ExifTool printed to UTF-16 and back to UTF-8. If that round trip gives the same length, it returns them as `value` (UTF-8 text). Anything else is binary and comes back as `data`, a `Picture` made with `BLOB TO PICTURE` and the codec `ExifTool data encapsulation`. Convert it back with `PICTURE TO BLOB` and the same constant to get the original bytes.

- Only the converted value is returned. ExifTool's raw numeric value (for example `6` instead of `Rotate 90 CW`) is not.
- A number and a text are both `Text`, so `"0"` and an empty value can't be told apart by type.
- A tag whose value is empty has no `name` and no `value` (it still has `description`, `id` and the groups). Search by `id` or `description` if you need those tags.

### Example

From the plugin's own test method (`test/Project/Sources/Methods/TEST.4dm`):

```4d
$folder:=Folder:C1567(fk resources folder:K87:11)
$file:=$folder.file("Canon_40D_photoshop_import.jpg")

$status:=ExifTool Get Metadata($file.platformPath)

If ($status.success)
	
/*
example of binary tag; returned as encapsulated picture, retrivable as BLOB
*/
	
	$tagInfo:=$status.tagInfo
	
	$pict:=$tagInfo.query("name == :1"; "ThumbnailImage")[0].data
	PICTURE TO BLOB:C692($pict; $data; ExifTool data encapsulation)
	BLOB TO DOCUMENT:C526(System folder:C487(Desktop:K41:16)+"thumbnail.jpg"; $data)
	
End if 
```

Reading one value, with a fallback when the file has no such tag:

```4d
$status:=ExifTool Get Metadata($file.platformPath)

If ($status.success)
	$found:=$status.tagInfo.query("name == :1"; "Orientation")
	If ($found.length#0)
		ALERT($found[0].value)  // e.g. "Horizontal (normal)"
	End if 
Else 
	ALERT($status.errorMessage)
End if 
```

Listing only the EXIF tags:

```4d
For each ($tag; $status.tagInfo)
	If ($tag["group[0]"]="EXIF")
		// $tag.name, $tag.value or $tag.data
	End if 
End for each 
```

---

## ExifTool Set Metadata

### Syntax

```4d
status:=ExifTool Set Metadata(file; tags) -> Object
```

| Parameter | Type | Description |
|-|-|-|
| `file` | Text | Platform path of the file to modify |
| `tags` | Collection | Objects with a `name` and a `value` or `data` (see below) |
| Result | Object | `success` and, on failure, `errorMessage` |

### Description

Builds an ExifTool argument list (`-name=value`, one per tag) and runs `exiftool -@ -` with it. ExifTool rewrites the file and keeps the original as `<file>_original`.

Each element of `tags` is an object:

| Property | Type | Description |
|-|-|-|
| `name` | Text | Tag to write, as ExifTool spells it: `Artist`, `Orientation`, or with a group, `EXIF:Artist`, `XMP-dc:Subject`. Elements without a Text `name` are skipped |
| `value` | Text, Null or Number | What to write (see below) |
| `data` | Picture | Binary value, used when `value` is neither Text nor Null: bytes wrapped with `BLOB TO PICTURE` and `ExifTool data encapsulation` |

The first matching case applies:

1. `value` is **Text** → written as is (`-name=value`), and ExifTool converts it the way it prints it (`"Rotate 90 CW"` for `Orientation`).
2. `value` is **Null** → the tag is deleted (`-name=`).
3. `data` is a **Picture** → its raw bytes are written. An empty picture deletes the tag.
4. Otherwise → `value` is read as a number and written as ExifTool's raw numeric value (`-name#=`, formatted like `6.000000`). So `New object("name"; "Orientation"; "value"; 6)` sets the raw value 6 (`Rotate 90 CW`). **An object with no `value` and no `data` writes `0`.**

`success` is `True` only when ExifTool reports `1 image files updated` and no failed files. Otherwise `errorMessage` holds ExifTool's error output, for example `Warning: Tag 'Foo' is not defined` followed by `Nothing to do.`

Value details:

- **List tags are not split.** `"red, blue"` for `Keywords` is written as one keyword, `red, blue`. Read the file back with [ExifTool Get Metadata](#exiftool-get-metadata) to check what was written.
- Text is sent as UTF-8. Newlines and `&` in a value are escaped for ExifTool and arrive unchanged.
- One leading space of a value is dropped (ExifTool's argument-file parsing removes a single space after `=`), so `" x"` is written as `"x"`.

Since this version (after the code review), a path containing a line break, or a `name` containing a control character, is rejected before ExifTool runs (`errorMessage` `invalid file path` or `invalid tag name`). Earlier versions passed them to ExifTool, where a line break started a new, unintended ExifTool option.

### Example

From the plugin's own test method (`test/Project/Sources/Methods/TEST.4dm`), which works on a copy so the resource stays untouched:

```4d
$folder:=Folder:C1567(Temporary folder:C486; fk platform path:K87:2).folder(Generate UUID:C1066)
$folder.create()

$file:=$file.copyTo($folder)

//--- change EXIF orientation

$tagInfo:=New collection:C1472

$tagInfo.push(New object:C1471("name"; "orientation"; "value"; 6))

$status:=ExifTool Set Metadata($file.platformPath; $tagInfo)

If ($status.success)
	
	READ PICTURE FILE:C678($file.platformPath; $rotated)
	WRITE PICTURE FILE:C680(Folder:C1567(fk resources folder:K87:11).file("rotated.jpg").platformPath; $rotated)
	
	$status:=ExifTool Get Metadata($file.platformPath)
	
	If ($status.success)
		$tagInfo:=$status.tagInfo
		SET TEXT TO PASTEBOARD:C523(JSON Stringify:C1217($tagInfo; *))
	End if 
	
End if 
```

Several kinds of value in one call:

```4d
$tags:=New collection
$tags.push(New object("name"; "Artist"; "value"; "Keisuke Miyako"))  // text
$tags.push(New object("name"; "Copyright"; "value"; Null))  // delete
$tags.push(New object("name"; "Orientation"; "value"; 1))  // raw number: Horizontal (normal)

$status:=ExifTool Set Metadata($file.platformPath; $tags)
If (Not($status.success))
	ALERT($status.errorMessage)
End if 
```

Writing binary data (for example a new thumbnail read from a file):

```4d
DOCUMENT TO BLOB($thumbnailPath; $data)
BLOB TO PICTURE($data; $pict; ExifTool data encapsulation)

$tags:=New collection(New object("name"; "ThumbnailImage"; "data"; $pict))
$status:=ExifTool Set Metadata($file.platformPath; $tags)
```

---

## Error handling & troubleshooting

- **Always check `success`.** Neither command raises a 4D error. A missing file returns `success` `False` with `errorMessage` `Error: File not found - …`.
- **Nothing is found with a POSIX path.** Pass `File.platformPath`, not `File.path`.
- **`errorMessage` says ExifTool could not be started.** The `exiftool` script in `Contents/MacOS` must exist and be executable, and `/usr/bin/perl` must exist. Copying the plugin with a tool that drops the execute permission (some unzip tools) causes this. Since this version the message comes from macOS (for example `The file “exiftool” doesn’t exist.`). Earlier versions returned no result at all in this case.
- **Large files time out.** After 3 seconds (read) or 5 seconds (write), ExifTool is asked to quit, then killed 2 seconds later. A read then returns whatever ExifTool printed so far, which can be an incomplete `tagInfo` with `success` `True`. A write usually reports failure; check the file. Large videos and files on slow network volumes are the likely cases.
- **Output over 64 KB was cut off in earlier versions.** Before this version, ExifTool's output was read only after it exited, so a file with a large embedded preview hit the time limit and lost every tag after the first 64 KB. Fixed in this version.
- **A `*_original` file appears next to the image.** That is ExifTool's backup of the file before [ExifTool Set Metadata](#exiftool-set-metadata) changed it.
- **A tag without `name` in `tagInfo`.** Its value is empty; see [ExifTool Get Metadata](#exiftool-get-metadata).
- **`Keywords` written as one item.** List values are not split on `", "`; see [ExifTool Set Metadata](#exiftool-set-metadata).
- **An element of `tags` without `value` wrote `0`.** Give each element a Text, Null or numeric `value`, or a `data` picture.

---

## Quick reference

```4d
// read
$status:=ExifTool Get Metadata($file.platformPath)
If ($status.success)
	$orientation:=$status.tagInfo.query("name == :1"; "Orientation")
	$thumb:=$status.tagInfo.query("name == :1"; "ThumbnailImage")
	If ($thumb.length#0)
		PICTURE TO BLOB($thumb[0].data; $data; ExifTool data encapsulation)
	End if 
End if 

// write
$tags:=New collection
$tags.push(New object("name"; "Artist"; "value"; "Me"))  // text
$tags.push(New object("name"; "Copyright"; "value"; Null))  // delete
$tags.push(New object("name"; "Orientation"; "value"; 6))  // raw number
$status:=ExifTool Set Metadata($file.platformPath; $tags)  // leaves <file>_original
```
