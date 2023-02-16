protoc.exe --cpp_out=dllexport_decl=PROTO_API:./ *.proto

echo off
for %%b in (*.h) do (
echo %%b
sed -i "6a #include <proto.hpp>" %%b
)

pause
