# 4d-plugin-exiftool
4D interface for ExifTool (by Phil Harvey)

based on:

* [C++ Interface for ExifTool (by Phil Harvey)](https://exiftool.org/cpp_exiftool/)

objective:

* easiler deployment with custom 4D application
* more poweful than native [SET PICTURE METADATA](https://doc.4d.com/4Dv19/4D/19.1/SET-PICTURE-METADATA.301-5652803.en.html)

design:

* internally create a single stay-open `ExifTool` object

## SCARD Get readers

```4d
tagInfo:Object:=ExifTool ImageInfo(file:Text{; opts:Collection; {timeout:Real}})
```

|パラメーター|データ型|説明|
|-|-|-|
|file|Text||
|opts|Collection||
|timeout|Real||
|status|Object||
