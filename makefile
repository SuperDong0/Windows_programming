# These build flags means:
# 	/MTd: Use multi-thread debug version static CRT
# 	/W4: Use warning level 4
#	/DUNICODE /D_UNICODE: Use unicode code
#	/EHsc: Enable exception handling
#       /ZI: means create the PDB file for debug
CPPFLAGS = /MTd /ZI /W4 /I .\inc /EHsc /nologo
CPPFLAGS = $(CPPFLAGS) /DUNICODE /D_UNICODE
RFLAGS   = /v /fo

DLLFLAGS = /MTd /ZI /W4 /DBUILDDLL /EHsc /I .\inc /nologo
DLLFLAGS = $(DLLFLAGS) /DUNICODE /D_UNICODE

OBJ_PATH = objs

#----------------------------------------------
#                 build exe
#----------------------------------------------
{src}.cpp{./$(OBJ_PATH)}.obj:
	@$(CPP) $(CPPFLAGS) /c /Fo$(OBJ_PATH)/ $< 

src = src/shell.cpp \
      src/helper.cpp \
      src/getopt.cpp

# Replace the cpp to obj for variable src
objs = $(src:cpp=obj)

# Replace the src/ to ./objs/
objs = $(objs:src/=./objs/)

resource = src/resource.rc

res_objs = ./$(OBJ_PATH)/exe_resuorce.res

target = ./shell.exe

libs = shell32.lib \
       dbghelp.lib \
       user32.lib \
       ole32.lib \
       Shlwapi.lib \
       Version.lib \
       Comdlg32.lib \
       Advapi32.lib \
       Imagehlp.lib \
       Psapi.lib \
       Wininet.lib \

all: $(target)

$(res_objs): $(resource)
	@$(RC) $(RFLAGS) $(res_objs) $(resource)

$(target): $(src) $(objs) $(res_objs)
	@link /DEBUG /NOLOGO /OUT:$@ $(objs) $(res_objs) $(libs)


#----------------------------------------------
#               build my_dll
#----------------------------------------------
{my_dll_src}.cpp{./$(OBJ_PATH)}.obj:
	@$(CPP) $(DLLFLAGS) /c /Fo$(OBJ_PATH)/ $<

dll_src = my_dll_src/my_dll.cpp

# Replace the cpp to obj for variable src
dll_objs = $(dll_src:cpp=obj)

# Replace the src/ to ./objs/
dll_objs = $(dll_objs:my_dll_src/=./objs/)

dll_target = my_dll.dll

dll_libs = user32.lib \
           Shlwapi.lib \
	   Ole32.lib

$(dll_target): $(dll_src) $(dll_objs)
	@link /DLL /NOLOGO /DEF:my_dll_src/my_dll.def /OUT:$@ $(dll_objs) $(dll_libs)
	@del $*.exp

my_dll: $(dll_target)


#----------------------------------------------
#         build shell extenstion dll
#----------------------------------------------
{shell_ext_src}.cpp{./$(OBJ_PATH)}.obj:
	@$(CPP) $(DLLFLAGS) /c /Fo$(OBJ_PATH)/ $<

ext_src = shell_ext_src/shell_ext.cpp \
          src/helper.cpp

ext_resource = shell_ext_src/resource.rc

ext_objs = $(ext_src:cpp=obj)
ext_objs = $(ext_objs:shell_ext_src/=./objs/)
ext_objs = $(ext_objs:src/=./objs/)

ext_target = shell_ext.dll

ext_libs = user32.lib \
           Comctl32.lib \
	   Ole32.lib \
	   Shell32.lib \
	   Shlwapi.lib \
	   Comdlg32.lib \
	   Advapi32.lib

ext_targe = shell_ext.dll
ext_res_objs = ./$(OBJ_PATH)/ext_resource.res

$(ext_res_objs): $(ext_resource)
	@$(RC) $(RFLAGS) $(ext_res_objs) $(ext_resource)

$(ext_targe): $(ext_src) $(ext_objs) $(ext_res_objs)
	@link /DLL /NOLOGO /DEF:shell_ext_src/shell_ext.def /OUT:$@ $(ext_objs) $(ext_res_objs) $(ext_libs)
	@del $*.exp

shell_ext: $(ext_targe)


#----------------------------------------------
#         build browser helper object
#----------------------------------------------
{BHO}.cpp{./$(OBJ_PATH)}.obj:
	@$(CPP) $(DLLFLAGS) /c /Fo$(OBJ_PATH)/ $<

bho_src = BHO/bho.cpp \
          BHO/IUnknownImpl.cpp \
          BHO/com_helper.cpp \
	  BHO/ComPtr.cpp \
          BHO/hook.cpp \
          src/helper.cpp \

http_recorder_src = BHO/http_recorder.cpp \
                    BHO/ComPtr.cpp \
                    src/helper.cpp

bho_resource = BHO/resource.rc

bho_objs = $(bho_src:cpp=obj)
bho_objs = $(bho_objs:BHO/=./objs/)
bho_objs = $(bho_objs:src/=./objs/)

http_recorder_objs = $(http_recorder_src:cpp=obj)
http_recorder_objs = $(http_recorder_objs:BHO/=./objs/)
http_recorder_objs = $(http_recorder_objs:src/=./objs/)

bho_target = bho.dll
http_recorder = http_recorder.exe

bho_libs = user32.lib \
	   Ole32.lib \
	   Shell32.lib \
	   Shlwapi.lib \
	   Comdlg32.lib \
	   Advapi32.lib \
       	   Imagehlp.lib \
	   OleAut32.lib \
	   Wininet.lib


bho_target = bho.dll
bho_res_objs = ./$(OBJ_PATH)/bho_resource.res

$(bho_res_objs): $(bho_resource)
	@$(RC) $(RFLAGS) $(bho_res_objs) $(bho_resource)

$(bho_target): $(bho_src) $(bho_objs) $(bho_res_objs)
	@link /DEBUG /DLL /NOLOGO /DEF:BHO/bho.def /OUT:$@ $(bho_objs) $(bho_res_objs) $(bho_libs)
	@del $*.exp

# Use DEBUG flag will make linker create PDB file for debugging
$(http_recorder): $(http_recorder_src) $(http_recorder_objs)
	@link /DEBUG /NOLOGO /OUT:$@ $(http_recorder_objs) $(bho_libs)

bho: $(bho_target) $(http_recorder)



#----------------------------------------------
#                 clean objects
#----------------------------------------------
clean:
	del /Q objs
	del *.obj *.exe *.res *.lib *.exp *.dll *.manifest *.ilk *.pdb *idb
