/* --------------------------------------------------------------------------------
 #
 #  4DPlugin-ExifTool.cpp
 #	source generated by 4D Plugin Wizard
 #	Project : ExifTool
 #	author : miyako
 #	2022/04/06
 #  
 # --------------------------------------------------------------------------------*/

#include "4DPlugin-ExifTool.h"

#pragma mark -

#if VERSIONMAC
static void getExifToolPath(std::string& path) {

    NSBundle *thisBundle = [NSBundle bundleWithIdentifier:@"com.4D.ExifTool"];
    if(thisBundle){
        NSString *exifToolPath = [thisBundle pathForAuxiliaryExecutable:@"exiftool"];
        if(exifToolPath){
            path = [exifToolPath UTF8String];
        }
    }
}
#endif

#if VERSIONWIN
static void getExifToolPath(std::string& path) {
    
    wchar_t    thisPath[_MAX_PATH] = {0};
    wchar_t    fDrive[_MAX_DRIVE], fDir[_MAX_DIR], fName[_MAX_FNAME], fExt[_MAX_EXT];
    HMODULE hplugin = GetModuleHandleW(L"ExifTool.4DX");
    GetModuleFileNameW(hplugin, thisPath, _MAX_PATH);
    _wsplitpath_s(thisPath, fDrive, fDir, fName, fExt);
    std::wstring path = fDrive;
    path += fDir;//path to plugin parent folder
    if(path.at(path.size() - 1) == L'\\')//remove delimiter
        path = path.substr(0, path.size() - 1);
    _wsplitpath_s(path.c_str(), fDrive, fDir, fName, fExt);
    wchar_t windows64Path[_MAX_PATH] = {0};
    _wmakepath_s(windows64Path, fDrive, fDir, L"Windows64\\", NULL);
    CUTF16String wpath = windows64Path;
    wpath += L"exiftool.exe";
    
    u16_to_u8(wpath, path);
}
#endif

ExifTool *et = NULL;

static void OnStartup() {
    
    std::string exifToolPath;
    getExifToolPath(exifToolPath);
    
    et = new ExifTool(exifToolPath.c_str());
}

void PluginMain(PA_long32 selector, PA_PluginParameters params) {
    
	try
	{
        switch(selector)
        {
            case kInitPlugin :
            case kServerInitPlugin :
                OnStartup();
                break;
                
			// --- ExifTool
            
			case 1 :
				ExifTool_Get_Metadata(params);
				break;
			case 2 :
				ExifTool_Set_Metadata(params);
				break;

        }

	}
	catch(...)
	{

	}
}

static void convert_hfs_to_posix(std::string hfs, std::string& posix) {
    
    NSString *h = [[NSString alloc]initWithUTF8String:hfs.c_str()];
    if(h) {
        NSURL *u = (NSURL *)CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                                          (CFStringRef)h,
                                                          kCFURLHFSPathStyle, false);
        if(u) {
            NSString *p = (NSString *)CFURLCopyFileSystemPath((CFURLRef)u, kCFURLPOSIXPathStyle);
            if(p) {
                posix = [p UTF8String];
                [p release];
            }
            [u release];
        }
        [h release];
    }
}

#pragma mark -

static void getData(PA_ObjectRef tag, TagInfo *i) {
    
    if ((i->value) && (i->valueLen > 0)) {

        PA_Variable parameters[3];
        
        parameters[0] = PA_CreateVariable(eVK_Blob);
        parameters[1] = PA_CreateVariable(eVK_Picture);
        parameters[2] = PA_CreateVariable(eVK_Unistring);
        
        PA_SetBlobVariable(&parameters[0], i->value, i->valueLen);
        PA_Unistring u = PA_CreateUnistring((PA_Unichar *)"p\0r\0i\0v\0a\0t\0e\0.\0e\0x\0i\0f\0t\0o\0o\0l\0.\0d\0a\0t\0a\0\0\0");
        PA_SetStringVariable(&parameters[2], &u);
        
        PA_ExecuteCommandByID(/*BLOB TO PICTURE*/682, parameters, 3);
        PA_DisposeUnistring(&u);
        PA_Picture pict = PA_GetPictureVariable(parameters[1]);

        ob_set_s(tag, "name", i->name);
        ob_set_p(tag, L"data", PA_DuplicatePicture( pict, 1));
        
        PA_ClearVariable(&parameters[0]);
        PA_ClearVariable(&parameters[1]);
        PA_ClearVariable(&parameters[2]);
        
    }

}

static void getHeader(PA_ObjectRef tag, TagInfo *i) {
    
    if(i->desc)
        ob_set_s(tag, "description", i->desc);
    
    if(i->id)
        ob_set_s(tag, "id", i->id);
    
    if(i->copyNum)
        ob_set_n(tag, "copyNum", i->copyNum);
    
    if(i->group[0]) {
        ob_set_s(tag, "group[0]", i->group[0]);
    }
    
    if(i->group[1]) {
        ob_set_s(tag, "group[1]", i->group[1]);
    }
    
    if(i->group[2]) {
        ob_set_s(tag, "group[2]", i->group[2]);
    }
    
}

