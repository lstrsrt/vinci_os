if not exist E:\EFI\BOOT (
    exit
)

echo Copying %1 ...
copy /V /Y %1 E:\EFI\BOOT