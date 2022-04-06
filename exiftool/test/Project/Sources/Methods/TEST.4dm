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
	BLOB TO DOCUMENT:C526(System folder:C487(Desktop:K41:16)+"test.jpeg"; $data)
	
	SET TEXT TO PASTEBOARD:C523(JSON Stringify:C1217($tagInfo; *))
	
End if 

$file:=$folder.file("Reconyx_HC500_Hyperfire.jpg")
$file:=$file.copyTo(Folder:C1567(fk desktop folder:K87:19))

$tagInfo:=$tagInfo.query("name == :1"; "ThumbnailImage")

$status:=ExifTool Set Metadata($file.platformPath; $tagInfo)

SET TEXT TO PASTEBOARD:C523(JSON Stringify:C1217($status; *))

$file.delete()