static void getValue(PA_ObjectRef tag, TagInfo *i) {
    
    bool isString = false;
    
    if(i->valueLen != 0) {
        CUTF16String u16;
        std::string u8(i->value, i->valueLen);
        u8_to_u16(u8, u16);
        if(u16.size() != 0) {
            u16_to_u8(u16, u8);
            if(u8.length() == i->valueLen) {
                ob_set_s(tag, "name", i->name);
                ob_set_a(tag, L"value", &u16);
                isString = true;
            }
        }
    }
    
    if(!isString) {
        getData(tag, i);
    }
    
}

void ExifTool_Get_Metadata(PA_PluginParameters params) {

    PA_Unistring *arg1 = PA_GetStringParameter(params, 1);
    PA_ObjectRef status = PA_CreateObject();
    
    ob_set_b(status, L"success", false);
    
    std::string imagePath;
    std::string opt = "-b"; //fixed
    double timeout = 5.0; //seconds
    
    if(et->IsRunning()) {
     
        if(arg1) {
            CUTF16String u16((const PA_Unichar *)arg1->fString, arg1->fLength);
            std::string hfs;
            u16_to_u8(u16, hfs);
    #if VERSIONMAC
            convert_hfs_to_posix(hfs, imagePath);
    #endif
        }

        TagInfo *info = et->ImageInfo(imagePath.c_str(), opt.c_str(), timeout);
        
        if (info) {
            
            PA_CollectionRef tagInfo = PA_CreateCollection();
            
            for (TagInfo *i=info; i; i=i->next) {
                
                PA_ObjectRef tag = PA_CreateObject();
                
                getHeader(tag, i);
                            
                getValue(tag, i);
                
                PA_Variable v = PA_CreateVariable(eVK_Object);
                PA_SetObjectVariable(&v, tag);
                PA_SetCollectionElement(tagInfo, PA_GetCollectionLength(tagInfo), v);
                PA_ClearVariable(&v);
            }
            
            ob_set_c(status, "tagInfo", tagInfo);
            
            delete info;
        }
            
        if (et->LastComplete() > 0) {
            ob_set_b(status, L"success", true);
        }
        
        char *errorMessage = et->GetError();
        if(errorMessage) {
            ob_set_s(status, "errorMessage", errorMessage);
        }
    }
    
    PA_ReturnObject(params, status);
}

static bool ob_is_defined_but_null(PA_ObjectRef obj, const wchar_t *_key) {
    
    bool is_defined = false;
    
    if(obj)
    {
        CUTF16String ukey;
        json_wconv(_key, &ukey);
        PA_Unistring key = PA_CreateUnistring((PA_Unichar *)ukey.c_str());
        
        if(PA_HasObjectProperty(obj, &key))
        {
            PA_Variable v = PA_GetObjectProperty(obj, &key);
            if(PA_GetVariableKind(v) == eVK_Null)
            {
                is_defined = true;
            }
        }
        
        PA_DisposeUnistring(&key);
    }
    
    return is_defined;
}

static bool ob_is_encapsuated_data(PA_ObjectRef obj, const wchar_t *_key, std::vector<unsigned char>& buf) {
    
    bool is_defined = false;
    
    if(obj)
    {
        CUTF16String ukey;
        json_wconv(_key, &ukey);
        PA_Unistring key = PA_CreateUnistring((PA_Unichar *)ukey.c_str());
        
        if(PA_HasObjectProperty(obj, &key))
        {
            PA_Variable v = PA_GetObjectProperty(obj, &key);
            if(PA_GetVariableKind(v) == eVK_Picture)
            {
                PA_Variable parameters[3];
                
                parameters[0] = v;
                parameters[1] = PA_CreateVariable(eVK_Blob);
                parameters[2] = PA_CreateVariable(eVK_Unistring);
                
                PA_Unistring u = PA_CreateUnistring((PA_Unichar *)"p\0r\0i\0v\0a\0t\0e\0.\0e\0x\0i\0f\0t\0o\0o\0l\0.\0d\0a\0t\0a\0\0\0");
                PA_SetStringVariable(&parameters[2], &u);
                
                PA_ExecuteCommandByID(/*PICTURE TO BLOB*/692, parameters, 3);
                PA_DisposeUnistring(&u);
                void *ptr = NULL;
                PA_long32 len = PA_GetBlobVariable(parameters[1], 0L);
                buf.resize(len);
                PA_GetBlobVariable(parameters[1], &buf[0]);
                
                PA_ClearVariable(&parameters[1]);
                PA_ClearVariable(&parameters[2]);
                
                is_defined = true;
            }
        }
        
        PA_DisposeUnistring(&key);
    }
    
    return is_defined;
}

