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
#            build multi desktops
#----------------------------------------------
{src}.cpp{./$(OBJ_PATH)}.obj:
	@$(CPP) $(CPPFLAGS) /c /Fo$(OBJ_PATH)/ $< 

{multi_desktops}.cpp{./$(OBJ_PATH)}.obj:
	@$(CPP) $(CPPFLAGS) /c /Fo$(OBJ_PATH)/ $< 

src = multi_desktops/tray_icon.cpp \
      multi_desktops/multi_desktops_dialog.cpp \
      src/helper.cpp \

# Replace the cpp to obj for variable src
objs = $(src:cpp=obj)

# Replace the source path to ./objs/
objs = $(objs:src/=./objs/)
objs = $(objs:multi_desktops/=./objs/)

resource = multi_desktops/resource.rc

res_objs = ./$(OBJ_PATH)/resuorce.res

target = ./MultiDesktops.exe

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

$(target): $(src) $(objs) $(res_objs)
	@link /DEBUG /NOLOGO /OUT:$@ $(objs) $(res_objs) $(libs)

$(res_objs): $(resource)
	@$(RC) $(RFLAGS) $(res_objs) $(resource)
