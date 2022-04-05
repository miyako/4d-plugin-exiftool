# 4d-plugin-exiftool
4D interface for ExifTool (by Phil Harvey)

based on:

* [C++ Interface for ExifTool (by Phil Harvey)](https://exiftool.org/cpp_exiftool/)

objective:

* easiler deployment with custom 4D application
* more poweful than native [SET PICTURE METADATA](https://doc.4d.com/4Dv19/4D/19.1/SET-PICTURE-METADATA.301-5652803.en.html)

design:

* internally create a single stay-open `ExifTool` object

## ExifTool Set Metadata

* repeat {SetNewValue}
* WriteInfo
* Complete
* GetError


```4d
status:Object:=Set Metadata(file:Text; opts:Collection; {timeout:Real})
```

|パラメーター|データ型|説明|
|-|-|-|
|file|Text||
|opts|Collection|KVP; value may be Text, Null or numerial (`#` added internally). to support 4D version < 19 R2, implementation does not use `4D.Blob` object capsule.|
|timeout|Real||
|status|Object||