void ExifTool_Set_Metadata(PA_PluginParameters params) {

    PA_Unistring *arg1 = PA_GetStringParameter(params, 1);
    PA_ObjectRef status = PA_CreateObject();
    
    ob_set_b(status, L"success", false);
    
    std::string imagePath;
    double timeout = 5.0; //seconds
    
    if(et->IsRunning()) {
     
        if(arg1) {
            CUTF16String u16((const PA_Unichar *)arg1->fString, arg1->fLength);
            std::string hfs;
            u16_to_u8(u16, hfs);
    #if VERSIONMAC
            convert_hfs_to_posix(hfs, imagePath);
    #endif
        }
        
        PA_CollectionRef tags = PA_GetCollectionParameter(params, 2);
        
        if(tags) {
            PA_long32 count = PA_GetCollectionLength(tags);
            
            for(PA_long32 i = 0; i < count; ++i) {
                
                PA_Variable v = PA_GetCollectionElement(tags, i);
                if(PA_GetVariableKind(v) == eVK_Object) {
                    PA_ObjectRef o = PA_GetObjectVariable(v);
                    if(o) {
                        CUTF8String name;
                        if(ob_get_s(o, L"name", &name)) {
                            CUTF8String value;
                            if(ob_get_s(o, L"value", &value)) {
                                et->SetNewValue((const char *)name.c_str(), (const char *)value.c_str());
                                continue;
                            }
                            
                            if(ob_is_defined_but_null(o, L"value")) {
                                et->SetNewValue((const char *)name.c_str(), NULL);
                                continue;
                            }
                            
                            std::vector<unsigned char>buf(0);
                            if(ob_is_encapsuated_data(o, L"data", buf)) {
                                et->SetNewValue((const char *)name.c_str(), (const char *)&buf[0], buf.size());
                                continue;
                            }
                            
                            double numValue = ob_get_n(o, L"value");
                            char _numValue[27];
                            sprintf(_numValue, "%lf", numValue);
                            name += (const uint8_t *)"#";
                            value = (const uint8_t *)_numValue;
                            et->SetNewValue((const char *)name.c_str(), (const char *)value.c_str());
                        }
                    }
                }
            }
        }

        et->WriteInfo(imagePath.c_str());
        
        int result = et->Complete(timeout);
        
        if (result > 0) {
            
            if(1 == et->GetSummary(SUMMARY_IMAGE_FILES_UPDATED)) {
                ob_set_b(status, L"success", true);
            }

            char *outputMessage = et->GetOutput();
            if(outputMessage) {
                ob_set_s(status, "outputMessage", outputMessage);
            }
            
            char *errorMessage = et->GetError();
            if(errorMessage) {
                ob_set_s(status, "errorMessage", errorMessage);
            }
   
        }
        
    }
    
    PA_ReturnObject(params, status);
}

static void u16_to_u8(CUTF16String& u16, std::string& u8) {

#ifdef _WIN32
    int len = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)u16.c_str(), u16.length(), NULL, 0, NULL, NULL);

    if (len) {
        std::vector<uint8_t> buf(len + 1);
        if (WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)u16.c_str(), u16.length(), (LPSTR)&buf[0], len, NULL, NULL)) {
            u8 = std::string((const char *)&buf[0]);
        }
    }
    else {
        u8 = std::string((const char *)"");
    }

#else
    CFStringRef str = CFStringCreateWithCharacters(kCFAllocatorDefault, (const UniChar *)u16.c_str(), u16.length());
    if (str) {
        size_t size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8) + sizeof(uint8_t);
        std::vector<uint8_t> buf(size);
        CFIndex len = 0;
        CFStringGetBytes(str, CFRangeMake(0, CFStringGetLength(str)), kCFStringEncodingUTF8, 0, true, (UInt8 *)&buf[0], size, &len);
        u8 = std::string((const char *)&buf[0], len);
        CFRelease(str);
    }
#endif
}

static void u8_to_u16(std::string& u8, CUTF16String& u16) {

#ifdef _WIN32
    int len = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)u8.c_str(), u8.length(), NULL, 0);

    if (len) {
        std::vector<uint8_t> buf((len + 1) * sizeof(PA_Unichar));
        if (MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)u8.c_str(), u8.length(), (LPWSTR)&buf[0], len)) {
            u16 = CUTF16String((const PA_Unichar *)&buf[0]);
        }
    }
    else {
        u16 = CUTF16String((const PA_Unichar *)L"");
    }

#else
    CFStringRef str = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)u8.c_str(), u8.length(), kCFStringEncodingUTF8, true);
    if (str) {
        CFIndex len = CFStringGetLength(str);
        std::vector<uint8_t> buf((len + 1) * sizeof(PA_Unichar));
        CFStringGetCharacters(str, CFRangeMake(0, len), (UniChar *)&buf[0]);
        u16 = CUTF16String((const PA_Unichar *)&buf[0]);
        CFRelease(str);
    }
#endif
}
