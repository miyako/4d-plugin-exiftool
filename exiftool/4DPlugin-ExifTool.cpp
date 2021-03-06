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

void PluginMain(PA_long32 selector, PA_PluginParameters params) {
    
	try
	{
        switch(selector)
        {
                
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

#if VERSIONMAC
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
#endif

#if VERSIONWIN
static bool createTask(std::wstring args,
                       std::wstring filepath,
                       PROCESS_INFORMATION *pi,
                       STARTUPINFO *si) {

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
    
    std::wstring currentDirectoryPath = windows64Path;
    
    std::wstring exexutablePath = currentDirectoryPath;
    exexutablePath += L"exiftool.exe";
    
    std::wstring arguments;
    
    arguments += L"\"";
    arguments += exexutablePath;
    //remove delimiter to avoid open-ended escape sequence
    if(arguments.at(arguments.size() - 1) == L'\\')
        arguments = arguments.substr(0, arguments.size() - 1);
    arguments += L"\" ";
    
    if(filepath.length()) {
        
        arguments += L"\"";
        arguments += filepath;
        //remove delimiter to avoid open-ended escape sequence
        if(arguments.at(arguments.size() - 1) == L'\\')
            arguments = arguments.substr(0, arguments.size() - 1);
        arguments += L"\" ";
        
    }
    
    arguments += args;
    
    wchar_t *commandLine = arguments.c_str();
    
    if (CreateProcess(
                      NULL,
                      commandLine,
                      NULL,
                      NULL,
                      TRUE, //handles are inherited
                      0,    //CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT
                      NULL, //pointer to the environment block for the new process
                      currentDirectoryPath.c_str(),
                      si,
                      pi
                      ))
    {
        return true;
    }
    
    return false;
}
#endif

#if VERSIONMAC
static bool waitUntilExitWithTimeout(NSTask *task, CFTimeInterval TO, BOOL SENDTERM, BOOL SENDKILL) {
    
    CFAbsoluteTime      started;
    CFAbsoluteTime      passed;
    BOOL                exited = NO;

    started = CFAbsoluteTimeGetCurrent();
    for (
         CFAbsoluteTime now = started;
         !exited && ((passed = now - started) < TO);
         now = CFAbsoluteTimeGetCurrent()
         )
    {
        if (![task isRunning])
        {
            exited = YES;
        } else {

            CFAbsoluteTime sleepTime = 0.1;
            useconds_t sleepUsec = round(sleepTime * 1000000.0);
            if (sleepUsec == 0) sleepUsec = 1;
            PA_YieldAbsolute();
        }
    }

    if (!exited)
    {
        //NSLog(@"%@ didn't exit after timeout of %0.2f sec", self, TO);

        if (SENDTERM)
        {
            TO = 2; // 2 second timeout, waiting for SIGTERM to kill process

            [task terminate];

            started = CFAbsoluteTimeGetCurrent();
            for (
                 CFAbsoluteTime now = started;
                 !exited && ((passed = now - started) < TO);
                 now = CFAbsoluteTimeGetCurrent()
                 )
            {
                if (![task isRunning])
                {
                    exited = YES;
                } else {
                    PA_YieldAbsolute();
                }
            }
        }

        if (!exited && SENDKILL)
        {
            TO = 2; // 2 second timeout, waiting for SIGKILL to kill process

            pid_t pid = [task processIdentifier];
            kill(pid, SIGKILL);

            started = CFAbsoluteTimeGetCurrent();
            for (
                 CFAbsoluteTime now = started;
                 !exited && ((passed = now - started) < TO);
                 now = CFAbsoluteTimeGetCurrent()
                 )
            {
                if (![task isRunning])
                {
                    exited = YES;
                } else {
                    PA_YieldAbsolute();
                }
            }
        }
    }

    return exited;
}

static NSTask *createTask() {

    NSTask *task = nil;
    
    NSBundle *thisBundle = [NSBundle bundleWithIdentifier:@"com.4D.ExifTool"];
    
    if(thisBundle){
        
        NSString *exifToolPath = [thisBundle pathForAuxiliaryExecutable:@"exiftool"];
        if(exifToolPath) {
            task = [[NSTask alloc]init];
            [task setCurrentDirectoryPath:[exifToolPath stringByDeletingLastPathComponent]];
            [task setLaunchPath:exifToolPath];
        }
        
    }
    
    return task;
}
#endif

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

// Unescape C-style escape sequences in a null-terminated string
// (as found in values of exiftool -php output)
// Returns: number of bytes in unescaped data (including null terminator)
// - on return string contains binary data which may have embedded zero bytes
static int unescape(char *str) {
    
    char *src = strchr(str, '\\');
    // return string length without converting if string doesn't contain escapes
    if (!src) return((int)(strlen(str) + 1));
    char *dst = src;
    for (;;) {
        char ch = *(src++);
        if (ch == '\\') {
            // must un-escape this character
            ch = *(src++);
            switch (ch) {
                case 'x':
                    // decode 2-digit hex character
                    ch = 0;
                    for (int i=0; i<2; ++i) {
                        char nibble = *(src++);
                        if (nibble >= '0' && nibble <= '9') {
                            nibble -= '0';
                        } else if (nibble >= 'A' && nibble <= 'F') {
                            nibble -= 'A' - 10;
                        } else if (nibble >= 'a' && nibble <= 'f') {
                            nibble -= 'a' - 10;
                        } else {
                            ch = 0; // (shouldn't happen)
                            break;
                        }
                        ch = (ch << 4) + nibble;
                    }
                    break;
                case 't':
                    ch = '\t';
                    break;
                case 'n':
                    ch = '\n';
                    break;
                case 'r':
                    ch = '\r';
                    break;
                case '\0':  // (shouldn't happen, but just to be safe)
                    *(dst++) = ch;
                    return((int)(dst - str));
                default:
                    // pass any other character straight through
                    break;
            }
            *(dst++) = ch;
        } else {
            *(dst++) = ch;
            if (!ch) break;
        }
    }
    return((int)(dst - str));
}


static void getResult(PA_ObjectRef status,
                      const char *stdOut,
                      size_t stdOutLen,
                      const char *stdErr,
                      size_t stdErrLen) {
    
//    std::string stdOutStr(stdOut, stdOutLen);
    std::string stdErrStr(stdErr, stdErrLen);
    
//    ob_set_s(status, L"stdOut", stdOutStr.c_str());
    ob_set_s(status, L"errorMessage", stdErrStr.c_str());
    
}

static int ExifTool_SetNewValue(TagInfo *mWriteInfo, const char *tag, const char *value, int len=-1) {
    
    int numSet = 0;
    if (tag) {
        TagInfo *info = new TagInfo;
        if (!info) return -3;
        info->name = new char[strlen(tag) + 1];
        if (!info->name) { delete info; return -3; }
        strcpy(info->name, tag);
        if (value) {
            if (len < 0) {
                if (value) len = (int)strlen(value);
                else len = 0;
            }
            if (len) {
                info->value = new char[len+1];
                if (!info->value) { delete info; return -3; }
                memcpy(info->value, value, len);
                // add null terminator (but note that we don't use it)
                info->value[len] = '\0';
                info->valueLen = len;
            }
        }
        // place at the end of the linked list
        TagInfo **pt = &mWriteInfo;
        while (*pt) {
            ++numSet;
            pt = &((*pt)->next);
        }
        *pt = info;
        ++numSet;
    } else {
        delete mWriteInfo;
        mWriteInfo = NULL;
    }
    return numSet;
}

static TagInfo *ExifTool_GetInfo(char *pt) {
    
    TagInfo *info = (TagInfo *)NULL;
    TagInfo *next = (TagInfo *)NULL;
    TagInfo *infoList = (TagInfo *)NULL;
    
    if (!pt) return info;
    
    int mode = 0;   // 0=looking for tag name, 1=tag properties
    
    for (;;) {
        // find the end of this line
        char *p0 = pt;
        pt = strchr(pt, '\n');
        if (!pt) break;
        *pt = '\0'; // null terminate this line
        ++pt;       // continue at next line
        // scan for opening quote
        p0 = strchr(p0, '"');
        if (!p0) {
            // no quote on line, so this must be the end of the tag info,
            // so fill in necessary members of this TagInfo structure
            if (info) {
                // (name and value are guaranteed to exist)
                if (!info->value) {
                    info->value = new char[1];
                    if (!info->value) break;
                    info->value[0] = '\0';
                }
                if (!info->num) {
                    info->num = info->value;
                    info->numLen = info->valueLen;
                }
            }
            mode = 0;               // look for next tag name
            continue;
        }
        char *p1 = ++p0;
        if (!mode) {    // looking for new tag
            if (next) delete next;  // delete old unused structure if necessary
            // create new TagInfo structure for this tag
            next = new TagInfo;
            if (!next) break;
            // extract tag/group names
            int g = 0;
            for (;;) {
                char ch = *p1;
                if (ch == '"' || ch == ':') {
                    int n = (int)(p1 - p0);
                    char *str = new char[n + 1];
                    if (!str) break;
                    memcpy(str, p0, n);
                    str[n] = '\0';
                    if (ch == '"') {
                        next->name = str;   // save tag name
                        break;
                    }
                    if (g > 2) {
                        // get copy number
                        if (!memcmp(str, "Copy", 4)) {
                            next->copyNum = atoi(str+4);
                            delete [] str; // done with this string
                        }
                    } else {
                        next->group[g] = str;   // save group name
                    }
                    ++g;
                    p0 = p1 + 1;
                }
                ++p1;
            }
            if (!next->name) continue;
            // file name given by line like:  "SourceFile" => "images/a.jpg",
            if (!strcmp(next->name,"SourceFile")) {
                char *p2 = pt - 2;
                if (*p2 == '\r') --p2; // skip Windows CR
                if (*p2 == ',') --p2;
                if (*p2 != '"') continue;
                int n = (int)(p2 - p1 - 6);
                if (n < 0) continue;
                char *str = new char[n+1];
                if (!str) break;
                memcpy(str, p1+6, n);
                str[n] = '\0';
                next->value = next->num = str;
                next->valueLen = next->numLen = n;
            } else {
                mode = 1;   // read tag properties next
            }
            // add to linked list of information
            if (info) {
                info->next = next;
            } else {
                infoList = next;
            }
            info = next;
            next = NULL;
        } else {
            // isolate the property name
            p1 = strchr(p0, '"');
            if (!p1) break;         // (shouldn't happen)
            *p1 = '\0';             // null terminate property name
            p1 += 5;                // step to start of value
            if (p1 >= pt) break;    // (shouldn't happen);
            if (*p1 == '"') ++p1;   // skip quote if it exists
            char *p2 = pt - 1;
            if (p2[-1] == '\r') --p2;// skip Windows CR
            if (p2[-1] == ',') --p2;// skip trailing comma
            if (p2[-1] == '"') --p2;// skip trailing quote
            if (p2 < p1) break;     // (shouldn't happen)
            *p2 = '\0';             // null terminate property value
            int n = unescape(p1);   // unescape characters in property value
            char **dst;
            if (!strcmp(p0, "desc")) {
                dst = &info->desc;
            } else if (!strcmp(p0, "id")) {
                dst = &info->id;
            } else if (!strcmp(p0, "num")) {
                dst = &info->num;
                info->numLen = n - 1;   // save length too (could be binary data)
            } else if (!strcmp(p0, "val")) {
                dst = &info->value;
                info->valueLen = n - 1; // save length too (could be binary data)
            } else {
                continue;   // (shouldn't happen)
            }
            *dst = new char[n];
            if (!*dst) break;
            memcpy(*dst, p1, n);    // copy property value
        }
    }
    if (next) delete next;
    return infoList;
}

static void ExifTool_Get_Metadata(PA_PluginParameters params) {

    PA_Unistring *arg1 = PA_GetStringParameter(params, 1);
    PA_ObjectRef status = PA_CreateObject();
    
    ob_set_b(status, L"success", false);

#if VERSIONMAC
    std::string imagePath;
#endif
#if VERSIONWIN
    std::wstring imagePath;
#endif
    
    TagInfo *info = NULL;
    
    if(arg1) {
        
#if VERSIONMAC
        CUTF16String u16((const PA_Unichar *)arg1->fString, arg1->fLength);
        std::string hfs;
        u16_to_u8(u16, hfs);
        convert_hfs_to_posix(hfs, imagePath);
#endif
#if VERSIONWIN
        imagePath = std::wstring((const wchar_t *)arg1->fString, arg1->fLength);
#endif
    }
    
#if VERSIONWIN
    
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;
    
    HANDLE stdOutPipeR = NULL;
    HANDLE stdOutPipeW = NULL;

    HANDLE stdInPipeR = NULL;
    HANDLE stdInPipeW = NULL;

    HANDLE stdErrPipeR = NULL;
    HANDLE stdErrPipeW = NULL;
    
    if (CreatePipe(&stdOutPipeR, &stdOutPipeW, &saAttr, 0)) {
        if (SetHandleInformation(stdOutPipeR, HANDLE_FLAG_INHERIT, 0) ) {
            if (CreatePipe(&stdInPipeR, &stdInPipeW, &saAttr, 0)) {
                if (SetHandleInformation(stdInPipeW, HANDLE_FLAG_INHERIT, 0) ) {
                    if (CreatePipe(&stdErrPipeR, &stdErrPipeW, &saAttr, 0)) {
                        if (SetHandleInformation(stdErrPipeR, HANDLE_FLAG_INHERIT, 0) ) {
                         
                            PROCESS_INFORMATION pi;
                            STARTUPINFO si;
                            
                            ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
                            ZeroMemory(&si, sizeof(STARTUPINFO));
                            
                            si.cb = sizeof(STARTUPINFO);
                            si.hStdError  = stdErrPipeW;
                            si.hStdOutput = stdOutPipeW;
                            si.hStdInput  = stdInPipeR;
                            si.dwFlags |= STARTF_USESTDHANDLES;
                            si.wShowWindow = SW_HIDE;
                            
                            if(createTask(L"-b -php -l -G:0:1:2:4 -D -sep , ",
                                       imagePath,
                                          &pi, &si)) {
                                
                                DWORD dwRead
                                #define BUFSIZE 4096
                                CHAR chBuf[BUFSIZE];
                                
                                std::vector<unsigned char>stdOutData(0);
                                size_t pos = 0;
                                
                                for (;;)
                                {
                                    bool bSuccess = ReadFile(stdOutPipeR, chBuf, BUFSIZE, &dwRead, NULL);
                                    if( ! bSuccess || dwRead == 0 ) break;
                                    
                                    stdOutData.resize(dwRead);
                                    memcpy(&stdOutData[pos], chBuf, dwRead);
                                    pos += dwRead;
                                    
                                }
                                
                                CloseHandle(stdInPipeW);
                                
                                std::vector<unsigned char>stdErrData(0);
                                pos = 0;
                                
                                for (;;)
                                {
                                    bool bSuccess = ReadFile(stdErrPipeR, chBuf, BUFSIZE, &dwRead, NULL);
                                    if( ! bSuccess || dwRead == 0 ) break;
                                    
                                    stdErrData.resize(dwRead);
                                    memcpy(&stdErrData[pos], chBuf, dwRead);
                                    pos += dwRead;
                                    
                                }
                                
                                getResult(status,
                                          (const char *)&stdOutData[0],
                                          stdOutData.size(),
                                          (const char *)&stdErrData[0],
                                          stdErrData.size());
                                
                                info = ExifTool_GetInfo((char *)&stdOutData[0]);
                                
                            }else{
                                CloseHandle(pi.hProcess);
                                CloseHandle(pi.hThread);
                                CloseHandle(stdOutPipeW);
                                CloseHandle(stdErrPipeW);
                                CloseHandle(stdInPipeR);
                            }
                            
                            DWORD exitCode;
                            if (GetExitCodeProcess(pi.hProcess, &exitCode))
                            {
                                if(exitCode == STILL_ACTIVE)
                                {
                                    TerminateProcess(pi.hProcess, 1);
                                }
                            };
                            
                        }
                    }
                }
            }
        }
    }
#endif
        
#if VERSIONMAC
    NSTask *task = createTask();
    
    if(task) {
     
        [task setArguments: @[[NSString stringWithUTF8String:imagePath.c_str()],
                              @"-b",
                              @"-php",
                              @"-l",
                              @"-G:0:1:2:4",
                              @"-D",
                              @"-sep",
                              @", "]];
        
        NSPipe *stdOut = [NSPipe pipe];
        [task setStandardOutput:stdOut];
        NSFileHandle *stdOutFile = stdOut.fileHandleForReading;
        
        NSPipe *stdErr = [NSPipe pipe];
        [task setStandardError:stdErr];
        NSFileHandle *stdErrFile = stdErr.fileHandleForReading;
        
        [task launch];

        waitUntilExitWithTimeout(task, 3, true, true);
        
        [task terminate];
        
        [task release];
        
        NSData *stdOutFileData = [stdOutFile readDataToEndOfFile];
        [stdOutFile closeFile];
        
        NSData *stdErrFileData = [stdErrFile readDataToEndOfFile];
        [stdErrFile closeFile];
        
        getResult(status,
                  (const char *)[stdOutFileData bytes],
                  [stdOutFileData length],
                  (const char *)[stdErrFileData bytes],
                  [stdErrFileData length]);
        
        info = ExifTool_GetInfo((char *)[stdOutFileData bytes]);
    }
#endif
    
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
        
        ob_set_b(status, L"success", true);
    }
    
    PA_ReturnObject(params, status);
}

static int ExifTool_GetSummary(const char *msg, char *stdOut, size_t len)
{
    for (int out=0; out<2; ++out) {
        // check stderr first because it isn't likely to be too long
        char *str = stdOut;
        if (!str) continue;
        char *pt = strstr(str, msg);
        if (!pt || pt - str < 2 || pt[-1] != ' ' || !isdigit(pt[-2])) continue;
        char ch = pt[strlen(msg)];
        if (ch != '\n' && ch != '\r') continue; // message must end with a newline
        pt -= 2;
        while (pt > str && isdigit(pt[-1])) --pt;
        return atoi(pt);
    }
    return -1;  // message not found
}

static int GetSummary_Error(char *stdOut, size_t len) {
    
#define SUMMARY_FILE_UPDATE_ERRORS      "files weren't updated due to errors"
 
    return ExifTool_GetSummary(SUMMARY_FILE_UPDATE_ERRORS, stdOut, len);
}

static int GetSummary_Success(char *stdOut, size_t len) {
    
#define SUMMARY_IMAGE_FILES_UPDATED     "image files updated"
 
    return ExifTool_GetSummary(SUMMARY_IMAGE_FILES_UPDATED, stdOut, len);
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

#pragma mark -

static int ExifTool_WriteInfo(const char *file, TagInfo *info, std::vector<char>& buf) {
    
    if (!file) return -5;
        
    // 12 extra characters for "-ex\n-sep\n, \n"
    int olen = 12 + 29;
    
    const int kBlockSize = buf.size();
    int size = kBlockSize;
    strcpy(&buf[0], file);
    int pos = (int)strlen(file);
    buf[pos++] = '\n';
    int escaped = 0;
    
    if (!info) return -4;
    
    // prepare assignment arguments for exiftool, looping through all tags to write
    while (info) {
        if (!info->name || strlen(info->name) > 100 || !strcmp(info->name, "SourceFile")) {
            info = info->next;
            continue;
        }
        // construct the tag name
        char tag[1024];
        tag[0] = '\0';
        for (int i=0; i<3; ++i) {
            if (info->group[i] && strlen(info->group[i]) < 100) {
                char *pt = strchr(tag, '\0');
                *(pt++) = '0' + i;          // leading family number
                strcpy(pt, info->group[i]); // group name
                strcat(tag,":");            // colon separator
            }
        }
        strcat(tag, info->name);
        // which value are we writing (converted or numerical?)
        char *val = info->value;
        int valLen = info->valueLen;
        if (!val) {
            val = info->num;
            valLen = info->numLen;
            if (val) strcat(tag, "#");  // write numerical value
        }
        int tagLen = (int)strlen(tag);
        int origLen = valLen;
        // count the number of characters in the value that need escaping
        if (val) {
            char *pt = val;
            char *end = pt + origLen;
            int count = 0;
            while (pt < end) {
                char ch = *(pt++);
                if (ch==10 || ch=='&' || ch=='\0') ++count;
            }
            valLen += count * 4;   // 4 extra bytes for each escaped character
        }
        // prepare exiftool argument (format is: "-TAG=VALUE\n")
        int n = tagLen + valLen + 3;
        // expand buffer if necessary
        if (pos + n + olen > size) {
            size += n + kBlockSize;
            buf.resize(size);
        }
        buf[pos++] = '-';
        memcpy(&buf[pos], tag, tagLen);
        pos += tagLen;
        buf[pos++] = '=';
        if (valLen == origLen) {
            if (val) {
                memcpy(&buf[pos], val, valLen);
                pos += valLen;
            }
        } else {
            // escape newlines and '&' characters in the value
            char *pt = val;
            char *end = pt + origLen;
            char *dst = &buf[pos];
            while (pt < end) {
                char ch = *(pt++);
                if (ch == 10) {
                    memcpy(dst, "&#10;", 5);
                    dst += 5;
                } else if (ch == 0) {
                    memcpy(dst, "&#00;", 5);
                    dst += 5;
                } else if (ch == '&') {
                    memcpy(dst, "&amp;", 5);
                    dst += 5;
                } else {
                    *(dst++) = ch;
                }
            }
            pos = (int)(dst - &buf[0]);
            escaped = 1;
        }
        buf[pos++] = '\n';
        info = info->next;
    }
    
    // get exiftool to unescape our newlines if necessary
    if (escaped) { strcpy(&buf[pos], "-ex\n");  pos += 4; }

//    strcpy(&buf[pos], "-overwrite_original_in_place\n");  pos += 29;
    
    // split concatenated lists back into individual items
    strcpy(&buf[pos], "-sep\n, ");  pos += 8;
    
    return 0;
}

static void ExifTool_Set_Metadata(PA_PluginParameters params) {

    PA_Unistring *arg1 = PA_GetStringParameter(params, 1);
    PA_ObjectRef status = PA_CreateObject();
    
    ob_set_b(status, L"success", false);

    std::string imagePath;
    
    TagInfo *info = NULL;
    
    if(arg1) {
        CUTF16String u16((const PA_Unichar *)arg1->fString, arg1->fLength);
        std::string hfs;
        u16_to_u8(u16, hfs);
#if VERSIONMAC
        convert_hfs_to_posix(hfs, imagePath);
#endif
    }
    
    TagInfo mWriteInfo;
    
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
                            ExifTool_SetNewValue(&mWriteInfo, (const char *)name.c_str(), (const char *)value.c_str());
                            continue;
                        }
                        
                        if(ob_is_defined_but_null(o, L"value")) {
                            ExifTool_SetNewValue(&mWriteInfo, (const char *)name.c_str(), NULL);
                            continue;
                        }
                        
                        std::vector<unsigned char>buf(0);
                        if(ob_is_encapsuated_data(o, L"data", buf)) {
                            ExifTool_SetNewValue(&mWriteInfo, (const char *)name.c_str(), (const char *)&buf[0], buf.size());
                            continue;
                        }
                        
                        double numValue = ob_get_n(o, L"value");
                        char _numValue[27];
                        sprintf(_numValue, "%lf", numValue);
                        name += (const uint8_t *)"#";
                        value = (const uint8_t *)_numValue;
                        ExifTool_SetNewValue(&mWriteInfo, (const char *)name.c_str(), (const char *)value.c_str());
                    }
                }
            }
        }
    
    }
    
    const int kBlockSize = 65536;
    std::vector<char>buf(kBlockSize);
    
    ExifTool_WriteInfo(imagePath.c_str(), &mWriteInfo, buf);
    
