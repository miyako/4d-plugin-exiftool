//%attributes = {}
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