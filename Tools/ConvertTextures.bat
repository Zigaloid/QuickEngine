
for /f "delims=" %%F in ('dir Textures\*.jpg /a-d /b') do (
	"C:\Program Files\ImageMagick-7.1.2-Q16-HDRI\magick.exe" .\Textures\%%F .\Textures\%%F.dds
	
)
rename textures\*.jpg.dds *.dds

copy textures\*.dds ..\assets\textures /y
erase %2.dds 