#if VERSIONMAC
    NSTask *task = createTask();
    
    if(task) {
        
        [task setArguments: @[
            @"-@",
            @"-"]];
        
        NSPipe *stdOut = [NSPipe pipe];
        [task setStandardOutput:stdOut];
        NSFileHandle *stdOutFile = stdOut.fileHandleForReading;
        
        NSPipe *stdErr = [NSPipe pipe];
        [task setStandardError:stdErr];
        NSFileHandle *stdErrFile = stdErr.fileHandleForReading;
        
        NSPipe *stdin = [NSPipe pipe];
        [task setStandardInput:stdin];
        NSFileHandle *stdinFile = stdin.fileHandleForWriting;
        
        fcntl(stdinFile.fileDescriptor, F_SETNOSIGPIPE, 1);
        
        [task launch];

        [stdinFile writeData:[NSData dataWithBytes:&buf[0] length:buf.size()]];
        [stdinFile closeFile];
        
        waitUntilExitWithTimeout(task, 5, true, true);
        
        NSData *stdOutFileData = [stdOutFile readDataToEndOfFile];
        [stdOutFile closeFile];
        NSData *stdErrFileData = [stdErrFile readDataToEndOfFile];
        [stdErrFile closeFile];
        
        [task release];
                
        int nmFilesOK = GetSummary_Success((char *)[stdOutFileData bytes], [stdOutFileData length]);
        int nmFilesNG = GetSummary_Error((char *)[stdOutFileData bytes], [stdOutFileData length]);
        
        if((nmFilesOK == 1) && (nmFilesNG <= 0)) {
            ob_set_b(status, L"success", true);
        }else{
            getResult(status,
                      (const char *)[stdOutFileData bytes],
                      [stdOutFileData length],
                      (const char *)[stdErrFileData bytes],
                      [stdErrFileData length]);
        }
    
    }
#endif

    PA_ReturnObject(params, status);
}

#pragma mark -

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
