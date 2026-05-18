erase /q temp 

md temp

tar -xf %1 -C .\temp\

for /f "delims=" %%F in ('dir temp\*.obj /a-d /b') do (
    .\Deployed\FbxToBgfxMesh.exe -i temp\%%F -o %2.mesh.bin -n
)

for /f "delims=" %%F in ('dir temp\*.jpg /a-d /b') do (
	"C:\Program Files\ImageMagick-7.1.2-Q16-HDRI\magick.exe" .\temp\%%F %2.dds
	
)

copy %2.dds ..\assets\textures /y
copy %2.mesh.bin ..\assets\meshes /y

erase %2.dds 
erase %2.mesh.bin 
erase /q temp 
rd temp


