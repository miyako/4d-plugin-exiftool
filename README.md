![version](https://img.shields.io/badge/version-19%2B-5682DF)
![platform](https://img.shields.io/static/v1?label=platform&message=mac-intel%20|%20mac-arm&color=blue)
[![license](https://img.shields.io/github/license/miyako/4d-plugin-exiftool)](LICENSE)
![downloads](https://img.shields.io/github/downloads/miyako/4d-plugin-exiftool/total)

# 4d-plugin-exiftool
4D interface for ExifTool (by Phil Harvey)

Mac only; Visual Studio does not have POSIX `fcntl`

**TODO**: refactor pipe with platform SDK

**TODO**: `NSTask` with timeout

reference: https://github.com/miyako/4d-plugin-midi

based on:

* [C++ Interface for ExifTool (by Phil Harvey)](https://exiftool.org/cpp_exiftool/)

objective:

* easiler deployment with custom 4D application
* more poweful than native [SET PICTURE METADATA](https://doc.4d.com/4Dv19/4D/19.1/SET-PICTURE-METADATA.301-5652803.en.html)

design:

* internally create a single stay-open `ExifTool` object

## ExifTool Get Metadata

* ImageInfo
* GetError

```4d
status:Object:=Get Metadata(file:Text)
```
|Parameter|Type|Description|
|-|-|-|
|file|Text|platform path of image file|
|status|Object||

binary data is returned as encapsulated picture (see below).

numeric value is returned as string. (no way ti differentiate `"0"` from empty value).

string data is returned as UTF-8.

data that can not be converted to UTF-8 and back is considered binary.

## ExifTool Set Metadata

* repeat {SetNewValue}
* WriteInfo
* Complete
* GetSummary
* GetOutput
* GetError

```4d
status:Object:=Set Metadata(file:Text; tags:Collection)
```

|Parameter|Type|Description|
|-|-|-|
|file|Text|platform path of image file|
|tags|Collection|value may be Text, Null or numerial (`#` added internally). to support 4D version < 19 R2, implementation does not use `4D.Blob` capsule object (see below)|
|status|Object||

to pass binary data, convert it to `Picture` using the custom UTI:

```4d
BLOB TO PICTURE($BLOB; $PICT; "private.exiftool.data")
```

constant `ExifTool data encapsulation` is exported by the plugin.

```4d
/*
	example of binary tag; returned as encapsulated picture, retrivable as BLOB
*/

$pict:=$status.tagInfo.query("name == :1"; "ThumbnailImage")[0].data
PICTURE TO BLOB($pict; $data; ExifTool data encapsulation)
BLOB TO DOCUMENT(System folder(Desktop)+"test.jpeg"; $data)
```

## Known issues

releasing the `ExifTool` seems to freeze 4D. it is not released for now.
