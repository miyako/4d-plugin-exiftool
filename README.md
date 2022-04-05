# 4d-plugin-exiftool
4D interface for ExifTool (by Phil Harvey)

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
status:Object:=Get Metadata(file:Text{; opts:Text})
```
|parameter|type|description|
|-|-|-|
|file|Text||
|opts|Text|only the following exiftool options are allowed: `-TAG` `-x` `-b` `-c` `-charset` `-d` `-L` `-lang` `-listItem` `-n` `-sep` `-sort` `--a` `-e` `-ee` `-ext` `-F` `-fast` `-fileOrder` `-i` `-if` `-m` `-password` `-r` `-scanForXMP` `-u` `-U` `-z` `-globalTimeShift` `-use` `-@` `-api`|
|status|Object||

## ExifTool Set Metadata

* repeat {SetNewValue}
* WriteInfo
* Complete
* GetSummary
* GetOutput
* GetError

```4d
status:Object:=Set Metadata(file:Text; tags:Object)
```

|parameter|type|description|
|-|-|-|
|file|Text||
|tags|Object|KVP; value may be Text, Null or numerial (`#` added internally). to support 4D version < 19 R2, implementation does not use `4D.Blob` capsule object (see below).|
|status|Object||

to pass binary data, convert it to `Picture` using the custom UTI:

```4d
BLOB TO PICTURE($BLOB; $PICT; "private.exiftool.data")
```

---

generic low level commands (`Command` `GetOutput`) are not implemented for now.
