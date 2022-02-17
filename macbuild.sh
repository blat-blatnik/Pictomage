#!/bin/bash

# Compile for both x86_64, and for arm64, then merge it into a universal binary.
#  https://developer.apple.com/documentation/apple-silicon/building-a-universal-macos-binary
flags="-O2 -Wno-switch"
frameworks="-framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL"
echo "Compiling..."
clang $flags $frameworks src/*.c -L./lib -lraylib_mac_arm64 -o bin/mac/Pictomage_arm64 -target arm64-apple-macos11
clang $flags $frameworks src/*.c -L./lib -lraylib_mac_x64 -o bin/mac/Pictomage_x64 -target x86_64-apple-macos10.12
lipo -create -output bin/mac/Pictomage_universal bin/mac/Pictomage_x64 bin/mac/Pictomage_arm64
echo "Compilation finished."

# Make the app bundle.
#  https://stackoverflow.com/a/3251285
echo "Creating app bundle..."
rm -r bin/mac/Pictomage.app
mkdir bin/mac/Pictomage.app
mkdir bin/mac/Pictomage.app/Contents
mkdir bin/mac/Pictomage.app/Contents/MacOS
mkdir bin/mac/Pictomage.app/Contents/Resources
cp bin/mac/Pictomage_universal bin/mac/Pictomage.app/Contents/MacOS/Pictomage
cp -a res/. bin/mac/Pictomage.app/Contents/Resources/res
cp Pictomage.icns bin/mac/Pictomage.app/Contents/Resources/Pictomage.icns
cp Info.plist bin/mac/Pictomage.app/Contents
echo "Finished making bundle."

# Make a .dmg installer.
#  https://github.com/raysan5/raylib/wiki/Working-on-macOS
#  https://developer.apple.com/forums/thread/128166
#  https://gist.github.com/jadeatucker/5382343
echo "Creating .dmg installer"
rm bin/mac/Pictomage.dmg
mkdir bin/mac/temp
ln -s /Applications bin/mac/temp/Applications
cp -a bin/mac/Pictomage.app/. bin/mac/temp/Pictomage.app
hdiutil create -size 32m -format UDRW -volname Pictomage -srcfolder bin/mac/temp -o bin/mac/temp/Pictomage.dmg
echo
echo "created bin/mac/temp/Pictomage.dmg."
echo "Mount the .dmg, make it look how you want, then unmount it."
echo "Note: if you don't want to have side panels in the dmg window, make sure to open it from the Desktop icon once mounted, and not from Finder."
echo
read -p "Press enter once you've unmounted the .dmg"
hdiutil convert bin/mac/temp/Pictomage.dmg -format UDZO -o bin/mac/Pictomage.dmg
echo "Created .dmg installer"

# Cleanup
rm -r bin/mac/temp