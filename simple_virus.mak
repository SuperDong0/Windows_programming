# These build flags means:
# 	/MTd: Use multi-thread debug version static CRT
# 	/W4: Use warning level 4
#	/DUNICODE /D_UNICODE: Use unicode code
#	/EHsc: Enable exception handling
#       /ZI: means create the PDB file for debug
CPPFLAGS = /MTd /ZI /W4 /I .\inc /EHsc /nologo
CPPFLAGS = $(CPPFLAGS) /DUNICODE /D_UNICODE
RFLAGS   = /v /fo

OBJ_PATH = objs

#----------------------------------------------
#          build simple virus demo
#----------------------------------------------
{src}.cpp{./$(OBJ_PATH)}.obj:
	@$(CPP) $(CPPFLAGS) /c /Fo$(OBJ_PATH)/ $< 

{simple_virus}.cpp{./$(OBJ_PATH)}.obj:
	@$(CPP) $(CPPFLAGS) /c /Fo$(OBJ_PATH)/ $< 

src = simple_virus/simple_virus.cpp \
      src/helper.cpp \

# Replace the cpp to obj for variable src
objs = $(src:cpp=obj)

# Replace the source path to ./objs/
objs = $(objs:simple_virus/=./objs/)
objs = $(objs:src/=./objs/)

resource = simple_virus/resource.rc

res_objs = ./$(OBJ_PATH)/simple_virus.res

target = ./SimpleVirusDemo.exe

libs = shell32.lib \
       user32.lib \
       ole32.lib \
       Shlwapi.lib \
       Comdlg32.lib \
       Advapi32.lib \

$(target): $(src) $(objs) $(res_objs)
	@link /DEBUG /NOLOGO /OUT:$@ $(objs) $(res_objs) $(libs)

$(res_objs): $(resource)
	@$(RC) $(RFLAGS) $(res_objs) $(resource)
